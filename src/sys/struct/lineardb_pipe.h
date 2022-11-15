#include "lineardb.h"


typedef struct linear_data_block_pipe {
    uint32_t len;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t reserve_size;
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
        lp->len = 1;
        do {
            lp->len <<= 1;
        } while(len >>= 1);
    }

    // if (lp->len < 1 << 16){
    //     lp->len = (1 << 16);
    // }

    lp->reserve_size = lp->len;

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
        free(lp);
    }
}

static inline uint32_t lineardbPipe_write(LineardbPipe *lp, void *data, uint32_t size)
{
    while ((uint32_t)(lp->len - lp->write_pos + lp->read_pos) >= size) {

        uint32_t ldb_size = lineardb_bind_byte(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);

        if (lp->reserve_size < ldb_size){
            // fprintf(stdout, "write block: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s\n", (char*)block);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->reserve_size;
            // lp->reserve_size = lp->len - (lp->write_pos & (lp->len - 1));
            lp->reserve_size = lp->len;
            // lp->write_block = (Lineardb *)(lp->buf + (lp->write_pos & (lp->len - 1)));
            // ldb_size = lineardb_bind_bytes(lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
        }else {
            // lineardb_load_bytes(lp->write_block, block, size);
            // lp->write_pos += __sizeof_block(lp->write_block);
            memcpy(__byteof_block(lp->write_block), data, size);
            lp->write_pos += ldb_size;
            lp->reserve_size = lp->len - (lp->write_pos & (lp->len - 1));
            // if (lp->write_block->byte == lp->buf){
            //     fprintf(stdout, "write block: >>>>> %s <<<<<\n", __byteof_block(lp->write_block));
            // }
            // lp->write_block = (Lineardb *)(lp->buf + (lp->write_pos & (lp->len - 1)));
            return size;
        }
    }

    return 0;
}

static inline Lineardb* lineardbPipe_hold_block(LineardbPipe *lp)
{
    while ((uint32_t)(lp->write_pos - lp->read_pos) > 0) {

        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
            // fprintf(stdout, "hold block: ##### %s #####\n", __byteof_block(lp->read_block));
        }else {
            // fprintf(stdout, "hold block: @@@@@ %s @@@@@\n", __byteof_block(lp->read_block));
            return lp->read_block;
        }
    }

    return NULL;
}

static inline void lineardbPipe_free_block(LineardbPipe *lp, Lineardb *ldb)
{
    //TODO
    if (ldb == lp->read_block){
        lp->read_pos += (__sizeof_block(lp->read_block));
        lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
    }
}