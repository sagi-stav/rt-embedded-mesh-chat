#include "genqueue.h"
#include <stdlib.h>

struct Queue {
    void** m_items;
    size_t m_size;
    size_t m_head;
    size_t m_tail;
    size_t m_nItems;
};

/* Helper for circular index calculation */
static size_t GetNextIndex(size_t _current, size_t _size) {
    return (_current + 1) % _size;
}

Queue* QueueCreate(size_t _size) {
    Queue* queue;
    if (_size == 0) return NULL;

    queue = (Queue*)malloc(sizeof(Queue));
    if (!queue) return NULL;

    queue->m_items = (void**)malloc(_size * sizeof(void*));
    if (!queue->m_items) {
        free(queue);
        return NULL;
    }

    queue->m_size = _size;
    queue->m_head = 0;
    queue->m_tail = 0;
    queue->m_nItems = 0;

    return queue;
}

void QueueDestroy(Queue** _queue, DestroyItem _itemDestroy) {
    size_t i;
    if (!_queue || !*_queue) return;

    if (_itemDestroy) {
        size_t curr = (*_queue)->m_head;
        for (i = 0; i < (*_queue)->m_nItems; ++i) {
            _itemDestroy((*_queue)->m_items[curr]);
            curr = GetNextIndex(curr, (*_queue)->m_size);
        }
    }

    free((*_queue)->m_items);
    free(*_queue);
    *_queue = NULL;
}

QueueResult QueueInsert(Queue* _queue, void* _item) {
    if (!_queue) return QUEUE_UNINITIALIZED_ERROR;
    if (!_item) return QUEUE_DATA_UNINITIALIZED_ERROR;
    if (_queue->m_nItems == _queue->m_size) return QUEUE_OVERFLOW_ERROR;

    _queue->m_items[_queue->m_tail] = _item;
    _queue->m_tail = GetNextIndex(_queue->m_tail, _queue->m_size);
    _queue->m_nItems++;

    return QUEUE_SUCCESS;
}

QueueResult QueueRemove(Queue* _queue, void** _item) {
    if (!_queue || !_item) return QUEUE_UNINITIALIZED_ERROR;
    if (_queue->m_nItems == 0) return QUEUE_DATA_NOT_FOUND_ERROR;

    *_item = _queue->m_items[_queue->m_head];
    _queue->m_head = GetNextIndex(_queue->m_head, _queue->m_size);
    _queue->m_nItems--;

    return QUEUE_SUCCESS;
}

size_t QueueIsEmpty(Queue* _queue) {
    return (!_queue || _queue->m_nItems == 0);
}

size_t QueueForEach(Queue* _queue, ActionFunction _action, void* _context) {
    size_t i, curr;
    if (!_queue || !_action) return 0;

    curr = _queue->m_head;
    for (i = 0; i < _queue->m_nItems; ++i) {
        if (_action(_queue->m_items[curr], _context) == 0) {
            return i + 1;
        }
        curr = GetNextIndex(curr, _queue->m_size);
    }
    return _queue->m_nItems;
}