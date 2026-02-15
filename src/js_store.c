#include "js_main.h"

static unsigned int js_store_hash(const char *key) {
    unsigned int h = 5381;
    while (*key)
        h = h * 33 + (unsigned char)*key++;
    return h;
}

int js_store_init(js_store_t *store, int bucket_count) {
    store->bucket_count = bucket_count;
    store->buckets = calloc(bucket_count, sizeof(js_store_entry_t *));
    if (!store->buckets)
        return -1;
    pthread_mutex_init(&store->lock, NULL);
    return 0;
}

static js_store_entry_t *js_store_find(js_store_t *store, const char *key,
                                       unsigned int *idx) {
    *idx = js_store_hash(key) % store->bucket_count;
    js_store_entry_t *e = store->buckets[*idx];
    while (e) {
        if (strcmp(e->key, key) == 0)
            return e;
        e = e->next;
    }
    return NULL;
}

char *js_store_get(js_store_t *store, const char *key) {
    pthread_mutex_lock(&store->lock);
    unsigned int idx;
    js_store_entry_t *e = js_store_find(store, key, &idx);
    char *val = e ? strdup(e->value) : NULL;
    pthread_mutex_unlock(&store->lock);
    return val;
}

int js_store_set(js_store_t *store, const char *key, const char *value) {
    pthread_mutex_lock(&store->lock);
    unsigned int idx;
    js_store_entry_t *e = js_store_find(store, key, &idx);
    if (e) {
        free(e->value);
        e->value = strdup(value);
    } else {
        e = malloc(sizeof(*e));
        if (!e) {
            pthread_mutex_unlock(&store->lock);
            return -1;
        }
        e->key = strdup(key);
        e->value = strdup(value);
        e->next = store->buckets[idx];
        store->buckets[idx] = e;
    }
    pthread_mutex_unlock(&store->lock);
    return 0;
}

int js_store_del(js_store_t *store, const char *key) {
    pthread_mutex_lock(&store->lock);
    unsigned int idx = js_store_hash(key) % store->bucket_count;
    js_store_entry_t **pp = &store->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            js_store_entry_t *e = *pp;
            *pp = e->next;
            free(e->key);
            free(e->value);
            free(e);
            pthread_mutex_unlock(&store->lock);
            return 0;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&store->lock);
    return -1;
}

int js_store_incr(js_store_t *store, const char *key) {
    pthread_mutex_lock(&store->lock);
    unsigned int idx;
    js_store_entry_t *e = js_store_find(store, key, &idx);
    int val;
    if (e) {
        val = atoi(e->value) + 1;
        free(e->value);
    } else {
        val = 1;
        e = malloc(sizeof(*e));
        if (!e) {
            pthread_mutex_unlock(&store->lock);
            return -1;
        }
        e->key = strdup(key);
        e->value = NULL;
        e->next = store->buckets[idx];
        store->buckets[idx] = e;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    e->value = strdup(buf);
    pthread_mutex_unlock(&store->lock);
    return val;
}

void js_store_clear(js_store_t *store) {
    pthread_mutex_lock(&store->lock);
    for (int i = 0; i < store->bucket_count; i++) {
        js_store_entry_t *e = store->buckets[i];
        while (e) {
            js_store_entry_t *next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
        store->buckets[i] = NULL;
    }
    pthread_mutex_unlock(&store->lock);
}

void js_store_free(js_store_t *store) {
    js_store_clear(store);
    free(store->buckets);
    store->buckets = NULL;
    pthread_mutex_destroy(&store->lock);
}
