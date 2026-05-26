#include "HashMap.h"
#include "gen_dlist.h"

#include <stdlib.h>  /* malloc, free  */
#include <stddef.h>  /* size_t        */

/* ===========================================================================
 * Internal constants
 * =========================================================================== */

#define MIN_CAPACITY 4

/* ===========================================================================
 * Internal types
 * =========================================================================== */

/*
 * KeyValuePair - internal node stored inside each bucket's List.
 * Hidden from the user; the user only sees HashMap*.
 */
typedef struct {
    const void* key;
    const void* value;
} KeyValuePair;

/*
 * HashMap - the opaque struct whose definition is hidden from the user.
 * The header exposes only: typedef struct HashMap HashMap;
 */
struct HashMap {
    List**           buckets;       /* array of List*, one per bucket   */
    size_t           capacity;      /* number of buckets (always prime) */
    size_t           size;          /* total key-value pairs stored     */
    HashFunction     hashFunc;
    EqualityFunction keysEqualFunc;
};

/* ===========================================================================
 * Context structs passed into ListItrForEach callbacks
 * =========================================================================== */

/* Used by HashMap_Insert and HashMap_Find */
typedef struct {
    const void*      searchKey;
    EqualityFunction keysEqualFunc;
    KeyValuePair*    foundPair;     /* NULL until a match is found      */
} SearchContext;

/* Used by HashMap_ForEach */
typedef struct {
    KeyValueActionFunction userAction;
    void*                  userContext;
    size_t                 invokeCount;
    int                    stopped;     /* 1 if user action returned 0  */
} ForEachContext;

/* ===========================================================================
 * Forward declarations — static helpers
 * =========================================================================== */

static int    IsPrime      (size_t _n);
static size_t NextPrime    (size_t _n);
static size_t BucketIndex  (const HashMap* _map, const void* _key);
static int    FindPairAction(void* _element, void* _context);
static int    ForEachAction (void* _element, void* _context);

/* ===========================================================================
 * API — HashMap_Create
 * =========================================================================== */

HashMap* HashMap_Create(size_t _capacity, HashFunction _hashFunc, EqualityFunction _keysEqualFunc)
{
    HashMap* map;
    size_t   primeCapacity;

    if (NULL == _hashFunc || NULL == _keysEqualFunc)
    {
        return NULL;
    }

    primeCapacity = NextPrime((_capacity < MIN_CAPACITY) ? MIN_CAPACITY : _capacity);

    map = (HashMap*)malloc(sizeof(HashMap));
    if (NULL == map)
    {
        return NULL;
    }

    /* calloc zeroes every slot — all buckets start as NULL (lazy allocation) */
    map->buckets = (List**)calloc(primeCapacity, sizeof(List*));
    if (NULL == map->buckets)
    {
        free(map);
        return NULL;
    }

    map->capacity      = primeCapacity;
    map->size          = 0;
    map->hashFunc      = _hashFunc;
    map->keysEqualFunc = _keysEqualFunc;

    return map;
}

/* ===========================================================================
 * API — HashMap_Destroy
 * =========================================================================== */

void HashMap_Destroy(HashMap** _map, void (*_keyDestroy)(void* _key), void (*_valDestroy)(void* _value))
{
    size_t        i;
    KeyValuePair* pair;

    if (NULL == _map || NULL == *_map)
    {
        return;
    }

    for (i = 0; i < (*_map)->capacity; i++)
    {
        if (NULL == (*_map)->buckets[i])
        {
            continue;
        }

        /*
         * Manually pop every pair so we can call the user's destroy
         * functions on key and value before freeing the pair itself.
         * ListPopHead is O(1) and safe to call until the list is empty.
         */
        while (!ListIsEmpty((*_map)->buckets[i]))
        {
            pair = (KeyValuePair*)ListPopHead((*_map)->buckets[i]);
            if (NULL == pair)
            {
                break;
            }

            if (NULL != _keyDestroy) { _keyDestroy((void*)pair->key);  }
            if (NULL != _valDestroy) { _valDestroy((void*)pair->value); }
            free(pair);
        }

        /* Destroy the now-empty list shell */
        ListDestroy(&(*_map)->buckets[i], NULL);
    }

    free((*_map)->buckets);
    free(*_map);
    *_map = NULL;
}

/* ===========================================================================
 * API — HashMap_Insert
 * =========================================================================== */

