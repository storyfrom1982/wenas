#include "lineardb.h"


typedef struct linear_data_block_pipeline {
    uint32_t len;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t leftover;
    uint32_t block_count;
    linedb_t *read_block;
    linedb_t *write_block;
    uint8_t *buf;
}linedb_pipe_t;


static inline linedb_pipe_t* linedb_pipe_build(uint32_t len)
{
    linedb_pipe_t *lp = (linedb_pipe_t *)malloc(sizeof(linedb_pipe_t));
    if (lp == NULL){
        return NULL;
    }

   if ((len & (len - 1)) == 0){
        lp->len = len;
    }else{
        lp->len = 1U;
        do {
            lp->len <<= 1;
        } while(len >>= 1);
    }

    // if (lp->len < (1U << 16)){
    //     lp->len = (1U << 16);
    // }

    lp->leftover = lp->len;

    lp->buf = (uint8_t *)malloc(lp->len);
    if (lp->buf == NULL){
        free(lp);
        return NULL;
    }

    lp->read_block = lp->write_block = (linedb_t*)lp->buf;
    lp->read_pos = lp->write_pos = 0;

    return lp;
}

static inline void linedb_pipe_destroy(linedb_pipe_t **pp_lp)
{
    if (pp_lp && *pp_lp){
        linedb_pipe_t *lp = *pp_lp;
        *pp_lp = NULL;
        free(lp->buf);
        free(lp);
    }
}

static inline uint32_t linedb_pipe_write(linedb_pipe_t *lp, void *data, uint32_t size)
{
    uint32_t ldb_size = __plan_sizeof_linedb(size); 
    while ((uint32_t)(lp->len - lp->write_pos + lp->read_pos) >= ldb_size) {

        if (lp->leftover < ldb_size){
            ldb_size = linedb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->leftover;
            lp->leftover = lp->len;
        }else {
            ldb_size = linedb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            memcpy(__dataof_linedb(lp->write_block), data, size);
            lp->write_pos += ldb_size;
            lp->leftover = lp->len - (lp->write_pos & (lp->len - 1));
            lp->block_count++;
            return size;
        }
    }

    return 0;
}

static inline uint32_t linedb_pipe_read(linedb_pipe_t *lp, uint8_t *buf, uint32_t size)
{
    while (((uint32_t)(lp->write_pos - lp->read_pos)) > 0) {
        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        }else {
            uint32_t data_size = __sizeof_data(lp->read_block);
            if (size >= data_size){
                memcpy(buf, __dataof_linedb(lp->read_block), data_size);
                lp->read_pos += (data_size + __sizeof_head(lp->read_block));
                lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
                lp->block_count--;
            }
            return data_size;
        }
    }

    return 0;
}

static inline linedb_t* linedb_pipe_hold_block(linedb_pipe_t *lp)
{
    while (((uint32_t)(lp->write_pos - lp->read_pos)) > 0) {
        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        }else {
            return lp->read_block;
        }
    }

    return NULL;
}

static inline void linedb_pipe_free_block(linedb_pipe_t *lp, linedb_t *ldb)
{
    if (ldb == lp->read_block){
        lp->read_pos += (__sizeof_linedb(lp->read_block));
        lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        lp->block_count--;
    }
}

static inline uint32_t linedb_pipe_readable(linedb_pipe_t *lp)
{
    return ((uint32_t)(lp->write_pos - lp->read_pos));
}

static inline uint32_t linedb_pipe_writable(linedb_pipe_t *lp)
{
    return ((uint32_t)(lp->len - lp->write_pos + lp->read_pos));
}

static inline uint32_t linedb_pipe_block_count(linedb_pipe_t *lp)
{
    return lp->block_count;
}