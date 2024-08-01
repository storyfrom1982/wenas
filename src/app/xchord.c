#include "ex/ex.h"

#include <sys/struct/xmsger.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>


#define KEY_SPACE   4
#define KEY_LIMIT   (1 << KEY_SPACE)


typedef struct chord {
    uint64_t key;
    struct chord* predecessor;
    struct chord* finger_table[KEY_SPACE];
}*chord_ptr;

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
chord_ptr closest_preceding_finger(chord_ptr chord, uint64_t key)
{
    for (int i = KEY_SPACE - 1; i >= 0; --i) {
        // __xlogd("closest_preceding_finger enter (n->key=%u < finger[%d]->key=%u < key=%u)\n", 
        //     chord->key, i, chord->finger_table[i]->key, key);
        if (key_in_open_interval(chord->finger_table[i]->key, chord->key, key)) {
            // __xlogd("closest_preceding_finger exit  predecessor->key=%u\n", chord->finger_table[i]->key);
            // 节点 i 大于 n，并且在 n 与 key 之间，逐步缩小范围
            return chord->finger_table[i];
        }
    }
    // n 的路由表中必须包含一个 key 所在的区间
    __xlogd("closest_preceding_finger  exit n->key=%u\n", chord->key);
    exit(0);
    // 网络中只有一个节点
    return chord;
}

chord_ptr find_predecessor(chord_ptr chord, uint64_t key)
{
    chord_ptr n = chord;
    chord_ptr suc = chord->finger_table[1];
    // __xlogd("find_predecessor enter (n->key=%u < key=%u <= suc->key=%u]\n", n->key, key, suc->key);
    // key 大于 n 并且小于等于 n 的后继，所以 n 是 key 的前继
    while (!key_in_right_closed_interval(key, n->key, suc->key)) {
        // n 的后继不是 key 的后继，要继续查找
        n = closest_preceding_finger(n, key);
        suc = n->finger_table[1];
        // __xlogd("find_predecessor (n->key=%u < key=%u <= suc->key=%u]\n", n->key, key, suc->key);
    }
    // __xlogd("find_predecessor exit  (n->key=%u < key=%u <= suc->key=%u]\n", n->key, key, suc->key);
    return n;
}

chord_ptr find_successor(chord_ptr chord, uint64_t key) {
    // __xlogd("find_successor enter key=%u n->key=%u\n", key, chord->key);
    if (key == chord->key){
        return chord;
    }
    chord_ptr n = find_predecessor(chord, key);
    // __xlogd("find_successor exit  key=%u suc->key=%u\n", key, n->finger_table[1]->key);
    return n->finger_table[1];
}

void print_node(chord_ptr node)
{
    __xlogd("node->key=======================================%u enter\n", node->key);
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    uint8_t product = 1;
    for (int i = 0; i < KEY_SPACE; ++i){
        __xlogd("node->finger[%u] start_key=%u key=%u\n", 
            i, (node->key + product) % KEY_LIMIT, node->finger_table[i]->key);
        product *= 2;
    }
    if (node->key == node->finger_table[1]->key){
        __xlogd("exit key=%u\n", node->key);
        exit(0);
    }
    __xlogd("node->key=======================================%u exit\n", node->key);
}

