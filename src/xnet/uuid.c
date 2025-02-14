#include "uuid.h"

#include <xlib/xxhash.h>
#include <xlib/xsha256.h>


void* uuid_generate(void *uuid_bin_buf, const char *user_name)
{
    int name_len = xlen(user_name);
    char seed[80] = {0};
    SHA256_CTX shactx;
    uint64_t *millisecond = (uint64_t *)seed;
    *millisecond = __xapi->time();
    millisecond++;
    *millisecond = __xapi->clock();
    if (name_len > 64){
        name_len = 64;
    }
    xcopy(seed + 16, user_name, name_len);
    sha256_init(&shactx);
    sha256_update(&shactx, (const uint8_t*)seed, name_len + 16);
    sha256_finish(&shactx, uuid_bin_buf);
    return uuid_bin_buf;
}

static inline int number_compare(const void *a, const void *b)
{
	return ((uuid_node_t*)a)->hash_key - ((uuid_node_t*)b)->hash_key;
}

static inline int uuid_compare(const void *a, const void *b)
{
    uint64_t *x = ((uuid_node_t*)a)->uuid;
    uint64_t *y = ((uuid_node_t*)b)->uuid;
    for (int i = 0; i < 4; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

void uuid_list_init(uuid_list_t *ul)
{
    ul->count = 0;
    avl_tree_init(&ul->hash_tree, number_compare, number_compare, sizeof(uuid_node_t), AVL_OFFSET(uuid_node_t, node));
    avl_tree_init(&ul->uuid_tree, uuid_compare, uuid_compare, sizeof(uuid_node_t), AVL_OFFSET(uuid_node_t, node));
}

void uuid_list_clear(uuid_list_t *ul)
{
    ul->count = 0;
    avl_tree_clear(&ul->hash_tree, NULL);
    avl_tree_clear(&ul->uuid_tree, NULL);
}

void uuid_list_add(uuid_list_t *ul, uuid_node_t *node)
{
    uuid_node_t *head = avl_tree_add(&ul->hash_tree, node);
    if (head){
        if (avl_tree_add(&ul->uuid_tree, node) == NULL){
            ul->count++;
            head->list_len++;
            node->next = head;
            node->prev = head->prev;
            node->next->prev = node;
            node->prev->next = node;
        }
    }else {
        ul->count++;
    }
}

uuid_node_t* uuid_list_del(uuid_list_t *ul, uuid_node_t *node)
{
    uuid_node_t *head = avl_tree_find(&ul->hash_tree, node);
    if (head){
        ul->count--;
        if (head->list_len == 0){
            // 这个节点没有冲突的值，直接从哈希树中删除
            avl_tree_remove(&ul->hash_tree, head);
        }else if (uuid_compare(head, node) == 0){
            // 这个节点哈希值冲突，这个节点是冲突列表的头，节点在哈希树中
            uuid_node_t *second = head->next;
            // 让下一个节点成为列表头，并且更新列表长度
            second->list_len = head->list_len - 1;
            // 将要删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            avl_tree_remove(&ul->uuid_tree, second);
            // 用第二个节点替换原来的节点
            avl_tree_replace(&ul->hash_tree, head, second);
        }else {
            // 这个节点有冲突的值，这个节点不是冲突列表的头，节点在原始树中
            // 列表头没有改变，只是长度减一
            head->list_len--;
            // 在原始树找到要要删除的节点
            head = avl_tree_find(&ul->uuid_tree, node);
            // 把删除的节点移出冲突列表
            head->next->prev = head->prev;
            head->prev->next = head->next;
            // 将节点移出原始树
            avl_tree_remove(&ul->uuid_tree, head);
        }
    }
    return head;
}

uuid_node_t* uuid_list_find(uuid_list_t *ul, uint64_t hash_key, uint64_t *uuid)
{
    uuid_node_t node, *head = NULL;
    node.hash_key = hash_key;
    head = avl_tree_find(&ul->hash_tree, &node);
    if (head){
        node.uuid[0] = uuid[0];
        node.uuid[1] = uuid[1];
        node.uuid[2] = uuid[2];
        node.uuid[3] = uuid[3];
        // 找到哈希值
        if (uuid_compare(&node, head) == 0){
            // uuid 一致
            return head;
        }else if (head->list_len > 0){
            // uuid 不一致，但是这个哈希值对应多个 uuid，在原始树中查找 uuid
            return avl_tree_find(&ul->uuid_tree, &node);
        }
    }
    return NULL;
}

uuid_node_t* uuid_list_find_by_hash(uuid_list_t *ul, uint64_t hash_key)
{
    uuid_node_t node, *head;
    node.hash_key = hash_key;
    return avl_tree_find(&ul->hash_tree, &node);
}

uuid_node_t* uuid_list_first(uuid_list_t *ul)
{
    return avl_tree_first(&ul->hash_tree);
}

uuid_node_t* uuid_list_last(uuid_list_t *ul)
{
    return avl_tree_last(&ul->hash_tree);
}

uuid_node_t* uuid_list_next(uuid_list_t *ul, uuid_node_t *node)
{
    return avl_tree_next(&ul->hash_tree, node);
}

uuid_node_t* uuid_list_prev(uuid_list_t *ul, uuid_node_t *node)
{
    return avl_tree_prev(&ul->hash_tree, node);
}