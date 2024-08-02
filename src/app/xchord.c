#include "ex/ex.h"

#include <sys/struct/xmsger.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>

// KEY_BITS >= 2
#define KEY_BITS    6
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
            //(a=2)->(key=7)->(b=13) 不含零点
            return true;
        }
    }else if (a > b){
        if ((a < key && key > b) || (a > key && key < b)){ 
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

chord_ptr closest_preceding_finger(chord_ptr chord, uint64_t key)
{
    chord_ptr finger;
    for (int i = KEY_BITS - 1; i >= 0; --i) {
        finger = chord->finger_table[i];
        if (key_in_open_interval(finger->key, chord->key, key)) {
            // 节点 i 大于 n，并且在 n 与 key 之间，逐步缩小范围
            return chord->finger_table[i];
        }
    }
    // 网络中只有一个节点
    return chord;
}

chord_ptr find_predecessor(chord_ptr chord, uint64_t key)
{
    chord_ptr n = chord;
    chord_ptr successor = chord->finger_table[1];
    // key 大于 n 并且小于等于 n 的后继，所以 n 是 key 的前继
    while (!key_in_right_closed_interval(key, n->key, successor->key)) {
        // n 的后继不是 key 的后继，要继续查找
        n = closest_preceding_finger(n, key);
        successor = n->finger_table[1];
    }
    return n;
}

chord_ptr find_successor(chord_ptr chord, uint64_t key) {
    if (key == chord->key){
        return chord;
    }
    chord_ptr n = find_predecessor(chord, key);
    return n->finger_table[1];
}

void print_node(chord_ptr node)
{
    __xlogd("node->key >>>>--------------------------> enter %u\n", node->key);
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%u] start_key=%u key=%u\n", 
            i, (node->key + (1 << (i-1))) % KEY_SPACE, node->finger_table[i]->key);
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    __xlogd("node->key >>>>-----------------------> exit %u\n", node->key);
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
    chord_ptr prev, next, finger_node;
    for (int i = 1; i < KEY_BITS; ++i){
        start = (node->key - (1 << (i-1))) % KEY_SPACE;
        __xlogd("update_others start %u\n", start);
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, start);
        __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        next = prev->finger_table[1];
        if (next->key == start){
            // start 的位置正好指向一个 node，所以这个 node 的路由表项可能会指向当前 node
            finger_node = next->finger_table[i];
            if ((key_in_open_interval(node->key, next->key, finger_node->key) 
                    || next->key == finger_node->key /*原来的路由表指向本身，需要更新*/)){
                next->finger_table[i] = node;
            }
        }
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        finger_node = prev->finger_table[i];
        while (key_in_open_interval(node->key, prev->key, finger_node->key) 
                || prev->key == finger_node->key /*原来的路由表指向本身，需要更新*/){
            // node 在 prev 与 prev 的后继之间
            __xlogd("update_others prev->finder[%d]->key=%u\n", i, node->key);
            prev->finger_table[i] = node;
            prev = prev->predecessor;
            finger_node = prev->finger_table[i];
            __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
                prev->key, node->key, i, prev->finger_table[i]->key);                
        }
    }
    print_node_all(node);
    __xlogd("update_others exit\n");
}

chord_ptr chord_create(uint8_t key)
{
    __xlogd("chord_create enter\n");

    chord_ptr node = (chord_ptr)calloc(1, sizeof(struct chord));

    node->key = key;
    node->predecessor = node;

    for (int i = 0; i < KEY_BITS; i++) {
        node->finger_table[i] = node;
    }

    __xlogd("chord_create exit\n");

    return node;
}


chord_ptr chord_join(chord_ptr ring, uint8_t key)
{
    __xlogd("chord_join >>>>--------------------> enter %u\n", key);

    uint32_t start;
    chord_ptr node = NULL;
    chord_ptr successor = NULL;

    if (key == ring->key){
        return NULL;
    }

    node = chord_create(key);

    if (ring->predecessor->key == ring->key){
        successor = ring;
        // 更新 finger_table，与新节点组成一个环
        for (int i = 1; i < KEY_BITS; ++i){
            start = (ring->key + (1 << (i-1))) % KEY_SPACE;
            if (key_in_right_closed_interval(start, ring->key, key)){
                // start 在 node 与新 node 之间
                ring->finger_table[i] = node;
            }
        }
        
    }else {
        successor = find_successor(ring, key);
        if (key == successor->key){
            __xlogd("duplicate key=%u successor->key=%u\n", key, successor->key);
            free(node);
            return NULL;
        }
    }

    // 设置 node 的前继
    node->predecessor = successor->predecessor;
    // 设置 node 的后继的前继等于 node
    successor->predecessor = node;
    // 初始化 finger_table
    node->finger_table[1] = successor;
    for (int i = 1; i < KEY_BITS-1; ++i){
        start = (node->key + (1 << i)) % KEY_SPACE;
        if (key_in_left_closed_interval(start, node->key, node->finger_table[i]->key)){
            // start 在 n 与 finger[i] 之间，所以 n + start 小于 finger[i], 所以 finger[i] 是 start 的后继节点
            node->finger_table[i+1] = node->finger_table[i];
        }else {
            node->finger_table[i+1] = find_successor(ring, start);
        }
    }

    // 更新所有需要指向 node 的节点
    update_others(node);

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

    int k = 3;
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