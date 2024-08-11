#include <xlib/avlmini.h>
#include <xlib/xxhash.h>
#include <xlib/xsha256.h>
#include <xnet/xtable.h>

#include <stdio.h>

#define TEST_INTERVAL   256


static void test_hash_tree()
{
    char text[1024] = {0};

    uint8_t sha256[32];
    SHA256_CTX shactx;

    xhtnode_t *node;
    xhash_tree_t tree;
    xhash_tree_init(&tree);

    xhtnode_t *nodes[TEST_INTERVAL];

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

        uint64_t h64 = xxhash64(sha256, 32, 0);

        nodes[i] = xhtnode_create(h64 % 16, sha256, 32);

        __xlogd("dth test add node\n");
        xhash_tree_add(&tree, nodes[i]);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.uuid_tree.count);
    }

    for (int i = TEST_INTERVAL-1; i >= 0; --i){
        node = xhash_tree_find(&tree, nodes[i]->key, nodes[i]->uuid, nodes[i]->uuid_len);
        // __xlogd("find %lu->(%s) %lu->(%s)\n", nodes[i]->hash, nodes[i]->key, node->hash, node->key);
        if (mcompare(node->uuid, nodes[i]->uuid, node->uuid_len) != 0){
            __xlogd("find %lu->(%s) %lu->(%s)\n", nodes[i]->key, nodes[i]->uuid, node->key, node->uuid);
            exit(0);
        }
    }    


    node = xhash_tree_first(&tree);
    for (xhtnode_t *last = xhash_tree_last(&tree);;node = xhash_tree_next(&tree, node)) {
        xhtnode_t *ret = xhash_tree_find(&tree, node->key, node->uuid, node->uuid_len);
        __xlogd("fond %lu->(%s)=%lu->(%s)\n", node->key, node->uuid, ret->key, ret->uuid);
        __xlogd("list len %lu\n", ret->list_len);
        if (mcompare(node->uuid, ret->uuid, node->uuid_len) != 0){
            __xlogd("find %lu->(%s) %lu->(%s)\n", ret->key, ret->uuid, node->key, node->uuid);
            exit(0);
        }             
        if (ret->list_len > 0){
            xhtnode_t *head = ret;
            do {
                __xlogd("head %lu->(%s)\n", head->key, head->uuid);
                head = head->next;
            }while (head != ret);
        }
        if (node == last){
            break;
        }
    }
    

    for (int i = TEST_INTERVAL-1; i >= 0; --i){
        node = xhash_tree_del(&tree, nodes[i]);
        // __xlogd("remove %lu->(%s) %lu->(%s)\n", nodes[i]->hash, nodes[i]->key, node->hash, node->key);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.uuid_tree.count);
        if (mcompare(node->uuid, nodes[i]->uuid, node->uuid_len) != 0){
            __xlogd("remove %lu->(%s) %lu->(%s)\n", nodes[i]->key, nodes[i]->uuid, node->key, node->uuid);
            exit(0);
        }
        // node = nodes[i];
        free(node->uuid);
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

static void test_hash_table()
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

    test_hash_tree();
    // test_hash_table();

    xlog_recorder_close();

    return 0;
}