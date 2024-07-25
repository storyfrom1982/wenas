#include <sys/struct/xavlmini.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>

#include <stdio.h>

struct tree_node {
    struct avl_node node;
    uint64_t key;
};

static inline int avl_node_compare(const void *n1, const void *n2)
{
	struct tree_node *x = (struct tree_node*)n1;
	struct tree_node *y = (struct tree_node*)n2;
	return x->key - y->key;
}

int main(int argc, char *argv[])
{
    char text[1024] = {0};

    struct avl_tree tree;

    avl_tree_init(&tree, avl_node_compare, sizeof(struct tree_node), AVL_OFFSET(struct tree_node, node));

    uint64_t millisecond = __xapi->time();
    __xlogi("c time %lu\n", millisecond);
    __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);
    __xlogi("c time %s\n", text);

    millisecond = __xapi->clock();
    __xlogi("c clock %lu\n", millisecond);
    __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);
    __xlogi("c clock %s\n", text);

    uint64_t uptime = millisecond / NANO_SECONDS;
    int days = uptime / (60 * 60 * 24);
    int hours = (uptime / (60 * 60)) % 24;
    int minutes = (uptime / 60) % 60;
    int seconds = uptime % 60;

    printf("系统已运行时间: %d 天, %d 小时, %d 分钟, %d 秒\n", days, hours, minutes, seconds);

    int n = snprintf(text, 1024, "%lu%lu%s", __xapi->time(), __xapi->clock(), "Hello");

    uint8_t sha256[32];
    SHA256_CTX shactx;
    sha256_init(&shactx);
    sha256_update(&shactx, text, n);
    sha256_finish(&shactx, sha256);

    uint64_t h64 = xhash64(sha256, 32, 0);

    __xlogd("hash first  %lu\n", h64);

    struct tree_node node, node1;
    node.key = h64;
    avl_tree_add(&tree, &node);


    sha256_init(&shactx);
    // text[n-1] = '\h';
    sha256_update(&shactx, text, n);
    sha256_finish(&shactx, sha256);

    h64 = xhash64(sha256, 32, 0);

    __xlogd("hash second %lu\n", h64);

    node1.key = h64;
    struct tree_node *fnode = (struct tree_node*) avl_tree_find(&tree, &node1);

    if (fnode){
        __xlogd("fond node   %lu\n", fnode->key);
    }else {
        __xlogd("cannot find node   %lu\n", node1.key);
    }
    

    return 0;
}