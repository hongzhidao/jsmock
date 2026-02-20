#include "js_main.h"

/* ---- conn event handlers ---- */

static void js_http_on_read(js_event_t *ev) {
    js_engine_t *eng = &js_thread_current->engine;
    js_conn_t *conn = js_event_data(ev, js_conn_t, event);

    int rc = js_conn_read(conn);
    if (rc <= 0) {
        js_conn_close(conn, &eng->epoll);
        js_conn_free(conn);
        return;
    }

    /* try to parse a complete HTTP request */
    js_http_request_t req = {0};
    int parsed = js_http_parse_request(&conn->rbuf, &req);
    if (parsed < 0) {
        js_conn_close(conn, &eng->epoll);
        js_conn_free(conn);
        return;
    }
    if (parsed == 0)
        return; /* need more data */

    /* execute JS handler */
    js_http_response_t resp = {0};
    js_runtime_t *rt = js_thread_current->rt;
    int handle_rc = js_qjs_handle_request(rt, &req, &resp, conn);
    js_http_request_free(&req);

    if (handle_rc < 0) {
        resp.status = 500;
        resp.body = strdup("Internal Server Error");
        resp.body_len = strlen(resp.body);
    } else if (handle_rc == 1) {
        /* async: handler will complete later via timer callback */
        conn->state = JS_CONN_PENDING;
        js_epoll_del(&eng->epoll, ev->fd);
        return;
    }

    /* sync path: serialize response into write buffer */
    js_http_serialize_response(&resp, &conn->wbuf);
    js_http_response_free(&resp);

    conn->state = JS_CONN_WRITING;
    js_epoll_mod(&eng->epoll, ev->fd, EPOLLOUT, ev);
}

static void js_http_on_write(js_event_t *ev) {
    js_engine_t *eng = &js_thread_current->engine;
    js_conn_t *conn = js_event_data(ev, js_conn_t, event);

    int rc = js_conn_write(conn);
    if (rc < 0) {
        js_conn_close(conn, &eng->epoll);
        js_conn_free(conn);
        return;
    }
    if (rc == 1) {
        /* fully written, close connection (no keep-alive for now) */
        js_conn_close(conn, &eng->epoll);
        js_conn_free(conn);
    }
}

void js_http_conn_init(js_conn_t *conn) {
    js_engine_t *eng = &js_thread_current->engine;
    conn->event.read  = js_http_on_read;
    conn->event.write = js_http_on_write;
    js_epoll_add(&eng->epoll, conn->event.fd, EPOLLIN, &conn->event);
}

/* ---- http ---- */

js_http_method_t js_http_method_from_str(const char *str, int len) {
    if (len == 3 && strncmp(str, "GET", 3) == 0) return JS_HTTP_GET;
    if (len == 4 && strncmp(str, "POST", 4) == 0) return JS_HTTP_POST;
    if (len == 3 && strncmp(str, "PUT", 3) == 0) return JS_HTTP_PUT;
    if (len == 5 && strncmp(str, "PATCH", 5) == 0) return JS_HTTP_PATCH;
    if (len == 6 && strncmp(str, "DELETE", 6) == 0) return JS_HTTP_DELETE;
    return JS_HTTP_ALL;
}

const char *js_http_status_text(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default:  return "Unknown";
    }
}

/*
 * Parse HTTP/1.1 request from buffer.
 * Returns: 1 = complete request parsed, 0 = need more data, -1 = error.
 */
