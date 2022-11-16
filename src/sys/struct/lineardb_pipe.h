#include "lineardb.h"


typedef struct linear_data_block_pipe {
    uint32_t len;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t leftover;
    Lineardb *read_block;
    Lineardb *write_block;
    uint8_t *buf;
}LineardbPipe;


static inline LineardbPipe* lineardbPipe_create(uint32_t len)
{
    LineardbPipe *lp = (LineardbPipe *)malloc(sizeof(LineardbPipe));
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

    if (lp->len < (1U << 16)){
        lp->len = (1U << 16);
    }

    lp->leftover = lp->len;

    lp->buf = (uint8_t *)malloc(lp->len);
    if (lp->buf == NULL){
        free(lp);
        return NULL;
    }

    lp->read_block = lp->write_block = (Lineardb*)lp->buf;
    lp->read_pos = lp->write_pos = 0;

    return lp;
}

static inline void lineardbPipe_release(LineardbPipe **pp_lp)
{
    if (pp_lp && *pp_lp){
        LineardbPipe *lp = *pp_lp;
        *pp_lp = NULL;
        free(lp->buf);
        free(lp);
    }
}

static inline uint32_t lineardbPipe_write(LineardbPipe *lp, void *data, uint32_t size)
{
    uint32_t ldb_size = __plan_sizeof_block(size); 
    while ((uint32_t)(lp->len - lp->write_pos + lp->read_pos) >= ldb_size) {

        if (lp->leftover < ldb_size){
            ldb_size = lineardb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->leftover;
            lp->leftover = lp->len;
        }else {
            ldb_size = lineardb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            memcpy(__dataof_block(lp->write_block), data, size);
            lp->write_pos += ldb_size;
            lp->leftover = lp->len - (lp->write_pos & (lp->len - 1));
            return size;
        }
    }

    return 0;
}

static inline Lineardb* lineardbPipe_hold_block(LineardbPipe *lp)
{
    while (((uint32_t)(lp->write_pos - lp->read_pos)) > 0) {
        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        }else {
            return lp->read_block;
        }
    }

    return NULL;
}

static inline void lineardbPipe_free_block(LineardbPipe *lp, Lineardb *ldb)
{
    if (ldb == lp->read_block){
        lp->read_pos += (__sizeof_block(lp->read_block));
        lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
    }
}