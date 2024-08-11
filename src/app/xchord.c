#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>

// KEY_BITS >= 2
#define KEY_BITS    4
#define KEY_SPACE   (1 << KEY_BITS)


typedef struct Node {
    uint32_t key;
    struct Node* predecessor;
    struct Node* finger_table[KEY_BITS];
}*Node_Ptr;

// Node_Ptr nodes[KEY_SPACE] = {0};


typedef struct XChord {

    uint32_t count;
    Node_Ptr ring;
    Node_Ptr prev, next;

}*XChord_Ptr;


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

Node_Ptr closest_preceding_finger(Node_Ptr chord, uint32_t key)
{
    Node_Ptr finger;
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

Node_Ptr find_predecessor(Node_Ptr chord, uint32_t key)
{
    Node_Ptr n = chord;
    Node_Ptr successor = chord->finger_table[1];
    // key 大于 n 并且小于等于 n 的后继，所以 n 是 key 的前继
    while (!key_in_right_closed_interval(key, n->key, successor->key)) {
        // n 的后继不是 key 的后继，要继续查找
        n = closest_preceding_finger(n, key);
        successor = n->finger_table[1];
        if (n->key == successor->key){
            return n;
        }
    }
    return n;
}

Node_Ptr find_successor(Node_Ptr chord, uint32_t key) {
    if (key == chord->key){
        return chord;
    }
    Node_Ptr n = find_predecessor(chord, key);
    return n->finger_table[1];
}

void print_node(Node_Ptr node)
{
    __xlogd("node->key >>>>--------------------------> enter %u\n", node->key);
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%d] start_key=%u key=%u\n", 
            i, (node->key + (1 << (i-1))) % KEY_SPACE, node->finger_table[i]->key);
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    __xlogd("node->key >>>>-----------------------> exit %u\n", node->key);
}

void node_print_all(Node_Ptr node)
{
    __xlogd("node_print_all >>>>------------------------------------------------> enter\n");
    Node_Ptr n = node->predecessor;
    while (n != node){
        Node_Ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == node){
            print_node(n);
            break;
        }        
    }   
    __xlogd("node_print_all >>>>------------------------------------------------> exit\n"); 
}

void update_others(Node_Ptr node)
{
    __xlogd("update_others enter\n");
    node_print_all(node);
    uint32_t start;
    Node_Ptr prev, next, finger_node;
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
    node_print_all(node);
    __xlogd("update_others exit\n");
}

Node_Ptr node_create(uint32_t key)
{
    __xlogd("node_create enter\n");

    Node_Ptr node = (Node_Ptr)calloc(1, sizeof(struct Node));

    node->key = key;
    node->predecessor = node;

    for (int i = 0; i < KEY_BITS; i++) {
        node->finger_table[i] = node;
    }

    __xlogd("node_create exit\n");

    return node;
}


Node_Ptr node_join(Node_Ptr ring, uint32_t key)
{
    __xlogd("node_join >>>>--------------------> enter %u\n", key);

    uint32_t start;
    Node_Ptr node = NULL;
    Node_Ptr successor = NULL;

    if (key == ring->key){
        return NULL;
    }

    node = node_create(key);

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

    __xlogd("node_join exit 0x%x\n", node);

    return node;
}

void node_remove(Node_Ptr ring, uint32_t key)
{
    __xlogd("node_remove enter %u\n", key);
    uint32_t start;
    Node_Ptr node, prev, next, finger_node, successor;
    successor = find_successor(ring, key);
    if (successor->key == key){
        node = successor;
        successor = node->finger_table[1];
    }else {
        node = successor->predecessor;
    }
    __xlogd("node_remove key=%u node=%u suc=%u\n", key, node->key, successor->key);
    successor->predecessor = node->predecessor;

    for (int i = 1; i < KEY_BITS; ++i){
        start = (node->key - (1 << (i-1))) % KEY_SPACE;
        __xlogd("node_remove start %u\n", start);
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, start);
        __xlogd("node_remove [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        next = prev->finger_table[1];
        if (next->key == start){
            // start 的位置正好指向一个 node，所以这个 node 的路由表项可能会指向当前 node
            finger_node = next->finger_table[i];
            if (finger_node->key == node->key){
                next->finger_table[i] = successor;
            }
        }
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        finger_node = prev->finger_table[i];
        while (finger_node->key == node->key){
            __xlogd("node_remove prev->finder[%d]->key=%u\n", i, node->key);
            prev->finger_table[i] = successor;
            prev = prev->predecessor;
            finger_node = prev->finger_table[i];
            __xlogd("node_remove [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
                prev->key, node->key, i, prev->finger_table[i]->key);                
        }
    }

    free(node);
    __xlogd("node_remove exit\n");
}



int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/chord/log", NULL);

    struct XChord chord = {0};
    chord.ring = node_create(0);
    Node_Ptr ring = chord.ring;

    uint32_t k = 1;
    Node_Ptr n = NULL, s = NULL;
    for (int i = 1; i < KEY_SPACE; ++i){
        if ((i % k) == 0){
            if (i <= k){
                n = node_join(ring, i);
            }else {
                n = node_join(ring, i);
            }
        }
        if (n != NULL){

        }
    }

    n = find_successor(ring, k);
    s = find_successor(n, n->key + 1);
    node_remove(s, n->key);

    n = find_successor(ring, (k * 2) % KEY_SPACE);
    s = find_successor(n, n->key + 1);
    node_remove(s, n->key);

    for (int i = 0; i < KEY_SPACE; ++i){
        if (i <= k){
            Node_Ptr search_node = find_successor(ring, (i+1)%KEY_SPACE);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_SPACE, search_node->key);
        }else {
            Node_Ptr search_node = find_successor(ring, (i+1)%KEY_SPACE);
            __xlogd("search key %u: fond %u\n", (i+1)%KEY_SPACE, search_node->key);
        }
    }

    n = ring->predecessor;
    while (n != ring){
        Node_Ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == ring){
            print_node(n);
            break;
        }        
    }


    n = ring->predecessor;
    while (n != ring){
        Node_Ptr t = n->predecessor;
        __xlogd("free node n=%u addr=0x%x\n", n->key, n);
        free(n);
        n = t;
        if (n == ring){
            __xlogd("free node n=%u addr=0x%x\n", n->key, n);
            free(n);
            break;
        }        
    }

    xlog_recorder_close();

    return 0;
}