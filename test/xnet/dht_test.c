#include <xlib/avlmini.h>
#include <xlib/xxhash.h>
#include <xlib/xsha256.h>
#include <xnet/xtable.h>
#include <xnet/uuid.h>

#include <stdio.h>

#define TEST_INTERVAL   256


uuid_node_t* uuid_node_create(uint64_t hash_key, uint64_t *uuid)
{
    uuid_node_t *node = (uuid_node_t*)calloc(1, sizeof(uuid_node_t));
    node->hash_key = hash_key;
    for (size_t i = 0; i < 4; i++){
        node->uuid[i] = uuid[i];
    }
    node->next = node;
    node->prev = node;
    return node;
}

static void test_hash_tree()
{

    char uuid_bin1[32];
    char uuid_bin2[32];
    char uuid_str1[65];
    char uuid_str2[65];

    uuid_node_t *node;
    uuid_list_t tree;
    uuid_list_init(&tree);

    uuid_node_t *nodes[TEST_INTERVAL];

    for (int i = 0; i < TEST_INTERVAL; ++i){

        uuid_generate(uuid_bin1, "HELLO");

        uint64_t h64 = xxhash64(uuid_bin1, 32, 0);

        __xlogd("dth test add node key=%lu %s\n", h64 % 16, uuid2str(uuid_bin1, uuid_str1));
        str2uuid(uuid_str1, uuid_bin2);
        if (mcompare(uuid_bin2, uuid_bin1, 32) != 0){
            __xlogd("uuid to str failed\n");
            exit(0);
        }        

        nodes[i] = uuid_node_create(h64 % 16, uuid_bin1);        

        uuid_list_add(&tree, nodes[i]);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.uuid_tree.count);
    }

    for (int i = TEST_INTERVAL-1; i >= 0; --i){
        node = uuid_list_find(&tree, nodes[i]->hash_key, nodes[i]->uuid);
        __xlogd("find node=%p\n", node);
        __xlogd("find key=%lu uuid=%s\n", nodes[i]->hash_key, uuid2str(nodes[i]->uuid, uuid_str1));
        // if (node == NULL){
        //     node = uuid_list_find_by_hash(&tree, nodes[i]->hash_key);
        //     if (node->list_len > 0){
        //         uuid_node_t *head = node;
        //         do {
        //             __xlogd("find head %lu->(%s)\n", head->hash_key, uuid2str(head->uuid, uuid_str2));
        //             head = head->next;
        //         }while (head != node);
        //     }

        //     node = avl_tree_first(&tree.uuid_tree);
        //     for (uuid_node_t *last = avl_tree_last(&tree.uuid_tree);;node = avl_tree_next(&tree.uuid_tree, node)) {
        //         __xlogd("uuid======%s\n", uuid2str(node->uuid, uuid_str2));
        //         if (node == last){
        //             break;
        //         }
        //     }            
        //     exit(0);
        // }
        __xlogd("find key=%lu ret= %s\n", node->hash_key, uuid2str(node->uuid, uuid_str2));
        if (mcompare(&node->uuid[0], &nodes[i]->uuid[0], 32) != 0){
            __xlogd("find uuid=%s ret=%s\n", uuid2str(nodes[i]->uuid, uuid_str1), uuid2str(node->uuid, uuid_str2));
            exit(0);
        }
    }    


    node = uuid_list_first(&tree);
    for (uuid_node_t *last = uuid_list_last(&tree);;node = uuid_list_next(&tree, node)) {
        uuid_node_t *ret = uuid_list_find(&tree, node->hash_key, node->uuid);
        __xlogd("find uuid=%s ret=%s\n", uuid2str(node->uuid, uuid_str1), uuid2str(ret->uuid, uuid_str2));
        __xlogd("list len %lu\n", ret->list_len);
        if (mcompare(node->uuid, ret->uuid, 32) != 0){
            __xlogd("find uuid=%s ret=%s\n", uuid2str(node->uuid, uuid_str1), uuid2str(ret->uuid, uuid_str2));
            exit(0);
        }             
        if (ret->list_len > 0){
            uuid_node_t *head = ret;
            do {
                __xlogd("head %lu->(%s)\n", head->hash_key, uuid2str(head->uuid, uuid_str1));
                head = head->next;
            }while (head != ret);
        }
        if (node == last){
            break;
        }
    }
    

    for (int i = TEST_INTERVAL-1; i >= 0; --i){
        node = uuid_list_del(&tree, nodes[i]);
        // __xlogd("remove %lu->(%s) %lu->(%s)\n", nodes[i]->hash, nodes[i]->key, node->hash, node->key);
        __xlogd("table count %lu hash tree count %lu origin tree count %lu\n",
                tree.count, tree.hash_tree.count, tree.uuid_tree.count);
        if (mcompare(node->uuid, nodes[i]->uuid, 32) != 0){
            __xlogd("remove uuid=%s ret=%s\n", uuid2str(nodes[i]->uuid, uuid_str1), uuid2str(node->uuid, uuid_str2));
            exit(0);
        }
        // node = nodes[i];
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