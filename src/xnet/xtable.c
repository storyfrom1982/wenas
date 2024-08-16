#include "xtable.h"



static inline int number_compare(const void *a, const void *b)
{
	xhtnode_t *x = (xhtnode_t*)a;
	xhtnode_t *y = (xhtnode_t*)b;
	return x->key - y->key;
}

static inline int bytes_compare(const void *a, const void *b)
{
    xhtnode_t *na = (xhtnode_t*)a;
	xhtnode_t *nb = (xhtnode_t*)b;
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

static inline int uuid_compare(const void *a, const void *b)
{
    xhtnode_t *na = (xhtnode_t*)a;
	xhtnode_t *nb = (xhtnode_t*)b;
    uint64_t *x = (uint64_t*)na->uuid;
    uint64_t *y = (uint64_t*)nb->uuid;
    if (na->uuid_len == nb->uuid_len && (na->uuid_len & 7) == 0){
        for (int i = 0; i < na->uuid_len; i+=8){
            if (x != y){
                return x - y;
            }
            x ++;
            y ++;
        }
        return 0;
    }
    return bytes_compare(a, b);
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

xhtnode_t* xhtnode_create(uint64_t hash_key, uint8_t *uuid, uint8_t uuid_len)
{
    xhtnode_t *node = (xhtnode_t*)calloc(1, sizeof(xhtnode_t));
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

void xhash_table_init(xhash_table_t *ht, uint32_t size)
{
    ht->count = 0;
    ht->size = size;
    ht->mask = size - 1;
    ht->head.next = &(ht->head);
    ht->head.prev = &(ht->head);
    ht->table = (xhtnode_t**)calloc(size, sizeof(void*));
    avl_tree_init(&ht->tree, bytes_compare, sizeof(xhtnode_t), AVL_OFFSET(xhtnode_t, node));
}


void xhash_table_clear(xhash_table_t *ht)
{
    if (ht){
        for (xhtnode_t *node = ht->head.next; node != &ht->head; node = node->next){
            free(node->uuid);
            free(node);
        }
        free(ht->table);
        mclear(ht, sizeof(xhash_table_t));
    }
}

void xhash_table_add(xhash_table_t *ht, const char *uuid, void *value)
{
    xhtnode_t *node = xhtnode_create(0, NULL, 0);
    node->key = hash_string(uuid, &node->uuid_len);
    node->uuid = (uint8_t*)malloc(node->uuid_len + 1);
    mcopy(node->uuid, uuid, node->uuid_len + 1);
    node->value = value;
    uint32_t pos = node->key & ht->mask;

    if (ht->table[pos] == NULL){
        ht->table[pos] = node;
        node->prev = &ht->head;
        node->next = ht->head.next;
        node->next->prev = node;
        node->prev->next = node;
        node->list_len = 0;
        ht->count++;

    }else if (bytes_compare(node, ht->table[pos]) != 0 
                && avl_tree_add(&ht->tree, node) == NULL){
        xhtnode_t *head = ht->table[pos];
        node->prev = head;
        node->next = head->next;
        node->prev->next = node;
        node->next->prev = node;
        head->list_len++;
        ht->count++;

    }else {
        free(node->uuid);
        free(node);
    }
}

void* xhash_table_del(xhash_table_t *ht, const char *uuid)
{
    xhtnode_t node = {0};
    void *value = NULL;
    node.key = hash_string(uuid, &node.uuid_len);
    node.uuid = (uint8_t*)uuid;
    uint32_t pos = (node.key & ht->mask);

    if (ht->table[pos] != NULL){

        xhtnode_t *head = ht->table[pos];

        if (head->list_len == 0){
            if (bytes_compare(&node, head) == 0){
                ht->table[pos] = NULL;
            }else {
                return NULL;
            }

        }else if (node.uuid_len == head->uuid_len 
                    && bytes_compare(&node, head) == 0){
            head->next->list_len = (head->list_len-1);
            ht->table[pos] = head->next;
            avl_tree_remove(&ht->tree, head->next);

        }else {
            head = avl_tree_find(&ht->tree, &node);
            if (head){
                ht->table[pos]->list_len--;
                avl_tree_remove(&ht->tree, head);
            }else {}
        }

        if (head){
            head->next->prev = head->prev;
            head->prev->next = head->next;
            value = head->value;
            free(head->uuid);
            free(head);
            ht->count--;
        }
    }

    return value;
}

void* xhash_table_find(xhash_table_t *ht, const char *uuid)
{
    xhtnode_t node = {0};
    node.key = hash_string(uuid, &node.uuid_len);
    node.uuid = (uint8_t*)uuid;
    uint32_t pos = node.key & ht->mask;
    if (ht->table[pos] != NULL){
        if (node.uuid_len == ht->table[pos]->uuid_len 
            && bytes_compare(&node, ht->table[pos]) == 0){
            return ht->table[pos]->value;
        }else {
            xhtnode_t *ret = avl_tree_find(&ht->tree, &node);
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

void xhash_tree_init(xhash_tree_t *ht)
{
    ht->count = 0;
    avl_tree_init(&ht->hash_tree, number_compare, sizeof(xhtnode_t), AVL_OFFSET(xhtnode_t, node));
    avl_tree_init(&ht->uuid_tree, uuid_compare, sizeof(xhtnode_t), AVL_OFFSET(xhtnode_t, node));
}

void xhash_tree_clear(xhash_tree_t *ht)
{
    ht->count = 0;
    avl_tree_clear(&ht->hash_tree, NULL);
    avl_tree_clear(&ht->uuid_tree, NULL);
}

void xhash_tree_add(xhash_tree_t *ht, xhtnode_t *node)
{
    xhtnode_t *head = avl_tree_add(&ht->hash_tree, node);
    if (head){
        if (avl_tree_add(&ht->uuid_tree, node) == NULL){
            ht->count++;
            head->list_len++;
            node->next = head;
            node->prev = head->prev;
            node->next->prev = node;
            node->prev->next = node;
        }
    }else {
        ht->count++;
    }
}

xhtnode_t* xhash_tree_del(xhash_tree_t *ht, xhtnode_t *node)
{
    xhtnode_t *head = avl_tree_find(&ht->hash_tree, node);
    if (head){
        ht->count--;
        if (head->list_len == 0){
            // 这个节点没有冲突的值，直接从哈希树中删除
            avl_tree_remove(&ht->hash_tree, head);
        }else if (mcompare(head->uuid, node->uuid, node->uuid_len) == 0){
            // 这个节点哈希值冲突，这个节点是冲突列表的头，节点在哈希树中
            xhtnode_t *second = head->next;
            // 让下一个节点成为列表头，并且更新列表长度
            second->list_len = head->list_len - 1;
            // 将要删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            avl_tree_remove(&ht->uuid_tree, second);
            // 用第二个节点替换原来的节点
            avl_tree_replace(&ht->hash_tree, head, second);
        }else {
            // 这个节点有冲突的值，这个节点不是冲突列表的头，节点在原始树中
            // 列表头没有改变，只是长度减一
            head->list_len--;
            // 在原始树找到要要删除的节点
            head = avl_tree_find(&ht->uuid_tree, node);
            // 把删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            // 将节点移出原始树
            avl_tree_remove(&ht->uuid_tree, head);
        }
    }
    return head;
}

xhtnode_t* xhash_tree_del_by_key(xhash_tree_t *ht, uint64_t key, uint8_t *uuid, uint32_t len)
{
    xhtnode_t node;
    node.key = key;
    node.uuid = uuid;
    node.uuid_len = len;
    return xhash_tree_del(ht, &node);
}

xhtnode_t* xhash_tree_find(xhash_tree_t *ht, uint64_t key, uint8_t *uuid, uint32_t len)
{
    xhtnode_t node, *head = NULL;
    node.key = key;
    node.uuid = uuid;
    node.uuid_len = len;
    head = avl_tree_find(&ht->hash_tree, &node);
    if (head){
        // 找到哈希值
        if (uuid_compare(&node, head) == 0){
            // uuid 一致
            return head;
        }else if (head->list_len > 0){
            // uuid 不一致，但是这个哈希值对应多个 uuid，在原始树中查找 uuid
            return avl_tree_find(&ht->uuid_tree, &node);
        }
    }
    return NULL;
}

xhtnode_t* xhash_tree_find_by_key(xhash_tree_t *ht, uint64_t key)
{
    xhtnode_t node, *head;
    node.key = key;
    return avl_tree_find(&ht->hash_tree, &node);
}

xhtnode_t *xhash_tree_first(xhash_tree_t *ht)
{
    return avl_tree_first(&ht->hash_tree);
}

xhtnode_t *xhash_tree_last(xhash_tree_t *ht)
{
    return avl_tree_last(&ht->hash_tree);
}

xhtnode_t *xhash_tree_next(xhash_tree_t *ht, xhtnode_t *node)
{
    return avl_tree_next(&ht->hash_tree, node);
}

xhtnode_t *xhash_tree_prev(xhash_tree_t *ht, xhtnode_t *node)
{
    return avl_tree_prev(&ht->hash_tree, node);
}