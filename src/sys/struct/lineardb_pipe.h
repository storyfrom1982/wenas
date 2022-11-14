#include "lineardb.h"


typedef struct linear_data_block_pipe {
    uint32_t len;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t reserve_size;
    Lineardb *ldb_pos;
    char *buf;
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

    if (lp->len < 1 << 16){
        lp->len = (1 << 16);
    }

    lp->reserve_size = lp->len;

    lp->buf = (char *)malloc(lp->len);
    if (lp->buf == NULL){
        free(lp);
        return NULL;
    }

    lp->ldb_pos = (Lineardb*)lp->buf;
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

static inline uint32_t lineardbPipe_write(LineardbPipe *lp, void *block, uint32_t size)
{
    if (lp->reserve_size < (uint32_t)(size + BLOCK_HEAD) && lp->reserve_size != size){
        lineardb_bind_bytes(lp->ldb_pos, lp->buf + (lp->write_pos & (lp->len - 1)), lp->reserve_size);
        lp->ldb_pos->byte[0] = 0;
        lp->write_pos += lp->reserve_size;
        lp->reserve_size = lp->len - (lp->write_pos & (lp->len - 1));
    }

    if ((uint32_t)(lp->len - lp->write_pos + lp->read_pos) < size) {
        return 0;
    }

    lp->ldb_pos = (Lineardb *)(lp->buf + (lp->write_pos & (lp->len - 1)));
    lineardb_copy_bytes(lp->ldb_pos, block, size);
    lp->write_pos += size;
    lp->reserve_size = lp->len - (lp->write_pos & (lp->len - 1));

    return size;
}