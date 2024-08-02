#include "ex/ex.h"

#include <sys/struct/xmsger.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>

// 2 <= KEY_BITS
#define KEY_BITS    4
#define KEY_SPACE   (1 << KEY_BITS)


typedef struct chord {
    uint64_t key;
    struct chord* predecessor;
    struct chord* finger_table[KEY_BITS];
}*chord_ptr;

chord_ptr nodes[KEY_SPACE] = {0};


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
    for (int i = KEY_BITS - 1; i >= 0; --i) {
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
    __xlogd("node->key >>>>--------------------------> %u enter\n", node->key);
    uint8_t product = 1;
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%u] start_key=%u key=%u\n", 
            i, (node->key + product) % KEY_SPACE, node->finger_table[i]->key);
        product *= 2;
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    if (node->key == node->finger_table[1]->key){
        __xlogd("exit key=%u\n", node->key);
        exit(0);
    }
    __xlogd("node->key >>>>-----------------------> %u exit\n", node->key);
}

void print_node_all(chord_ptr node)
{
    __xlogd("print_node_all >>>>------------------------------------------------> enter\n");
    chord_ptr n = node->predecessor;
    while (n != node){
        chord_ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == node){
            print_node(n);
            break;
        }        
    }   
    __xlogd("print_node_all >>>>----------------------------------------------> exit\n"); 
}

void update_others(chord_ptr node)
{
    __xlogd("update_others enter\n");
    print_node_all(node);
    uint8_t start;
    chord_ptr prev;
    for (int i = 1, stride = 1; i < KEY_BITS; ++i, stride *= 2){
        start = (node->key - stride) % KEY_SPACE;
        __xlogd("update_others start %u\n", start);
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, start);
        __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        while (key_in_left_closed_interval(node->key, prev->key, prev->finger_table[i]->key)){
            // if (prev->key == node->key){
            //     __xlogd("update_others prev->finder[%d]->key=%u node->key=%u\n", i, prev->finger_table[i]->key, node->key);
            //     // exit(0);
            //     prev = prev->predecessor;
            //     break;
            // }else 
            {
                // node 在 prev 与 prev 的后继之间
                __xlogd("update_others prev->finder[%d]->key=%u\n", i, node->key);
                prev->finger_table[i] = node;
                prev = prev->predecessor;
                __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
                    prev->key, node->key, i, prev->finger_table[i]->key);                
            }
        }
    }
    print_node_all(node);
    __xlogd("update_others exit\n");
}

chord_ptr chord_create(uint8_t key)
{
    __xlogd("chord_create enter\n");
    chord_ptr ring = (chord_ptr)calloc(1, sizeof(struct chord));

    ring->key = key;
    ring->predecessor = ring;

    for (int i = 0; i < KEY_BITS; i++) {
        ring->finger_table[i] = ring;
    }

    __xlogd("chord_create exit\n");

    return ring;
}

chord_ptr chord_join(chord_ptr ring, uint8_t key)
{
    __xlogd("chord_join >>>>--------------------> enter %u\n", key);

    uint32_t start;
    chord_ptr node = NULL;

    if (key == ring->key){
        return node;
    }

    node = chord_create(key);
    // free(node);
    // return NULL;

    if (ring->predecessor->key == ring->key){
        // 第一个节点加入，首先要更新自己的前继和后继，使两个节点组成一个环
        ring->predecessor = node;
        ring->finger_table[1] = node;
        node->predecessor = ring;
        node->finger_table[1] = ring;
        // 更新自己的路由表
        for (int i = 1, stride = 1; i < KEY_BITS; ++i, stride *= 2){
            start = (ring->key + stride) % KEY_SPACE;
            if (key_in_right_closed_interval(start, ring->key, node->key)){
                // start 在 node 与新 node 之间
                ring->finger_table[i] = node;
            }
        }        
        
    }else {
        chord_ptr successor = find_successor(ring, node->key);
        if (successor->key == node->key){
            __xlogd("Duplicate key node->key=%u successor->%u\n", node->key, successor->key);
            free(node);
            return NULL;
        }
        __xlogd("node->%u successor->%u\n", node->key, successor->key);
        // 设置 n 的后继
        node->finger_table[1] = successor;
        // 设置 n 的前继
        node->predecessor = successor->predecessor;
        // 设置 n 的前继的后继等于 n
        node->predecessor->finger_table[1] = node;
        // 设置 n 的后继的前继等于 n
        successor->predecessor = node;
    }


    for (int i = 1, stride = 2; i < KEY_BITS-1; ++i, stride *= 2){
        start = (node->key + stride) % KEY_SPACE;
        if (key_in_left_closed_interval(start, node->key, node->finger_table[i]->key)){
            // start 在 n 与 finger[i] 之间，所以 n + start 小于 finger[i], 所以 finger[i] 是 start 的后继节点
            node->finger_table[i+1] = node->finger_table[i];
            __xlogd("chord_join node[%u]->finger_table[%u]->key=%u start=%u\n", node->key, i+1, node->finger_table[i+1]->key, start);
        }else 
        {
            node->finger_table[i+1] = find_successor(ring, start);
            __xlogd("chord_join node[%u]->finger_table[%u]->key=%u start=%u\n", node->key, i+1, node->finger_table[i+1]->key, start);
        }
    }

    // print_node(ring);
    // print_node(node);
    
    // __xlogd("chord_join ----------------------------------------------- update_others enter\n");
    update_others(node);
    // __xlogd("chord_join ----------------------------------------------- update_others exit\n");

    // print_node(ring);
    // print_node(node);

    __xlogd("chord_join exit 0x%x\n", node);

    return node;
}



int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/chord/log", NULL);

    // chord_ptr nodes[KEY_LIMIT] = {0};

    // for (int i = 0; i < KEY_LIMIT; ++i){
    //     nodes[i] = chord_create(i);
    // }

    nodes[0] = chord_create(0);

    int k = 4;
    chord_ptr n = NULL;
    for (int i = 1; i < KEY_SPACE; ++i){
        if ((i % k) == 0){
            if (i <= k){
                n = chord_join(nodes[0], i);
            }else {
                n = chord_join(nodes[k], i);
            }
        }
        if (n != NULL){
            nodes[i] = n;
            n = NULL;
        }
    }

    // for (int i = 0; i < KEY_LIMIT; ++i){
    //     print_node(&nodes[i]->node);
    // }

    for (int i = 0; i < KEY_SPACE; ++i){
        if (i <= k){
            chord_ptr search_node = find_successor(nodes[0], (i+1)%KEY_SPACE);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_SPACE, search_node->key);
        }else {
            chord_ptr search_node = find_successor(nodes[k], (i+1)%KEY_SPACE);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_SPACE, search_node->key);
        }
    }

    n = nodes[0]->predecessor;
    while (n != nodes[0]){
        chord_ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == nodes[0]){
            print_node(n);
            break;
        }        
    }


    n = nodes[0]->predecessor;
    while (n != nodes[0]){
        chord_ptr t = n->predecessor;
        __xlogd("free node n=%u addr=0x%x\n", n->key, n);
        free(n);
        n = t;
        if (n == nodes[0]){
            __xlogd("free node n=%u addr=0x%x\n", n->key, n);
            free(n);
            break;
        }        
    }

    xlog_recorder_close();

    return 0;
}