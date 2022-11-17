#ifndef __LINEAR_KV__
#define __LINEAR_KV__

#include "lineardb.h"

typedef struct linear_key {
    uint8_t byte[8];
}LKey;

typedef struct linear_key_value {
    uint32_t size, pos;
    Lineardb *value;
    LKey *head, *key;
}Linearkv;

static inline void linearkv_bind_buffer(Linearkv *lkv, uint8_t *buf, uint32_t size)
{
    lkv->pos = 0;
    lkv->size = size;
    lkv->head = (LKey*)buf;
    lkv->value = NULL;
}

static inline void linearkv_load_lineardb(Linearkv *lkv, Lineardb *ldb)
{
    lkv->pos = __sizeof_data(ldb);
    memcpy(lkv->head, __dataof_block(ldb), lkv->pos);
}

static inline Linearkv* linearkv_create(uint32_t capacity)
{
    Linearkv *lkv = (Linearkv *)malloc(sizeof(Linearkv));
    uint8_t *buf = (uint8_t *)malloc(capacity);
    linearkv_bind_buffer(lkv, buf, capacity);
    return lkv;
}

static inline void linearkv_release(Linearkv **pp_lkv)
{
    if (pp_lkv && *pp_lkv){
        Linearkv *lkv = *pp_lkv;
        *pp_lkv = NULL;
        free(lkv->head);
        free(lkv);
    }
}

static inline void linearkv_append_number(Linearkv *lkv, char *key, Lineardb ldb)
{
    lkv->key = (LKey *)(lkv->head->byte + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->value = ldb;
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void linearkv_append_string(Linearkv *lkv, char *key, char *value)
{
    lkv->key = (LKey *)(lkv->head->byte + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    // fprintf(stdout, "strlen(key)=%lu\n", lkv->key->byte[0]);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_load_string(lkv->value, value);
    lkv->pos += __sizeof_block(lkv->value);
}

static inline Lineardb* linearkv_find(Linearkv *lkv, char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < lkv->pos) {
        lkv->key = (LKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        // fprintf(stdout, "strlen(key)=%lu byte[0]=%lu\n", strlen(key), lkv->key->byte[0]);
        // fprintf(stdout, "memcmp()=%d\n", strncmp(&lkv->key->byte[1], key, lkv->key->byte[0]));
        // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
        find_pos += __sizeof_block(lkv->value);
    }
    return NULL;
}

static inline Lineardb* linearkv_find_after(Linearkv *lkv, Lineardb *position, char *key)
{
    uint32_t find_pos = ((position->byte) - (lkv->head->byte) + __sizeof_block(position));
    // fprintf(stdout, "find pos = %u head pos = %u\n", find_pos, lkv->pos);
    while (find_pos < lkv->pos) {
        lkv->key = (LKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
        find_pos += __sizeof_block(lkv->value);
    }

    // fprintf(stdout, "find from head =======================>>>>>>>>>>>>>>>>\n");
    // int i = 0;

    find_pos = 0;
    do {
        lkv->key = (LKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        find_pos += __sizeof_block(lkv->value);
        // fprintf(stdout, "find count=%d\n", i++);
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
    } while (lkv->value != position);
    

    return NULL;
}

typedef struct linear_key_value_builder {
    uint32_t len, pos;
    LKey *key;
    Lineardb *value;
    uint8_t head[10240];
}lkv_builder_t, *lkv_parser_t;


static inline void lkv_builder_clear(lkv_builder_t *builder)
{
    builder->len = 10240;
    builder->pos = 0;
    builder->key = NULL;
    builder->value = NULL;
}

static inline void lkv_load_lineardb(lkv_parser_t parser, Lineardb *ldb)
{
    parser->pos = __sizeof_data(ldb);
    memcpy(parser->head, __dataof_block(ldb), parser->pos);
}

static inline void lkv_add_string(lkv_builder_t *lkv, char *key, char *value)
{
    lkv->key = (LKey *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    // fprintf(stdout, "strlen(key)=%lu\n", lkv->key->byte[0]);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_load_string(lkv->value, value);
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_number(lkv_builder_t *lkv, char *key, Lineardb ldb)
{
    lkv->key = (LKey *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->value = ldb;
    lkv->pos += __sizeof_block(lkv->value);
}

static inline Lineardb* lkv_find(lkv_parser_t parser, char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < parser->pos) {
        parser->key = (LKey *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
        // fprintf(stdout, "strlen(key)=%lu byte[0]=%lu\n", strlen(key), lkv->key->byte[0]);
        // fprintf(stdout, "memcmp()=%d\n", strncmp(&lkv->key->byte[1], key, lkv->key->byte[0]));
        // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->value;
        }
        find_pos += __sizeof_block(parser->value);
    }
    return NULL;
}

static inline Lineardb* lkv_find_after(lkv_parser_t parser, Lineardb *position, char *key)
{
    uint32_t find_pos = ((position->byte) - (parser->head) + __sizeof_block(position));
    // fprintf(stdout, "find pos = %u head pos = %u\n", find_pos, lkv->pos);
    while (find_pos < parser->pos) {
        parser->key = (LKey *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->value;
        }
        find_pos += __sizeof_block(parser->value);
    }

    // fprintf(stdout, "find from head =======================>>>>>>>>>>>>>>>>\n");
    // int i = 0;

    find_pos = 0;
    do {
        parser->key = (LKey *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
        find_pos += __sizeof_block(parser->value);
        // fprintf(stdout, "find count=%d\n", i++);
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->value;
        }
    } while (parser->value != position);
    

    return NULL;
}

#endif //__LINEAR_KV__