Map_Result HashMap_Insert(HashMap* _map, const void* _key, const void* _value)
{
    size_t        idx;
    KeyValuePair* pair;
    SearchContext ctx;

    if (NULL == _map) { return MAP_UNINITIALIZED_ERROR; }
    if (NULL == _key) { return MAP_KEY_NULL_ERROR;      }

    idx = BucketIndex(_map, _key);

    /* Lazy-allocate the bucket list on first use */
    if (NULL == _map->buckets[idx])
    {
        _map->buckets[idx] = ListCreate();
        if (NULL == _map->buckets[idx])
        {
            return MAP_ALLOCATION_ERROR;
        }
    }

    /* Reject duplicate keys */
    ctx.searchKey     = _key;
    ctx.keysEqualFunc = _map->keysEqualFunc;
    ctx.foundPair     = NULL;

    ListItrForEach(ListItrBegin(_map->buckets[idx]),
                   ListItrEnd  (_map->buckets[idx]),
                   FindPairAction,
                   &ctx);

    if (NULL != ctx.foundPair)
    {
        return MAP_KEY_DUPLICATE_ERROR;
    }

    pair = (KeyValuePair*)malloc(sizeof(KeyValuePair));
    if (NULL == pair)
    {
        return MAP_ALLOCATION_ERROR;
    }

    pair->key   = _key;
    pair->value = _value;

    if (NULL == ListPushHead(_map->buckets[idx], pair))
    {
        free(pair);
        return MAP_ALLOCATION_ERROR;
    }

    _map->size++;
    return MAP_SUCCESS;
}

/* ===========================================================================
 * API — HashMap_Remove
 *
 * We iterate manually (not via ListItrForEach) so we can hold onto the
 * iterator and pass it directly to ListItrRemove after the match is found.
 * A manual loop is explicit and unambiguous.
 * =========================================================================== */

Map_Result HashMap_Remove(HashMap* _map, const void* _searchKey, void** _pKey, void** _pValue)
{
    size_t        idx;
    ListItr       itr;
    ListItr       end;
    KeyValuePair* pair;

    if (NULL == _map)       { return MAP_UNINITIALIZED_ERROR; }
    if (NULL == _searchKey) { return MAP_KEY_NULL_ERROR;      }

    idx = BucketIndex(_map, _searchKey);

    if (NULL == _map->buckets[idx])
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    itr = ListItrBegin(_map->buckets[idx]);
    end = ListItrEnd  (_map->buckets[idx]);

    while (itr != end)
    {
        pair = (KeyValuePair*)ListItrGet(itr);

        if (_map->keysEqualFunc((void*)pair->key, (void*)_searchKey))
        {
            if (NULL != _pKey)   { *_pKey   = (void*)pair->key;   }
            if (NULL != _pValue) { *_pValue = (void*)pair->value;  }

            ListItrRemove(itr);
            free(pair);

            _map->size--;
            return MAP_SUCCESS;
        }

        itr = ListItrNext(itr);
    }

    return MAP_KEY_NOT_FOUND_ERROR;
}

/* ===========================================================================
 * API — HashMap_Find
 * =========================================================================== */

Map_Result HashMap_Find(const HashMap* _map, const void* _key, void** _pValue)
{
    size_t        idx;
    SearchContext ctx;

    if (NULL == _map) { return MAP_UNINITIALIZED_ERROR; }
    if (NULL == _key) { return MAP_KEY_NULL_ERROR;      }

    idx = BucketIndex(_map, _key);

    if (NULL == _map->buckets[idx])
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    ctx.searchKey     = _key;
    ctx.keysEqualFunc = _map->keysEqualFunc;
    ctx.foundPair     = NULL;

    ListItrForEach(ListItrBegin(_map->buckets[idx]),
                   ListItrEnd  (_map->buckets[idx]),
                   FindPairAction,
                   &ctx);

    if (NULL == ctx.foundPair)
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    if (NULL != _pValue)
    {
        *_pValue = (void*)ctx.foundPair->value;
    }

    return MAP_SUCCESS;
}

/* ===========================================================================
 * API — HashMap_Size
 * =========================================================================== */

size_t HashMap_Size(const HashMap* _map)
{
    if (NULL == _map)
    {
        return 0;
    }
    return _map->size;
}

