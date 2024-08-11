#include "xtable.h"



static inline int number_compare(const void *a, const void *b)
{
	xnode_t *x = (xnode_t*)a;
	xnode_t *y = (xnode_t*)b;
	return x->hash - y->hash;
}

static inline int bytes_compare(const void *a, const void *b)
{
    xnode_t *na = (xnode_t*)a;
	xnode_t *nb = (xnode_t*)b;
    // __xlogd("bytes_compare enter %s<>%s\n", na->key, nb->key);
    char *s1 = na->key; 
    char *s2 = nb->key;
    int len = (na->key_len < nb->key_len ? na->key_len : nb->key_len);
    while (--len && *(char*)s1 == *(char*)s2){
        s1 = (char*)s1 + 1;
        s2 = (char*)s2 + 1;
    }
    len = (*(char*)s1 - *(char*)s2);
    if (len == 0){
        return na->key_len - nb->key_len;
    }
    return len;
}

static inline int memory_compare(const void *a, const void *b)
{
    // __xlogd("memory_compare enter\n");
    xnode_t *na = (xnode_t*)a;
	xnode_t *nb = (xnode_t*)b;
    // __xlogd("memory_compare a.len=%lu b.len=%lu\n", na->key_len, nb->key_len);
    uint64_t *x = (uint64_t*)na->key;
    uint64_t *y = (uint64_t*)nb->key;
    if (na->key_len == nb->key_len && (na->key_len & 7) == 0){
        for (int i = 0; i < na->key_len; i+=8){
            // __xlogd("memory_compare x=%lu y=%lu\n", x, y);
            if (x != y){
                return x - y;
            }
            x ++;
            y ++;
        }
        // __xlogd("memory_compare exit\n");
        return 0;
    }
    return bytes_compare(a, b);
}

#define HASHWORDBITS 32

static inline uint32_t hash_string (const char *key, uint32_t *key_len)
{
    unsigned long int hval, g;
    const char *str = key;
    hval = 0;
    while (*str != '\0'){
        (*key_len)++;
        hval <<= 4;
        hval += (unsigned char) *str++;
        g = hval & ((unsigned long int) 0xf << (HASHWORDBITS - 4));
        if (g != 0){
            hval ^= g >> (HASHWORDBITS - 8);
            hval ^= g;
        }
    }
    return hval;
}

xnode_t* xnode_create(uint64_t hash, uint8_t *key, uint8_t key_len)
{
    xnode_t *node = (xnode_t*)calloc(1, sizeof(xnode_t));
    node->hash = hash;
    node->key_len = key_len;
    if (key && key_len){
        node->key = (uint8_t*)malloc(node->key_len + 1);
        mcopy(node->key, key, node->key_len + 1);
    }
    node->list_len = 0;
    node->node.parent = &node->node;
    node->node.left = (struct avl_node*)node;
    node->node.right = (struct avl_node*)node;
    node->next = node;
    node->prev = node;
    return node;    
}

void xhash_table_init(xhash_table_t *table, uint32_t size /*must be the power of 2*/)
{
    table->count = 0;
    table->size = size;
    table->mask = size - 1;
    table->head.next = &(table->head);
    table->head.prev = &(table->head);
    table->table = (xnode_t**)calloc(size, sizeof(void*));
    avl_tree_init(&table->tree, bytes_compare, sizeof(xnode_t), AVL_OFFSET(xnode_t, node));
}


void xhash_table_clear(xhash_table_t *table)
{
    __xlogd("xhash_table_clear enter\n");   
    if (table == NULL){
        return;
    }
    for (xnode_t *node = table->head.next; node != &table->head; node = node->next){
        __xlogd("xhash_table_clear list len=%u key=%s pos=(%u)\n", node->list_len, node->key, node->hash & table->mask);
        if (((node->hash & table->mask) != (node->prev->hash & table->mask)) && node->prev != &table->head && node->list_len == 0){
            __xlogd("xhash_table_clear list len=%u key=%s pos=(%u)\n", node->list_len, node->key, node->hash & table->mask);
            __xlogd("xhash_table_clear error\n");
            exit(0);
        }
        free(node->key);
        free(node);
    }
    free(table->table);
    mclear(table, sizeof(xhash_table_t));
    __xlogd("xhash_table_clear exit\n");
}

