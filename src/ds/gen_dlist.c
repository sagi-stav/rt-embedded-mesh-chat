#include "gen_dlist.h"
#include <stdlib.h>

struct Node {
    void* m_data;
    struct Node* m_next;
    struct Node* m_prev;
};

struct List {
    struct Node m_head;
    struct Node m_tail;
};

/* ==========================================
 * Static Helper Functions
 * ========================================== */

/* * Inserts a new node *before* the provided node.
 * Centralizes logic to prevent code duplication across Push and Insert API functions.
 */
static ListItr InsertNodeBefore(struct Node* _nextNode, void* _item) 
{
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (NULL == newNode) 
    {
        return NULL;
    }

    newNode->m_data = _item;
    newNode->m_next = _nextNode;
    newNode->m_prev = _nextNode->m_prev;

    _nextNode->m_prev->m_next = newNode;
    _nextNode->m_prev = newNode;

    return (ListItr)newNode;
}

/* * Unlinks and frees an existing node.
 * Centralizes logic to prevent code duplication across Pop and Remove API functions.
 */
static void* RemoveNode(struct Node* _nodeToRemove) 
{
    void* data = _nodeToRemove->m_data;

    _nodeToRemove->m_prev->m_next = _nodeToRemove->m_next;
    _nodeToRemove->m_next->m_prev = _nodeToRemove->m_prev;

    free(_nodeToRemove);
    
    return data;
}

/* ==========================================
 * API Implementation
 * ========================================== */

List* ListCreate(void) 
{
    List* list = (List*)malloc(sizeof(List));
    if (NULL == list) 
    {
        return NULL;
    }

    /* Initialize sentinel nodes */
    list->m_head.m_prev = NULL;
    list->m_head.m_next = &list->m_tail;
    list->m_head.m_data = NULL;

    list->m_tail.m_prev = &list->m_head;
    list->m_tail.m_next = NULL;
    list->m_tail.m_data = NULL;

    return list;
}

void ListDestroy(List** _pList, void (*_elementDestroy)(void* _item)) 
{
    struct Node* current;
    struct Node* nextNode;

    if (NULL == _pList || NULL == *_pList) 
    {
        return;
    }

    current = (*_pList)->m_head.m_next;
    
    /* Iterate through the list until reaching the tail sentinel node */
    while (current != &(*_pList)->m_tail) 
    {
        nextNode = current->m_next;
        if (NULL != _elementDestroy && NULL != current->m_data) 
        {
            _elementDestroy(current->m_data);
        }
        free(current);
        current = nextNode;
    }

    free(*_pList);
    *_pList = NULL;
}

ListItr ListPushHead(List* _list, void* _item) 
{
    if (NULL == _list) 
    {
        return NULL;
    }
    return InsertNodeBefore(_list->m_head.m_next, _item);
}

ListItr ListPushTail(List* _list, void* _item) 
{
    if (NULL == _list) 
    {
        return NULL;
    }
    return InsertNodeBefore(&_list->m_tail, _item);
}

void* ListPopHead(List* _list) 
{
    if (NULL == _list || _list->m_head.m_next == &_list->m_tail) 
    {
        return NULL;
    }
    return RemoveNode(_list->m_head.m_next);
}

void* ListPopTail(List* _list) 
{
    if (NULL == _list || _list->m_tail.m_prev == &_list->m_head) 
    {
        return NULL;
    }
    return RemoveNode(_list->m_tail.m_prev);
}

ListItr ListItrBegin(const List* _list) 
{
    if (NULL == _list) 
    {
        return NULL;
    }
    return (ListItr)_list->m_head.m_next;
}

ListItr ListItrEnd(const List* _list) 
{
    if (NULL == _list) 
    {
        return NULL;
    }
    return (ListItr)&_list->m_tail;
}

ListItr ListItrNext(ListItr _itr) 
{
    /* As per header constraints: return the iterator itself if it is the End iterator */
    if (NULL == _itr || NULL == ((struct Node*)_itr)->m_next) 
    {
        return _itr; 
    }
    return (ListItr)((struct Node*)_itr)->m_next;
}

ListItr ListItrPrev(ListItr _itr) 
{
    /* Check if we hit the Begin iterator. The head sentinel's m_prev is NULL */
    if (NULL == _itr || NULL == ((struct Node*)_itr)->m_prev || NULL == ((struct Node*)_itr)->m_prev->m_prev) 
    {
        return _itr; 
    }
    return (ListItr)((struct Node*)_itr)->m_prev;
}

void* ListItrGet(ListItr _itr) 
{
    /* Return NULL if invalid or pointing to the End iterator (tail sentinel) */
    if (NULL == _itr || NULL == ((struct Node*)_itr)->m_next) 
    {
        return NULL; 
    }
    return ((struct Node*)_itr)->m_data;
}

void* ListItrSet(ListItr _itr, void* _element) 
{
    void* oldData;
    if (NULL == _itr || NULL == ((struct Node*)_itr)->m_next) 
    {
        return NULL;
    }
    oldData = ((struct Node*)_itr)->m_data;
    ((struct Node*)_itr)->m_data = _element;
    
    return oldData;
}

ListItr ListItrInsertBefore(ListItr _itr, void* _element) 
{
    if (NULL == _itr) 
    {
        return NULL;
    }
    return InsertNodeBefore((struct Node*)_itr, _element);
}

void* ListItrRemove(ListItr _itr) 
{
    /* Safeguard: Ensure we are not attempting to remove the sentinel nodes.
       Head sentinel has m_prev == NULL. Tail sentinel has m_next == NULL. */
    if (NULL == _itr || NULL == ((struct Node*)_itr)->m_next || NULL == ((struct Node*)_itr)->m_prev)
    {
        return NULL; 
    }
    return RemoveNode((struct Node*)_itr);
}

size_t ListSize(const List* _list) 
{
    size_t count = 0;
    struct Node* current;

    if (NULL == _list) 
    {
        return 0;
    }

    current = _list->m_head.m_next;
    while (current != &_list->m_tail) 
    {
        count++;
        current = current->m_next;
    }

    return count;
}

size_t ListIsEmpty(List* _list) 
{
    /* An uninitialized/invalid list is considered empty */
    if (NULL == _list) 
    {
        return 1; 
    }
    return (_list->m_head.m_next == &_list->m_tail) ? 1 : 0;
}

ListItr ListItrForEach(ListItr _begin, ListItr _end, ListActionFunction _action, void* _context) 
{
    struct Node* current = (struct Node*)_begin;

    if (NULL == _begin || NULL == _end || NULL == _action) 
    {
        return NULL;
    }

    while (current != (struct Node*)_end) 
    {
        /* Halt iteration if the user-provided action function returns 0 */
        if (_action(current->m_data, _context) == 0) 
        {
            break; 
        }
        current = current->m_next;
    }

    return (ListItr)current;
}