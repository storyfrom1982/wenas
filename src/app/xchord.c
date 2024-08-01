#include "ex/ex.h"

#include <sys/struct/xmsger.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>


#define KEY_SPACE   4
#define KEY_LIMIT   (1 << KEY_SPACE)


typedef struct chord* chord_ptr;


typedef struct node {
    uint64_t key;
    chord_ptr chord;
}node_t;


typedef struct chord {
    node_t node;
    node_t* predecessor;
    node_t* finger_table[KEY_SPACE];
}chord_t;

chord_ptr nodes[KEY_LIMIT] = {0};


static inline bool key_in_open_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (a < b){ 
        if (a < key && key < b){
            // __xlogd("key_in_open_interval --- a < b\n");
            //(a=2)->(key=7)->(b=13) 不含零点
            return true;
        }
    }else if (a > b){
        if ((a < key && key > b) || (a > key && key < b)){ 
            // __xlogd("key_in_open_interval a > b --- key=%lu, a=%lu, b=%lu\n", key, a, b);
            //(a=13)->(key=15)->0->(b=2) 环绕零点
            //(a=13)->0->(key=1)->(b=2) 环绕零点
            return true;
        }
    }
    return false;
}

static inline bool key_in_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == a || key == b || a == b){
        return true;
    }
    return key_in_open_interval(key, a, b);
}

static inline bool key_in_left_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == a){
        return true;
    }
    return key_in_open_interval(key, a, b);
}

static inline bool key_in_right_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == b){
        return true;
    }
    return key_in_open_interval(key, a, b);
}
#include <stdlib.h>
node_t* closest_preceding_finger(chord_ptr chord, uint64_t key)
{
    for (int i = KEY_SPACE - 1; i >= 0; --i) {
        __xlogd("closest_preceding_finger enter (n->key=%u < finger[%d]->key=%u < key=%u)\n", 
            chord->node.key, i, chord->finger_table[i]->key, key);
        if (key_in_open_interval(chord->finger_table[i]->key, chord->node.key, key)) {
            __xlogd("closest_preceding_finger exit  predecessor->key=%u\n", chord->finger_table[i]->key);
            // 节点 i 大于 n，并且在 n 与 key 之间，逐步缩小范围
            return chord->finger_table[i];
        }
    }
    // n 的路由表中必须包含一个 key 所在的区间
    __xlogd("closest_preceding_finger  exit n->key=%u\n", chord->node.key);
    exit(0);
    // 网络中只有一个节点
    return &chord->node;
}

node_t* find_predecessor(chord_ptr chord, uint64_t key)
{
    node_t *n = &chord->node;
    node_t *suc = chord->finger_table[1];
    __xlogd("find_predecessor enter (n->key=%u < key=%u <= suc->key=%u]\n", n->key, key, suc->key);
    // key 大于 n 并且小于等于 n 的后继，所以 n 是 key 的前继
    while (!key_in_right_closed_interval(key, n->key, suc->key)) {
        // n 的后继不是 key 的后继，要继续查找
        n = closest_preceding_finger(n->chord, key);
        suc = n->chord->finger_table[1];
    }
    __xlogd("find_predecessor exit  (n->key=%u < key=%u <= suc->key=%u]\n", n->key, key, suc->key);
    return n;
}

node_t* find_successor(chord_ptr chord, uint64_t key) {
    __xlogd("find_successor enter key=%u n->key=%u\n", key, chord->node.key);
    if (key == chord->node.key){
        return &chord->node;
    }
    node_t *n = find_predecessor(chord, key);
    __xlogd("find_successor exit  key=%u suc->key=%u\n", key, n->chord->finger_table[1]->key);
    return n->chord->finger_table[1];
}

void print_node(node_t *node)
{
    __xlogd("node->key=======================================%u enter\n", node->key);
    __xlogd("node->predecessor=%u\n", node->chord->predecessor->key);
    uint8_t product = 1;
    for (int i = 0; i < KEY_SPACE; ++i){
        __xlogd("node->finger[%u] start_key=%u key=%u\n", 
            i, (node->key + product) % KEY_LIMIT, node->chord->finger_table[i]->key);
        product *= 2;
    }
    if (node->key == node->chord->finger_table[1]->key){
        __xlogd("exit key=%u\n", node->key);
        exit(0);
    }
    __xlogd("node->key=======================================%u exit\n", node->key);
}