void xhash_table_add(xhash_table_t *table, const char *key, void *value)
{
    // __xlogd("xhash_table_add enter\n");
    xnode_t *node = xnode_create(0, NULL, 0);
    node->hash = hash_string(key, &node->key_len);
    // __xlogd("xhash_table_add hash %lu str_len=%u\n", node->hash, node->key_len);
    node->key = (uint8_t*)malloc(node->key_len + 1);
    mcopy(node->key, key, node->key_len + 1);
    node->value = value;
    uint32_t pos = node->hash & table->mask;
    __xlogd("xhash_table_add hash key %s hash=%lu pos=(%u)\n", node->key, node->hash, pos);
    if (table->table[pos] == NULL){
        // __xlogd("xhash_table_add 1\n");
        table->table[pos] = node;
        node->prev = &table->head;
        node->next = table->head.next;
        node->next->prev = node;
        node->prev->next = node;
        node->list_len = 0;
        table->count++;
    }else {
        if (bytes_compare(node, table->table[pos]) != 0 && avl_tree_add(&table->tree, node) == NULL){
            xnode_t *head = table->table[pos];
            node->prev = head;
            node->next = head->next;
            node->prev->next = node;
            node->next->prev = node;
            head->list_len++;
            table->count++;
            __xlogd("xhash_table_add pos=(%u) list len=(%u)\n", head->hash & table->mask, head->list_len);
        }else {
            __xlogd("xhash_table_add repeat key ###### %s\n", node->key);
            free(node->key);
            free(node);
        }
        // __xlogd("xhash_table_add 2.3\n");
    }
    
    // __xlogd("xhash_table_add exit\n");
}

