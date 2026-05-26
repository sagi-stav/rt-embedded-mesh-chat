#include <stdlib.h> /* malloc, realloc, free */
#include <assert.h> /* assert */
#include "vector.h"

/**
 * @struct Vector
 * @brief Internal representation of the dynamic array.
 * * m_items: Pointer to an array of void pointers (generic storage).
 * m_originalCapacity: Stored to ensure we don't shrink below the initial size.
 * m_capacity: Current total allocated slots.
 * m_size: Number of elements actually stored.
 * m_blockSize: Growth/shrink factor (step size).
 */
struct Vector {
    void** m_items;
    size_t m_originalCapacity;
    size_t m_capacity;
    size_t m_size;
    size_t m_blockSize;
};

/* --- Lifecycle Functions --- */

Vector* VectorCreate(size_t _initialCapacity, size_t _blockSize) {
    Vector* vector = NULL;

    /* Per requirements: return NULL if both params are zero */
    if (_initialCapacity == 0 && _blockSize == 0) {
        return NULL;
    }

    vector = (Vector*)malloc(sizeof(Vector));
    if (NULL == vector) {
        return NULL;
    }

    /* Optimization: Only allocate items if initial capacity > 0 */
    vector->m_items = (void**)malloc(_initialCapacity * sizeof(void*));
    if (NULL == vector->m_items && _initialCapacity > 0) {
        free(vector);
        return NULL;
    }

    vector->m_originalCapacity = _initialCapacity;
    vector->m_capacity = _initialCapacity;
    vector->m_size = 0;
    vector->m_blockSize = _blockSize;

    return vector;
}

void VectorDestroy(Vector** _vector, void (*_elementDestroy)(void* _item)) {
    size_t i;
    if (_vector == NULL || *_vector == NULL) {
        return;
    }

    /* If a destroyer function is provided, apply it to all current elements */
    if (_elementDestroy != NULL) {
        for (i = 0; i < (*_vector)->m_size; ++i) {
            _elementDestroy((*_vector)->m_items[i]);
        }
    }

    free((*_vector)->m_items);
    free(*_vector);
    
    /* Defensive programming: set pointer to NULL to prevent double-free/dangling pointers */
    *_vector = NULL;
}

/* --- Core Logic --- */

VectorResult VectorAppend(Vector* _vector, void* _item) {
    void** temp = NULL;
    if (NULL == _vector) return VECTOR_UNITIALIZED_ERROR;

    /* Check if resize is needed */
    if (_vector->m_size == _vector->m_capacity) {
        if (_vector->m_blockSize == 0) return VECTOR_ALLOCATION_ERROR; /* Fixed size vector */

        /* Reallocate with block size increment */
        temp = (void**)realloc(_vector->m_items, (_vector->m_capacity + _vector->m_blockSize) * sizeof(void*));
        if (NULL == temp) return VECTOR_ALLOCATION_ERROR;

        _vector->m_items = temp;
        _vector->m_capacity += _vector->m_blockSize;
    }

    _vector->m_items[_vector->m_size] = _item;
    _vector->m_size++;
    return VECTOR_SUCCESS;
}

VectorResult VectorRemove(Vector* _vector, void** _pValue) {
    void** temp = NULL;
    if (NULL == _vector) return VECTOR_UNITIALIZED_ERROR;
    assert(_pValue != NULL);

    if (_vector->m_size == 0) return VECTOR_INDEX_OUT_OF_BOUNDS_ERROR;

    /* Extract the last element */
    *_pValue = _vector->m_items[_vector->m_size - 1];
    _vector->m_size--;

    /* * Shrink Logic with "Floor" protection:
     * 1. Only shrink if blockSize > 0 (not a fixed-size vector).
     * 2. Only shrink if current capacity is GREATER than the original capacity.
     * 3. Strategy: Shrink when there's a gap of 2*blockSize to avoid "oscillations" (Hysteresis).
     */
    if (_vector->m_blockSize > 0 &&
        _vector->m_capacity > _vector->m_originalCapacity &&
        (_vector->m_capacity - _vector->m_size) >= (_vector->m_blockSize * 2)) {

        size_t newCapacity = _vector->m_capacity - _vector->m_blockSize;

        /* Ensure we never drop below the initial floor */
        if (newCapacity < _vector->m_originalCapacity) {
            newCapacity = _vector->m_originalCapacity;
        }

        /* Only perform realloc if we are actually changing the size */
        if (newCapacity != _vector->m_capacity) {
            temp = (void**)realloc(_vector->m_items, newCapacity * sizeof(void*));
            if (temp != NULL) {
                _vector->m_items = temp;
                _vector->m_capacity = newCapacity;
            }
        }
        }

    return VECTOR_SUCCESS;
}
/* --- Accessors & Utilities --- */

VectorResult VectorGet(const Vector* _vector, size_t _index, void** _pValue) {
    if (NULL == _vector) return VECTOR_UNITIALIZED_ERROR;
    if (_index >= _vector->m_size) return VECTOR_INDEX_OUT_OF_BOUNDS_ERROR;
    assert(_pValue != NULL);

    *_pValue = _vector->m_items[_index];
    return VECTOR_SUCCESS;
}

VectorResult VectorSet(Vector* _vector, size_t _index, void* _value) {
    if (NULL == _vector) return VECTOR_UNITIALIZED_ERROR;
    if (_index >= _vector->m_size) return VECTOR_INDEX_OUT_OF_BOUNDS_ERROR;

    _vector->m_items[_index] = _value;
    return VECTOR_SUCCESS;
}

size_t VectorSize(const Vector* _vector) {
    return (NULL == _vector) ? 0 : _vector->m_size;
}

size_t VectorCapacity(const Vector* _vector) {
    return (NULL == _vector) ? 0 : _vector->m_capacity;
}

size_t VectorForEach(const Vector* _vector, VectorElementAction _action, void* _context) {
    size_t i;
    if (NULL == _vector || NULL == _action) return 0;

    for (i = 0; i < _vector->m_size; ++i) {
        /* If user action returns 0, we halt iteration as per documentation */
        if (_action(_vector->m_items[i], i, _context) == 0) {
            break;
        }
    }
    return i;
}