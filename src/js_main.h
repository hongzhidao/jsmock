#ifndef JS_MAIN_H
#define JS_MAIN_H

/* ---- system headers ---- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ---- quickjs ---- */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "quickjs.h"
#pragma GCC diagnostic pop

/* ---- module headers (dependency order) ---- */
#include "js_epoll.h"
#include "js_timer.h"
#include "js_engine.h"
#include "js_thread.h"
#include "js_conn.h"
#include "js_http.h"
#include "js_route.h"
#include "js_store.h"
#include "js_qjs.h"
#include "js_web.h"
#include "js_tls.h"
#include "js_runtime.h"

#endif
