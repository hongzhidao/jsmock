#ifndef JS_STORE_H
#define JS_STORE_H

/* ---- struct ---- */

typedef struct js_store_entry_s {
    char                    *key;
    char                    *value;   /* JSON string */
    struct js_store_entry_s *next;
} js_store_entry_t;

typedef struct {
    js_store_entry_t **buckets;
    int                bucket_count;
    pthread_mutex_t    lock;         /* thread-safe access */
} js_store_t;

/* ---- api ---- */

int   js_store_init(js_store_t *store, int bucket_count);
char *js_store_get(js_store_t *store, const char *key);
int   js_store_set(js_store_t *store, const char *key, const char *value);
int   js_store_del(js_store_t *store, const char *key);
int   js_store_incr(js_store_t *store, const char *key);
void  js_store_clear(js_store_t *store);
void  js_store_free(js_store_t *store);

#endif