void update_others(node_t *node)
{
    __xlogd("update_others enter\n");
    print_node(node);
    uint8_t key;
    uint8_t product = 1;
    node_t *prev;
    for (int i = 1; i < KEY_SPACE; ++i){
        // if (key_in_left_closed_interval(node->key, node->chord->predecessor->key, 
        //         node->chord->predecessor->chord->finger_table[i]->key)){
        //     node->chord->predecessor->chord->finger_table[i] = node;
        // }
        key = (node->key - product) % KEY_LIMIT;
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node->chord, key);
        __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->chord->finger_table[i]->key);
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        while (key_in_left_closed_interval(node->key, prev->key, prev->chord->finger_table[i]->key)){
            // node 在 prev 与 prev 的后继之间
            if (i == 1 && prev->key == node->key){
                __xlogd("update_others i=%d prev->key=%u node->key=%u\n", i, prev->key, node->key);
                prev = prev->chord->predecessor;
            }else {
                __xlogd("update_others prev->finder[%d]->key=%u\n", i, node->key);
                prev->chord->finger_table[i] = node;
                prev = prev->chord->predecessor;
            }
        }
        product *= 2;
    }
    print_node(node);
    __xlogd("update_others exit\n");
}

chord_ptr chord_create(uint8_t key)
{
    __xlogd("chord_create enter\n");
    chord_ptr ring = (chord_ptr)calloc(1, sizeof(chord_t));

    ring->node.key = key;
    ring->predecessor = &ring->node;
    ring->node.chord = ring;

    for (int i = 0; i < KEY_SPACE; i++) {
        ring->finger_table[i] = &ring->node;
    }

    __xlogd("chord_create exit\n");

    return ring;
}

void chord_join(chord_ptr chord, node_t *ring)
{
    __xlogd("chord_join enter\n");

    if (ring->chord->predecessor->key == ring->key){
        ring->chord->predecessor = &chord->node;
        ring->chord->finger_table[1] = &chord->node;
        chord->predecessor = ring;
        chord->finger_table[1] = ring;
        // 第一个节点加入，首先要更新自己的路由表，使两个节点组成一个环
        uint8_t start_key;
        uint8_t product = 1;
        for (int i = 1; i < KEY_SPACE; ++i){
            start_key = (ring->key + product) % KEY_LIMIT;
            if (key_in_right_closed_interval(start_key, ring->key, chord->node.key)){
                // start key 在 node 与新 node 之间
                ring->chord->finger_table[i] = &chord->node;
            }
            product *= 2;
        }        
    }else {
        node_t *suc = find_successor(ring->chord, chord->node.key);
        if (suc->key == chord->node.key){
            __xlogd("Duplicate key node->key=%u suc->%u\n", chord->node.key, suc->key);
            return;
        }
        __xlogd("node->%u suc->%u\n", chord->node.key, suc->key);
        // 设置 n 的后继
        chord->finger_table[1] = suc;
        // 设置 n 的前继
        chord->predecessor = suc->chord->predecessor;
        // 设置 n 的前继的后继等于 n
        chord->predecessor->chord->finger_table[1] = &chord->node;
        // 设置 n 的后继的前继等于 n
        suc->chord->predecessor = &chord->node;
    }

    uint8_t start_key;
    uint8_t product = 2;
    for (int i = 1; i < KEY_SPACE -1; ++i){
        start_key = (chord->node.key + product) % KEY_LIMIT;
        if (key_in_left_closed_interval(start_key, chord->node.key, chord->finger_table[i]->key)){
            // start key 在 n 与 finger[i] 之间，所以 finger[i] 是 start key 的后继节点
            __xlogd("chord_join >>>>>>>>>>>---------------------->\n");
            chord->finger_table[i+1] = chord->finger_table[i];
        }else {
            chord->finger_table[i+1] = find_successor(ring->chord, start_key);
        }
        product *= 2;
    }

    for (int i = 0; i < chord->node.key; ++i)
        print_node(&nodes[i]->node);

    __xlogd("chord_join ----------------------------------------------- update_others enter\n");
    update_others(&chord->node);
    __xlogd("chord_join ----------------------------------------------- update_others exit\n");

    for (int i = 0; i < chord->node.key; ++i)
        print_node(&nodes[i]->node);
}



int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/chord/log", NULL);

    // chord_ptr nodes[KEY_LIMIT] = {0};

    for (int i = 0; i < KEY_LIMIT; ++i){
        nodes[i] = chord_create(i);
    }

    for (int i = 0; i < KEY_LIMIT - 8; ++i){
        // chord_join(nodes[(i+1)%KEY_LIMIT], &nodes[i]->node);
        chord_join(nodes[(i+1)%KEY_LIMIT], &nodes[0]->node);
    }

    // for (int i = 0; i < KEY_LIMIT; ++i){
    //     print_node(&nodes[i]->node);
    // }

    for (int i = 0; i < KEY_LIMIT; ++i){
        // node_t *search_node = find_successor(nodes[i%(KEY_LIMIT-8)], (i+1)%KEY_LIMIT);
        node_t *search_node = find_successor(nodes[0], (i+1)%KEY_LIMIT);
        __xlogd("search key %u: fond %u\n", (i+1)%KEY_LIMIT, search_node->key);
    }

    for (int i = 0; i < KEY_LIMIT; ++i){
        free(nodes[i]);
    }

    xlog_recorder_close();

    return 0;
}