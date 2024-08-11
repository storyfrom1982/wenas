#include <sys/struct/xavlmini.h>
#include <sys/struct/xhash64.h>
#include <sys/struct/xsha256.h>
#include <sys/struct/xtable.h>

#include <stdio.h>

#define TEST_INTERVAL   256


static void test_hashtree()
{
    char text[1024] = {0};

    uint8_t sha256[32];
    SHA256_CTX shactx;

    xnode_t *node;
    xtree_table_t tree;
    xtree_table_init(&tree);

    xnode_t *nodes[TEST_INTERVAL];

    for (int i = 0; i < TEST_INTERVAL; ++i){
        uint64_t millisecond = __xapi->time();
        // __xlogi("c time %lu\n", millisecond);
        __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);
        // __xlogi("c time %s\n", text);

        millisecond = __xapi->clock();
        // __xlogi("c clock %lu\n", millisecond);
        __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);
        // __xlogi("c clock %s\n", text);

        int n = snprintf(text, 1024, "%lu%lu%s", __xapi->time(), __xapi->clock(), "Hello");

        sha256_init(&shactx);
        sha256_update(&shactx, text, n);
        sha256_finish(&shactx, sha256);

        uint64_t h64 = xhash64(sha256, 32, 0);

        nodes[i] = xnode_create(h64 % 16, sha256, 32);

        __xlogd("dth test add node\n");
        xtree_table_add(&tree, nodes[i]);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.origin_tree.count);
    }

    node = xtree_table_first(&tree);
    xnode_t *last = xtree_table_last(&tree);
    __xlogd("first %lu->(%s) last %lu->(%s)\n", node->hash, last->key, last->hash, last->key);

    
    // do {
    //     xnode_t *ret = xtree_table_find(&tree, node->hash, node->key, node->key_len);
    //     __xlogd("fond %lu->(%s)=%lu->(%s)\n", node->hash, node->key, ret->hash, ret->key);
    //     __xlogd("list len %lu\n", ret->list_len);
    //     if (ret->list_len > 0){
    //         xnode_t *head = ret->next;
    //         while (head != ret)
    //         {
    //             __xlogd("head %lu->(%s)\n", head->hash, head->key);
    //             head = head->next;
    //         }
            
    //     }
    //     node = xtree_table_next(&tree, node);
    // }while (node != last);
    

    for (int i = TEST_INTERVAL-1; i >= 0; --i){
        node = xtree_table_remove(&tree, nodes[i]);
        __xlogd("remove %lu->(%s) %lu->(%s)\n", nodes[i]->hash, nodes[i]->key, node->hash, node->key);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.origin_tree.count);
        free(node->key);
        free(node);
    }
}

char * rand_str(int in_len) 
{ 
  char *__r = (char *)malloc(in_len + 1); 

  int i; 

  if (__r == 0) 
  { 
    return 0; 
  } 

  srand(time(NULL) + rand());    
  for (i = 0; i  < in_len; i++) 
  { 
    __r[i] = rand()%94+32;
  } 

  __r[i] = 0; 

  return __r; 
}

static void test_htable()
{
    char text[1024] = {0};

    uint8_t sha256[32];
    SHA256_CTX shactx;

    xhash_table_t tree;
    xhash_table_init(&tree, 16);

    char *nodes[TEST_INTERVAL];

    for (int i = 0; i < TEST_INTERVAL; ++i){

        uint8_t str_len = rand() % 16;
        str_len = str_len == 0 ? 10 : str_len;
        nodes[i] = rand_str(str_len);

        // __xlogd("dth test add str=%s\n", nodes[i]);
        xhash_table_add(&tree, nodes[i], nodes[i]);
        // __xlogd("table count %lu origin tree count %lu\n", tree.count, tree.tree.count);
    }

    for (int i = 0; i < TEST_INTERVAL; ++i){
        char *str = xhash_table_find(&tree, nodes[i]);
        // __xlogd("dth test find str:  >>>>------------------>   (%s)    =   (%s)\n", nodes[i], str);
        if (mcompare(str, nodes[i], slength(str)) != 0){
            __xlogd("dth test find str:  >>>>------------------>   (%s)    =   (%s)\n", nodes[i], str);
            exit(0);
        }
    }

    for (int i = 0; i < TEST_INTERVAL; ++i){
        char *str = xhash_table_del(&tree, nodes[i]);
        __xlogd("dth test delete str:  >>>>------------------>   (%s)    =   (%s)\n", nodes[i], str);
        __xlogd("table count %lu origin tree count %lu\n", tree.count, tree.tree.count);
        if (str != NULL){
            if (mcompare(str, nodes[i], slength(str)) != 0){
                __xlogd("dth test delete str:  >>>>------------------>   (%s)    =   (%s)\n", nodes[i], str);
                exit(0);
            }            
        }
    }

    for (int i = 0; i < TEST_INTERVAL; ++i){
        free(nodes[i]);
    }    

    xhash_table_clear(&tree);
}

int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/xpeer/log", NULL);

    // test_hashtree();
    test_htable();

    xlog_recorder_close();

    return 0;
}