void update_others(chord_ptr node)
{
    __xlogd("update_others enter\n");
    print_node(node);
    uint8_t key;
    uint8_t product = 1;
    chord_ptr prev;
    for (int i = 1; i < KEY_SPACE; ++i){
        // if (key_in_left_closed_interval(node->key, node->predecessor->key, 
        //         node->predecessor->chord->finger_table[i]->key)){
        //     node->predecessor->chord->finger_table[i] = node;
        // }
        key = (node->key - product) % KEY_LIMIT;
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, key);
        __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        while (key_in_left_closed_interval(node->key, prev->key, prev->finger_table[i]->key)){
            // node 在 prev 与 prev 的后继之间
            if (i == 1 && prev->key == node->key){
                __xlogd("update_others i=%d prev->key=%u node->key=%u\n", i, prev->key, node->key);
                prev = prev->predecessor;
            }else {
                __xlogd("update_others prev->finder[%d]->key=%u\n", i, node->key);
                prev->finger_table[i] = node;
                prev = prev->predecessor;
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
    chord_ptr ring = (chord_ptr)calloc(1, sizeof(struct chord));

    ring->key = key;
    ring->predecessor = ring;

    for (int i = 0; i < KEY_SPACE; i++) {
        ring->finger_table[i] = ring;
    }

    __xlogd("chord_create exit\n");

    return ring;
}

void chord_join(chord_ptr chord, chord_ptr ring)
{
    __xlogd("chord_join enter\n");

    uint32_t start;

    if (chord->key == ring->key){
        return;
    }

    if (ring->predecessor->key == ring->key){
        // 第一个节点加入，首先要更新自己的前继和后继，使两个节点组成一个环
        ring->predecessor = chord;
        ring->finger_table[1] = chord;
        chord->predecessor = ring;
        chord->finger_table[1] = ring;
        // 更新自己的路由表
        for (int i = 1, stride = 1; i < KEY_SPACE; ++i, stride *= 2){
            start = (ring->key + stride) % KEY_LIMIT;
            if (key_in_right_closed_interval(start, ring->key, chord->key)){
                // start 在 node 与新 node 之间
                ring->finger_table[i] = chord;
            }
        }        
    }else {
        chord_ptr suc = find_successor(ring, chord->key);
        if (suc->key == chord->key){
            __xlogd("Duplicate key node->key=%u suc->%u\n", chord->key, suc->key);
            return;
        }
        __xlogd("node->%u suc->%u\n", chord->key, suc->key);
        // 设置 n 的后继
        chord->finger_table[1] = suc;
        // 设置 n 的前继
        chord->predecessor = suc->predecessor;
        // 设置 n 的前继的后继等于 n
        chord->predecessor->finger_table[1] = chord;
        // 设置 n 的后继的前继等于 n
        suc->predecessor = chord;
    }

    for (int i = 1, stride = 2; i < KEY_SPACE -1; ++i, stride *= 2){
        start = (chord->key + stride) % KEY_LIMIT;
        if (key_in_left_closed_interval(start, chord->key, chord->finger_table[i]->key)){
            // start 在 n 与 finger[i] 之间，所以 n + start 小于 finger[i], 所以 finger[i] 是 start 的后继节点
            chord->finger_table[i+1] = chord->finger_table[i];
        }else {
            chord->finger_table[i+1] = find_successor(ring, start);
        }
    }

    print_node(ring);
    print_node(chord);

    __xlogd("chord_join ----------------------------------------------- update_others enter\n");
    update_others(chord);
    __xlogd("chord_join ----------------------------------------------- update_others exit\n");

    print_node(ring);
    print_node(chord);
}



int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/chord/log", NULL);

    // chord_ptr nodes[KEY_LIMIT] = {0};

    for (int i = 0; i < KEY_LIMIT; ++i){
        nodes[i] = chord_create(i);
    }

    int k = 4;
    for (int i = 0; i < KEY_LIMIT; ++i){
        if ((i % k) == 0){
            if (i <= k){
                chord_join(nodes[(i)%KEY_LIMIT], nodes[0]);
            }else {
                chord_join(nodes[(i)%KEY_LIMIT], nodes[k]);
            }
        }
    }

    // for (int i = 0; i < KEY_LIMIT; ++i){
    //     print_node(&nodes[i]->node);
    // }

    for (int i = 0; i < KEY_LIMIT; ++i){
        if (i <= k){
            chord_ptr search_node = find_successor(nodes[0], (i+1)%KEY_LIMIT);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_LIMIT, search_node->key);
        }else {
            chord_ptr search_node = find_successor(nodes[k], (i+1)%KEY_LIMIT);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_LIMIT, search_node->key);
        }
    }

    for (int i = 0; i < KEY_LIMIT; ++i){
        free(nodes[i]);
    }

    xlog_recorder_close();

    return 0;
}