int js_http_parse_request(js_buf_t *buf, js_http_request_t *req) {
    /* find end of headers */
    char *end = memmem(buf->data, buf->len, "\r\n\r\n", 4);
    if (!end)
        return 0; /* incomplete */

    char *p = buf->data;

    /* request line: METHOD PATH HTTP/1.1\r\n */
    char *sp1 = memchr(p, ' ', end - p);
    if (!sp1) return -1;
    req->method = js_http_method_from_str(p, sp1 - p);

    char *sp2 = memchr(sp1 + 1, ' ', end - sp1 - 1);
    if (!sp2) return -1;

    /* split path and query */
    char *q = memchr(sp1 + 1, '?', sp2 - sp1 - 1);
    if (q) {
        req->path = strndup(sp1 + 1, q - sp1 - 1);
        req->query = strndup(q + 1, sp2 - q - 1);
    } else {
        req->path = strndup(sp1 + 1, sp2 - sp1 - 1);
        req->query = NULL;
    }

    /* skip past request line */
    char *line = memchr(sp2, '\n', end - sp2);
    if (!line) return -1;
    line++;

    /* parse headers — end points to first \r of \r\n\r\n, so include +2
       to cover the last header's \r\n which starts at end itself */
    req->headers = NULL;
    req->header_count = 0;
    req->content_length = -1;

    char *header_end = end + 2;
    while (line < header_end) {
        char *crlf = memmem(line, header_end - line, "\r\n", 2);
        if (!crlf || crlf == line)
            break;

        char *colon = memchr(line, ':', crlf - line);
        if (!colon) {
            line = crlf + 2;
            continue;
        }

        /* grow headers array */
        req->headers = realloc(req->headers,
                               sizeof(js_header_t) * (req->header_count + 1));
        js_header_t *h = &req->headers[req->header_count++];
        h->name = strndup(line, colon - line);

        /* skip ": " */
        colon++;
        while (colon < crlf && *colon == ' ')
            colon++;
        h->value = strndup(colon, crlf - colon);

        /* check Content-Length */
        if (strcasecmp(h->name, "Content-Length") == 0)
            req->content_length = atoi(h->value);

        line = crlf + 2;
    }

    /* body */
    size_t header_size = (end + 4) - buf->data;
    if (req->content_length > 0) {
        size_t total = header_size + req->content_length;
        if (buf->len < total)
            return 0; /* need more body data */
        req->body = strndup(end + 4, req->content_length);
        req->body_len = req->content_length;
        js_buf_consume(buf, total);
    } else {
        req->body = NULL;
        req->body_len = 0;
        js_buf_consume(buf, header_size);
    }

    return 1;
}

int js_http_serialize_response(js_http_response_t *resp, js_buf_t *out) {
    char line[256];
    int n;

    /* status line */
    n = snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\n",
                 resp->status, js_http_status_text(resp->status));
    if (js_buf_append(out, line, n) < 0)
        return -1;

    /* headers */
    for (int i = 0; i < resp->header_count; i++) {
        n = snprintf(line, sizeof(line), "%s: %s\r\n",
                     resp->headers[i].name, resp->headers[i].value);
        if (js_buf_append(out, line, n) < 0)
            return -1;
    }

    /* Content-Length if body present */
    if (resp->body && resp->body_len > 0) {
        n = snprintf(line, sizeof(line), "Content-Length: %zu\r\n",
                     resp->body_len);
        if (js_buf_append(out, line, n) < 0)
            return -1;
    }

    /* end of headers */
    if (js_buf_append(out, "\r\n", 2) < 0)
        return -1;

    /* body */
    if (resp->body && resp->body_len > 0) {
        if (js_buf_append(out, resp->body, resp->body_len) < 0)
            return -1;
    }

    return 0;
}

void js_http_request_free(js_http_request_t *req) {
    free(req->path);
    free(req->query);
    for (int i = 0; i < req->header_count; i++) {
        free(req->headers[i].name);
        free(req->headers[i].value);
    }
    free(req->headers);
    free(req->body);
    memset(req, 0, sizeof(*req));
}

void js_http_response_free(js_http_response_t *resp) {
    for (int i = 0; i < resp->header_count; i++) {
        free(resp->headers[i].name);
        free(resp->headers[i].value);
    }
    free(resp->headers);
    free(resp->body);
    memset(resp, 0, sizeof(*resp));
}
