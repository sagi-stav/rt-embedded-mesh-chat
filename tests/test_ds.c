#include "test_framework.h"
#include "HashMap.h"
#include "genqueue.h"
#include "gen_dlist.h"
#include <string.h>
#include <stdlib.h>

/* ── hash/equal helpers for string-keyed HashMaps ─────────────────────────── */

static size_t hash_str(void *key) {
    const char *s = (const char *)key;
    size_t h = 5381;
    int c;
    while ((c = (unsigned char)*s++)) h = ((h << 5) + h) ^ (size_t)c;
    return h;
}

static int equal_str(void *a, void *b) {
    return strcmp((const char *)a, (const char *)b) == 0;
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  HashMap tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_hashmap_create_destroy(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_NOT_NULL(m);
    HashMap_Destroy(&m, NULL, NULL);
    ASSERT_NULL(m);
}

static void test_hashmap_insert_find(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    int val = 42;
    ASSERT_EQ(HashMap_Insert(m, "key", &val), MAP_SUCCESS);
    void *found = NULL;
    ASSERT_EQ(HashMap_Find(m, "key", &found), MAP_SUCCESS);
    ASSERT_EQ(*(int *)found, 42);
    HashMap_Destroy(&m, NULL, NULL);
}

static void test_hashmap_duplicate_key(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    int v1 = 1, v2 = 2;
    HashMap_Insert(m, "dup", &v1);
    ASSERT_EQ(HashMap_Insert(m, "dup", &v2), MAP_KEY_DUPLICATE_ERROR);
    HashMap_Destroy(&m, NULL, NULL);
}

static void test_hashmap_find_not_found(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    void *found = NULL;
    ASSERT_EQ(HashMap_Find(m, "missing", &found), MAP_KEY_NOT_FOUND_ERROR);
    HashMap_Destroy(&m, NULL, NULL);
}

static void test_hashmap_remove(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    int val = 99;
    HashMap_Insert(m, "toremove", &val);
    void *pkey = NULL, *pval = NULL;
    ASSERT_EQ(HashMap_Remove(m, "toremove", &pkey, &pval), MAP_SUCCESS);
    ASSERT_EQ(*(int *)pval, 99);
    void *found = NULL;
    ASSERT_EQ(HashMap_Find(m, "toremove", &found), MAP_KEY_NOT_FOUND_ERROR);
    HashMap_Destroy(&m, NULL, NULL);
}

static void test_hashmap_size(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    int v[3] = {1, 2, 3};
    HashMap_Insert(m, "a", &v[0]);
    HashMap_Insert(m, "b", &v[1]);
    HashMap_Insert(m, "c", &v[2]);
    ASSERT_EQ(HashMap_Size(m), 3);
    HashMap_Destroy(&m, NULL, NULL);
}

static int count_action(const void *key, void *val, void *ctx) {
    (void)key; (void)val;
    (*(int *)ctx)++;
    return 1;
}

static void test_hashmap_foreach(void) {
    HashMap *m = HashMap_Create(16, hash_str, equal_str);
    ASSERT_FATAL(m);
    int v[4] = {1, 2, 3, 4};
    HashMap_Insert(m, "a", &v[0]);
    HashMap_Insert(m, "b", &v[1]);
    HashMap_Insert(m, "c", &v[2]);
    HashMap_Insert(m, "d", &v[3]);
    int count = 0;
    HashMap_ForEach(m, count_action, &count);
    ASSERT_EQ(count, 4);
    HashMap_Destroy(&m, NULL, NULL);
}

static void test_hashmap_multiple_keys(void) {
    HashMap *m = HashMap_Create(8, hash_str, equal_str);
    ASSERT_FATAL(m);
    int vals[5] = {10, 20, 30, 40, 50};
    const char *keys[5] = {"alpha", "beta", "gamma", "delta", "epsilon"};
    for (int i = 0; i < 5; i++)
        ASSERT_EQ(HashMap_Insert(m, keys[i], &vals[i]), MAP_SUCCESS);
    for (int i = 0; i < 5; i++) {
        void *found = NULL;
        ASSERT_EQ(HashMap_Find(m, keys[i], &found), MAP_SUCCESS);
        ASSERT_EQ(*(int *)found, vals[i]);
    }
    HashMap_Destroy(&m, NULL, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  Queue tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_queue_create_destroy(void) {
    Queue *q = QueueCreate(8);
    ASSERT_NOT_NULL(q);
    QueueDestroy(&q, NULL);
    ASSERT_NULL(q);
}

static void test_queue_fifo_order(void) {
    Queue *q = QueueCreate(4);
    ASSERT_FATAL(q);
    int a = 1, b = 2, c = 3;
    QueueInsert(q, &a);
    QueueInsert(q, &b);
    QueueInsert(q, &c);
    void *out;
    QueueRemove(q, &out); ASSERT_EQ(*(int *)out, 1);
    QueueRemove(q, &out); ASSERT_EQ(*(int *)out, 2);
    QueueRemove(q, &out); ASSERT_EQ(*(int *)out, 3);
    QueueDestroy(&q, NULL);
}

static void test_queue_is_empty(void) {
    Queue *q = QueueCreate(4);
    ASSERT_FATAL(q);
    ASSERT(QueueIsEmpty(q));
    int v = 1;
    QueueInsert(q, &v);
    ASSERT(!QueueIsEmpty(q));
    void *out;
    QueueRemove(q, &out);
    ASSERT(QueueIsEmpty(q));
    QueueDestroy(&q, NULL);
}

static void test_queue_overflow(void) {
    Queue *q = QueueCreate(2);
    ASSERT_FATAL(q);
    int a = 1, b = 2, c = 3;
    QueueInsert(q, &a);
    QueueInsert(q, &b);
    ASSERT_EQ(QueueInsert(q, &c), QUEUE_OVERFLOW_ERROR);
    QueueDestroy(&q, NULL);
}

static void test_queue_remove_empty(void) {
    Queue *q = QueueCreate(4);
    ASSERT_FATAL(q);
    void *out;
    ASSERT_EQ(QueueRemove(q, &out), QUEUE_DATA_NOT_FOUND_ERROR);
    QueueDestroy(&q, NULL);
}

static void test_queue_fill_and_drain(void) {
    Queue *q = QueueCreate(5);
    ASSERT_FATAL(q);
    int vals[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++)
        ASSERT_EQ(QueueInsert(q, &vals[i]), QUEUE_SUCCESS);
    void *out;
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(QueueRemove(q, &out), QUEUE_SUCCESS);
        ASSERT_EQ(*(int *)out, vals[i]);
    }
    ASSERT(QueueIsEmpty(q));
    QueueDestroy(&q, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  List tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_list_create_destroy(void) {
    List *l = ListCreate();
    ASSERT_NOT_NULL(l);
    ListDestroy(&l, NULL);
    ASSERT_NULL(l);
}

static void test_list_push_pop_head(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 1, b = 2;
    ListPushHead(l, &a);
    ListPushHead(l, &b);
    ASSERT_EQ(*(int *)ListPopHead(l), 2); /* LIFO */
    ASSERT_EQ(*(int *)ListPopHead(l), 1);
    ListDestroy(&l, NULL);
}

static void test_list_push_tail_fifo(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 1, b = 2, c = 3;
    ListPushTail(l, &a);
    ListPushTail(l, &b);
    ListPushTail(l, &c);
    ASSERT_EQ(*(int *)ListPopHead(l), 1); /* FIFO */
    ASSERT_EQ(*(int *)ListPopHead(l), 2);
    ASSERT_EQ(*(int *)ListPopHead(l), 3);
    ListDestroy(&l, NULL);
}

static void test_list_size(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    ASSERT_EQ(ListSize(l), 0);
    int v = 1;
    ListPushTail(l, &v);
    ListPushTail(l, &v);
    ASSERT_EQ(ListSize(l), 2);
    ListDestroy(&l, NULL);
}

static void test_list_is_empty(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    ASSERT(ListIsEmpty(l));
    int v = 1;
    ListPushTail(l, &v);
    ASSERT(!ListIsEmpty(l));
    ListDestroy(&l, NULL);
}

static int sum_action(void *elem, void *ctx) {
    *(int *)ctx += *(int *)elem;
    return 1;
}

static void test_list_foreach_sum(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 10, b = 20, c = 30;
    ListPushTail(l, &a);
    ListPushTail(l, &b);
    ListPushTail(l, &c);
    int sum = 0;
    ListItrForEach(ListItrBegin(l), ListItrEnd(l), sum_action, &sum);
    ASSERT_EQ(sum, 60);
    ListDestroy(&l, NULL);
}

static int stop_at_20(void *elem, void *ctx) {
    (void)ctx;
    return *(int *)elem != 20; /* return 0 (stop) when element is 20 */
}

static void test_list_foreach_stops(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 10, b = 20, c = 30;
    ListPushTail(l, &a);
    ListPushTail(l, &b);
    ListPushTail(l, &c);
    ListItr end     = ListItrEnd(l);
    ListItr stopped = ListItrForEach(ListItrBegin(l), end, stop_at_20, NULL);
    ASSERT_NEQ(stopped, end);
    ASSERT_EQ(*(int *)ListItrGet(stopped), 20);
    ListDestroy(&l, NULL);
}

static void test_list_iterator_remove_middle(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 1, b = 2, c = 3;
    ListPushTail(l, &a);
    ListPushTail(l, &b);
    ListPushTail(l, &c);
    ListItr itr = ListItrNext(ListItrBegin(l)); /* points at b (middle) */
    ListItrRemove(itr);
    ASSERT_EQ(ListSize(l), 2);
    ASSERT_EQ(*(int *)ListPopHead(l), 1);
    ASSERT_EQ(*(int *)ListPopHead(l), 3);
    ListDestroy(&l, NULL);
}

static void test_list_pop_tail(void) {
    List *l = ListCreate();
    ASSERT_FATAL(l);
    int a = 1, b = 2, c = 3;
    ListPushTail(l, &a);
    ListPushTail(l, &b);
    ListPushTail(l, &c);
    ASSERT_EQ(*(int *)ListPopTail(l), 3);
    ASSERT_EQ(*(int *)ListPopTail(l), 2);
    ASSERT_EQ(ListSize(l), 1);
    ListDestroy(&l, NULL);
}

/* ── suite entry point ─────────────────────────────────────────────────────── */

void run_ds_tests(void) {
    TEST_SUITE("HashMap");
    RUN_TEST(test_hashmap_create_destroy);
    RUN_TEST(test_hashmap_insert_find);
    RUN_TEST(test_hashmap_duplicate_key);
    RUN_TEST(test_hashmap_find_not_found);
    RUN_TEST(test_hashmap_remove);
    RUN_TEST(test_hashmap_size);
    RUN_TEST(test_hashmap_foreach);
    RUN_TEST(test_hashmap_multiple_keys);

    TEST_SUITE("Queue");
    RUN_TEST(test_queue_create_destroy);
    RUN_TEST(test_queue_fifo_order);
    RUN_TEST(test_queue_is_empty);
    RUN_TEST(test_queue_overflow);
    RUN_TEST(test_queue_remove_empty);
    RUN_TEST(test_queue_fill_and_drain);

    TEST_SUITE("List");
    RUN_TEST(test_list_create_destroy);
    RUN_TEST(test_list_push_pop_head);
    RUN_TEST(test_list_push_tail_fifo);
    RUN_TEST(test_list_size);
    RUN_TEST(test_list_is_empty);
    RUN_TEST(test_list_foreach_sum);
    RUN_TEST(test_list_foreach_stops);
    RUN_TEST(test_list_iterator_remove_middle);
    RUN_TEST(test_list_pop_tail);
}
