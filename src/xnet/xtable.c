#include "xtable.h"



static inline int number_compare(const void *a, const void *b)
{
    // if (((index_node_t*)(a))->index == ((index_node_t*)(b))->index){
    //     return 0;
    // }
	// return ((index_node_t*)(a))->index > ((index_node_t*)(b))->index ? 1 : -1;
    return ((index_node_t*)(a))->index - ((index_node_t*)(b))->index;
}

static inline int bytes_compare(const void *a, const void *b)
{
    search_node_t *na = (search_node_t*)a;
	search_node_t *nb = (search_node_t*)b;
    char *s1 = na->uuid; 
    char *s2 = nb->uuid;
    int len = (na->uuid_len < nb->uuid_len ? na->uuid_len : nb->uuid_len);
    while (--len && *(char*)s1 == *(char*)s2){
        s1 = (char*)s1 + 1;
        s2 = (char*)s2 + 1;
    }
    len = (*(char*)s1 - *(char*)s2);
    if (len == 0){
        return na->uuid_len - nb->uuid_len;
    }
    return len;
}

static inline uint32_t hash_string(const char *uuid, uint32_t *uuid_len)
{
    unsigned long int hval, g;
    const char *str = uuid;
    hval = 0;
    while (*str != '\0'){
        (*uuid_len)++;
        hval <<= 4;
        hval += (unsigned char) *str++;
        g = hval & ((unsigned long int) 0xf << (32 - 4));
        if (g != 0){
            hval ^= g >> (32 - 8);
            hval ^= g;
        }
    }
    return hval;
}

search_node_t* hnode_create(uint64_t hash_key, uint8_t *uuid, uint8_t uuid_len)
{
    search_node_t *node = (search_node_t*)calloc(1, sizeof(search_node_t));
    node->key = hash_key;
    if (uuid && uuid_len){
        node->uuid_len = uuid_len;
        node->uuid = (uint8_t*)malloc(node->uuid_len + 1);
        mcopy(node->uuid, uuid, node->uuid_len + 1);
    }
    node->next = node;
    node->prev = node;
    return node;
}

void search_table_init(search_table_t *st, uint32_t size)
{
    st->count = 0;
    st->size = size;
    st->mask = size - 1;
    st->head.next = &(st->head);
    st->head.prev = &(st->head);
    st->table = (search_node_t**)calloc(size, sizeof(void*));
    avl_tree_init(&st->tree, bytes_compare, sizeof(search_node_t), AVL_OFFSET(search_node_t, node));
}


void search_table_clear(search_table_t *st)
{
    if (st){
        for (search_node_t *node = st->head.next; node != &st->head; node = node->next){
            free(node->uuid);
            free(node);
        }
        free(st->table);
        mclear(st, sizeof(search_table_t));
    }
}

void search_table_add(search_table_t *st, const char *uuid, void *value)
{
    search_node_t *node = hnode_create(0, NULL, 0);
    node->key = hash_string(uuid, &node->uuid_len);
    node->uuid = (uint8_t*)malloc(node->uuid_len + 1);
    mcopy(node->uuid, uuid, node->uuid_len + 1);
    node->value = value;
    uint32_t pos = node->key & st->mask;

    if (st->table[pos] == NULL){
        st->table[pos] = node;
        node->prev = &st->head;
        node->next = st->head.next;
        node->next->prev = node;
        node->prev->next = node;
        node->list_len = 0;
        st->count++;

    }else if (bytes_compare(node, st->table[pos]) != 0 
                && avl_tree_add(&st->tree, node) == NULL){
        search_node_t *head = st->table[pos];
        node->prev = head;
        node->next = head->next;
        node->prev->next = node;
        node->next->prev = node;
        head->list_len++;
        st->count++;

    }else {
        free(node->uuid);
        free(node);
    }
}