/* ===========================================================================
 * API — HashMap_ForEach
 * =========================================================================== */

size_t HashMap_ForEach(const HashMap* _map, KeyValueActionFunction _action, void* _context)
{
    size_t         i;
    ForEachContext ctx;

    if (NULL == _map || NULL == _action)
    {
        return 0;
    }

    ctx.userAction  = _action;
    ctx.userContext = _context;
    ctx.invokeCount = 0;
    ctx.stopped     = 0;

    for (i = 0; i < _map->capacity; i++)
    {
        if (NULL == _map->buckets[i])
        {
            continue;
        }

        ListItrForEach(ListItrBegin(_map->buckets[i]),
                       ListItrEnd  (_map->buckets[i]),
                       ForEachAction,
                       &ctx);

        /* User action returned 0 — stop iterating over remaining buckets */
        if (ctx.stopped)
        {
            break;
        }
    }

    return ctx.invokeCount;
}

/* ===========================================================================
 * API — HashMap_GetStatistics  (debug build only)
 * =========================================================================== */

#ifndef NDEBUG

Map_Stats HashMap_GetStatistics(const HashMap* _map)
{
    Map_Stats stats = {0, 0, 0, 0};
    size_t    i;
    size_t    chainLen;
    size_t    totalChainLen = 0;

    if (NULL == _map)
    {
        return stats;
    }

    stats.numberOfBuckets = _map->capacity;

    for (i = 0; i < _map->capacity; i++)
    {
        if (NULL == _map->buckets[i])
        {
            continue;
        }

        chainLen = ListSize(_map->buckets[i]);
        if (0 == chainLen)
        {
            continue;
        }

        stats.numberOfChains++;
        totalChainLen += chainLen;

        if (chainLen > stats.maxChainLength)
        {
            stats.maxChainLength = chainLen;
        }
    }

    stats.averageChainLength = (stats.numberOfChains > 0)
                                ? (totalChainLen / stats.numberOfChains)
                                : 0;
    return stats;
}

#endif /* NDEBUG */

/* ===========================================================================
 * Helper — IsPrime
 * =========================================================================== */

static int IsPrime(size_t _n)
{
    size_t i;

    if (_n < 2)      { return 0; }
    if (_n == 2)     { return 1; }
    if (_n % 2 == 0) { return 0; }

    for (i = 3; i * i <= _n; i += 2)
    {
        if (_n % i == 0) { return 0; }
    }
    return 1;
}

/* ===========================================================================
 * Helper — NextPrime
 * Returns the smallest prime number >= _n.
 * =========================================================================== */

static size_t NextPrime(size_t _n)
{
    if (_n < 2) { _n = 2; }

    while (!IsPrime(_n))
    {
        _n++;
    }
    return _n;
}

/* ===========================================================================
 * Helper — BucketIndex
 * Maps a key to its bucket index using the user's hash function.
 * =========================================================================== */

static size_t BucketIndex(const HashMap* _map, const void* _key)
{
    return _map->hashFunc((void*)_key) % _map->capacity;
}

/* ===========================================================================
 * Helper — FindPairAction  (ListItrForEach callback)
 * Returns 0 (stop) when the key matches, 1 (continue) otherwise.
 * =========================================================================== */

static int FindPairAction(void* _element, void* _context)
{
    KeyValuePair*  pair = (KeyValuePair*)_element;
    SearchContext* ctx  = (SearchContext*)_context;

    if (ctx->keysEqualFunc((void*)pair->key, (void*)ctx->searchKey))
    {
        ctx->foundPair = pair;
        return 0; /* stop iteration */
    }
    return 1; /* continue */
}

/* ===========================================================================
 * Helper — ForEachAction  (ListItrForEach callback)
 * Unwraps KeyValuePair and calls the user's KeyValueActionFunction.
 * Sets ctx->stopped = 1 if the user action returns 0.
 * =========================================================================== */

static int ForEachAction(void* _element, void* _context)
{
    KeyValuePair*   pair = (KeyValuePair*)_element;
    ForEachContext* ctx  = (ForEachContext*)_context;
    int             result;

    result = ctx->userAction(pair->key, (void*)pair->value, ctx->userContext);
    ctx->invokeCount++;

    if (0 == result)
    {
        ctx->stopped = 1;
    }

    return result; /* propagate stop signal to ListItrForEach */
}