void* xhash_table_del(xhash_table_t *table, const char *key)
{
    __xlogd("xhash_table_del enter %s\n", key);
    xnode_t node = {0};
    void *value = NULL;
    node.hash = hash_string(key, &node.key_len);
    node.key = (uint8_t*)key;
    uint32_t pos = (node.hash & table->mask);
    __xlogd("xhash_table_del hash key %s hash=%lu pos=(%u)\n", node.key, node.hash, pos);
    if (table->table[pos] != NULL){
        // xnode_t *next = NULL;
        xnode_t *head = table->table[pos];
        if (head->list_len == 0){
            if (bytes_compare(&node, head) == 0){
                table->table[pos] = NULL;
            }else {
                __xlogd("xhash_table_del next->key=%s next->pos=(%u) remove key (%s)(%u) == (%s)(%u)\n", 
                        head->next->key, head->next->hash & table->mask, node.key, node.hash & table->mask, head->key, head->hash & table->mask);                
                return NULL;
            }
        }else if (node.key_len == head->key_len && bytes_compare(&node, head) == 0){
            // if ((head->hash & table->mask) != (head->next->hash & table->mask)){
            //     __xlogd("xhash_table_del remove head list_len=%u head hash=%u next hash=%u prev hash=%u\n", 
            //         head->list_len, head->hash & table->mask, head->next->hash & table->mask, head->prev->hash & table->mask);
            //     exit(0);
            // }
            head->next->list_len = (head->list_len-1);
            table->table[pos] = head->next;
            // next = table->table[pos];
            avl_tree_remove(&table->tree, head->next); 
            if (bytes_compare(&node, head) != 0){
                __xlogd("xhash_table_del >>> next->key=%s next->pos=(%u) remove key (%s)(%u) == (%s)(%u)\n", 
                        head->next->key, head->next->hash & table->mask, node.key, node.hash & table->mask, head->key, head->hash & table->mask);
                exit(0);
            }            
        }else {
            head = avl_tree_find(&table->tree, &node);
            if (head){
                // __xlogd("xhash_table_del remove node list_len=%u prev len=%u head hash=%u next hash=%u prev hash=%u\n", 
                //     head->list_len, head->prev->list_len, head->hash & table->mask, head->next->hash & table->mask, head->prev->hash & table->mask);
                table->table[pos]->list_len--;
                // __xlogd("xhash_table_del remove %s key=%s\n", key, head->key);
                // next = table->table[pos];
                // __xlogd("xhash_table_del remove next list_len=%u head hash=%u next hash=%u prev hash=%u\n", 
                //     next->list_len, next->hash & table->mask, next->next->hash & table->mask, next->prev->hash & table->mask);                                
                avl_tree_remove(&table->tree, head);
                // next = table->table[pos];
                if (bytes_compare(&node, head) != 0){
                    __xlogd("xhash_table_del ###>>> next->key=%s next->pos=(%u) remove key (%s)(%u) == (%s)(%u)\n", 
                            head->next->key, head->next->hash & table->mask, node.key, node.hash & table->mask, head->key, head->hash & table->mask);
                    exit(0);
                }                
            }else {
                __xlogd("xhash_table_del remove ###### %s len=%u\n", node.key, node.key_len);
                // exit(0);
            }
        }
        // __xlogd("xhash_table_del >>> remove next list_len=%u head hash=%u next hash=%u prev hash=%u\n", 
        //     next->list_len, next->hash & table->mask, next->next->hash & table->mask, next->prev->hash & table->mask);          
        if (head){
            head->next->prev = head->prev;
            head->prev->next = head->next;
            value = head->value;
            free(head->key);
            free(head);
            table->count--;
        }
        // if (next){
        //     if (next->list_len > 0 && ((next->hash & table->mask) != (next->next->hash & table->mask))){
        //         __xlogd("xhash_table_del remove next list_len=%u head hash=%u next hash=%u prev hash=%u\n", 
        //             next->list_len, next->hash & table->mask, next->next->hash & table->mask, next->prev->hash & table->mask);                
        //         exit(0);
        //     }
        // }
    }
    __xlogd("xhash_table_del exit\n");
    return value;
}

void* xhash_table_find(xhash_table_t *table, const char *key)
{
    // __xlogd("xhash_table_find enter %s\n", key);
    xnode_t node = {0};
    node.hash = hash_string(key, &node.key_len);
    // __xlogd("xhash_table_find key-len=%u\n", node.key_len);
    node.key = (uint8_t*)key;
    uint32_t pos = node.hash & table->mask;
    if (table->table[pos] != NULL){
        // __xlogd("xhash_table_find str=%s pos=%s len=%u=%u\n", key, table->table[pos]->key, node.key_len, table->table[pos]->key_len);
        if (node.key_len == table->table[pos]->key_len 
            && bytes_compare(&node, table->table[pos]) == 0){
            // __xlogd("xhash_table_find exit 1=%s\n", table->table[pos]->value);
            return table->table[pos]->value;
        }else {
            xnode_t *ret = avl_tree_find(&table->tree, &node);
            // __xlogd("xhash_table_find exit 2 %s\n", ret->value);
            if (ret){
                return ret->value;
            }
        }
    }
    // __xlogd("xhash_table_find exit\n");
    return NULL;
}

void xtree_table_init(xtree_table_t *table)
{
    table->count = 0;
    avl_tree_init(&table->hash_tree, number_compare, sizeof(xnode_t), AVL_OFFSET(xnode_t, node));
    avl_tree_init(&table->origin_tree, memory_compare, sizeof(xnode_t), AVL_OFFSET(xnode_t, node));
}

void xtree_table_clear(xtree_table_t *table)
{

}