void* search_table_del(search_table_t *st, const char *uuid)
{
    search_node_t node = {0};
    void *value = NULL;
    node.key = hash_string(uuid, &node.uuid_len);
    node.uuid = (uint8_t*)uuid;
    uint32_t pos = (node.key & st->mask);

    if (st->table[pos] != NULL){

        search_node_t *head = st->table[pos];

        if (head->list_len == 0){
            if (bytes_compare(&node, head) == 0){
                st->table[pos] = NULL;
            }else {
                return NULL;
            }

        }else if (node.uuid_len == head->uuid_len 
                    && bytes_compare(&node, head) == 0){
            head->next->list_len = (head->list_len-1);
            st->table[pos] = head->next;
            avl_tree_remove(&st->tree, head->next);

        }else {
            head = avl_tree_find(&st->tree, &node);
            if (head){
                st->table[pos]->list_len--;
                avl_tree_remove(&st->tree, head);
            }else {}
        }

        if (head){
            head->next->prev = head->prev;
            head->prev->next = head->next;
            value = head->value;
            free(head->uuid);
            free(head);
            st->count--;
        }
    }

    return value;
}

void* search_table_find(search_table_t *st, const char *uuid)
{
    search_node_t node = {0};
    node.key = hash_string(uuid, &node.uuid_len);
    node.uuid = (uint8_t*)uuid;
    uint32_t pos = node.key & st->mask;
    if (st->table[pos] != NULL){
        if (node.uuid_len == st->table[pos]->uuid_len 
            && bytes_compare(&node, st->table[pos]) == 0){
            return st->table[pos]->value;
        }else {
            search_node_t *ret = avl_tree_find(&st->tree, &node);
            if (ret){
                return ret->value;
            }
        }
    }
    return NULL;
}

///////////////////////////////////////////
///////////////////////////////////////////
///////////////////////////////////////////

void index_table_init(index_table_t *it, uint32_t size /*must be the power of 2*/)
{
    it->count = 0;
    it->size = size;
    it->mask = size - 1;
    it->head.next = &(it->head);
    it->head.prev = &(it->head);
    it->table = (index_node_t**)calloc(size, sizeof(void*));
    avl_tree_init(&it->tree, number_compare, sizeof(index_node_t), AVL_OFFSET(index_node_t, node));
}

void index_table_clear(index_table_t *it)
{
    it->count = 0;
    avl_tree_clear(&it->tree, NULL);
}

void index_table_add(index_table_t *it, index_node_t *node)
{
    uint32_t pos = node->index & it->mask;

    if (it->table[pos] == NULL){
        it->table[pos] = node;
        node->prev = &it->head;
        node->next = it->head.next;
        node->next->prev = node;
        node->prev->next = node;
        node->list_len = 0;
        it->count++;

    }else if ((node->index != it->table[pos]->index) 
                && avl_tree_add(&it->tree, node) == NULL){
        index_node_t *head = it->table[pos];
        node->prev = head;
        node->next = head->next;
        node->prev->next = node;
        node->next->prev = node;
        head->list_len++;
        it->count++;
    }
}

void* index_table_del(index_table_t *it, index_node_t *node)
{
    index_node_t *head = NULL;
    uint32_t pos = (node->index & it->mask);

    if (it->table[pos] != NULL){

        head = it->table[pos];

        if (head->list_len == 0){
            if (node->index == head->index){
                it->table[pos] = NULL;
            }else {
                return NULL;
            }

        }else if (node->index == head->index){
            head->next->list_len = (head->list_len-1);
            it->table[pos] = head->next;
            avl_tree_remove(&it->tree, head->next);

        }else {
            head = avl_tree_find(&it->tree, node);
            if (head){
                it->table[pos]->list_len--;
                avl_tree_remove(&it->tree, head);
            }else {}
        }

        if (head){
            head->next->prev = head->prev;
            head->prev->next = head->next;
            it->count--;
        }
    }

    return head;
}

void* index_table_find(index_table_t *it, uint64_t index)
{
    __xlogd("index_table_find enter\n");
    uint32_t pos = index & it->mask;
    if (it->table[pos] != NULL){
        if (index == it->table[pos]->index){
            __xlogd("index_table_find exit %p\n", it->table[pos]);
            return it->table[pos];
        }else {
            index_node_t node;
            node.index = index;
            __xlogd("index_table_find exit\n");
            return avl_tree_find(&it->tree, &node);
        }
    }
    __xlogd("index_table_find exit NULL\n");
    return NULL;
}