void xtree_table_add(xtree_table_t *table, xnode_t *node)
{
    // __xlogd("xtree_table_add enter %lu\n", node->hash);
    table->count++;
    // __xlogd("xtree_table_add 1\n");
    xnode_t *head = avl_tree_add(&table->hash_tree, node);
    // __xlogd("xtree_table_add 2\n");
    if (head){
        // __xlogd("xtree_table_add 3\n");
        node->next = head;
        node->prev = head->prev;
        node->next->prev = node;
        node->prev->next = node;
        head->list_len++;
        // __xlogd("xtree_table_add 4\n");
        head = avl_tree_add(&table->origin_tree, node);
        // __xlogd("xtree_table_add 5\n");
        __xlogd("xtree_table_add origin tree count %lu\n", table->origin_tree.count);
    }
    // __xlogd("xtree_table_add exit\n");
}

xnode_t* xtree_table_remove(xtree_table_t *table, xnode_t *node)
{
    // __xlogd("xtree_table_remove enter\n");
    xnode_t *head = avl_tree_find(&table->hash_tree, node);
    if (head){
        table->count--;
        if (head->list_len == 0){
            // __xlogd("xtree_table_remove 1\n");
            // 这个节点没有冲突的值，直接从哈希树中删除
            avl_tree_remove(&table->hash_tree, head);
            // __xlogd("xtree_table_remove 1.1\n");
        }else if (mcompare(head->key, node->key, node->key_len) == 0){
            // __xlogd("xtree_table_remove 2\n");
            // 这个节点哈希值冲突，这个节点是冲突列表的头，节点在哈希树中
            xnode_t *second = head->next;
            // 让下一个节点成为列表头，并且更新列表长度
            second->list_len = head->list_len - 1;
            // 将要删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            // __xlogd("xtree_table_remove 2.1\n");
            avl_tree_remove(&table->origin_tree, second);
            // __xlogd("xtree_table_remove 2.2\n");
            // 用第二个节点替换原来的节点
            avl_tree_replace(&table->hash_tree, head, second);
            // __xlogd("xtree_table_remove 2.3\n");
        }else {
            // __xlogd("xtree_table_remove 3\n");
            // 这个节点有冲突的值，这个节点不是冲突列表的头，节点在原始树中
            // 列表头没有改变，只是长度减一
            head->list_len--;
            // 在原始树找到要要删除的节点
            head = avl_tree_find(&table->origin_tree, node);
            // __xlogd("xtree_table_remove 3.1\n");
            // 把删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            // 将节点移出原始树
            // __xlogd("xtree_table_remove 3.2\n");
            avl_tree_remove(&table->origin_tree, head);
            // __xlogd("xtree_table_remove 3.3\n");
        }
    }
    // __xlogd("xtree_table_remove exit\n");
    return head;
}

xnode_t* xtree_table_find(xtree_table_t *table, uint64_t hash, uint8_t *key, uint32_t len)
{
    xnode_t node, *head;
    node.hash = hash;
    node.key = key;
    node.key_len = len;
    head = avl_tree_find(&table->hash_tree, &node);
    if (head){
        if (head->list_len == 0 || mcompare(head->key, key, len) == 0){
            return head;
        }else {
            return avl_tree_find(&table->origin_tree, &node);
        }
    }
    return head;
}

xnode_t* xtree_table_find_same_hash(xtree_table_t *table, uint64_t hash)
{
    xnode_t node, *head;
    node.hash = hash;
    return avl_tree_find(&table->hash_tree, &node);
}

xnode_t *xtree_table_first(xtree_table_t *table)
{
    return avl_tree_first(&table->hash_tree);
}

xnode_t *xtree_table_last(xtree_table_t *table)
{
    return avl_tree_last(&table->hash_tree);
}

xnode_t *xtree_table_next(xtree_table_t *table, xnode_t *node)
{
    return avl_tree_next(&table->hash_tree, node);
}

xnode_t *xtree_table_prev(xtree_table_t *table, xnode_t *node)
{
    return avl_tree_prev(&table->hash_tree, node);
}