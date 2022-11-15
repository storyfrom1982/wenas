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
        lp->len = 1;
        do {
            lp->len <<= 1;
        } while(len >>= 1);
    }

    // if (lp->len < 1 << 16){
    //     lp->len = (1 << 16);
    // }

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
        free(lp);
    }
}

static inline uint32_t lineardbPipe_write(LineardbPipe *lp, void *data, uint32_t size)
{
    uint32_t ldb_size = size < 0x100 ? (2 + size) : size < 0x10000 ? (3 + size) : (5 + size); 
    // uint32_t ldb_size = lineardb_bind_address(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
    // fprintf(stdout, "writable ====%u data size %u\n", (lp->len - lp->write_pos + lp->read_pos), ldb_size);
    while ((uint32_t)(lp->len - lp->write_pos + lp->read_pos) >= ldb_size) {

        if (lp->leftover < ldb_size){
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %u %p\n", lp->leftover, &lp->write_block->byte[0]);
            // fprintf(stdout, "write block: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s\n", (char*)data);
            ldb_size = lineardb_bind_address(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->leftover;
            lp->leftover = lp->len;
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>## %u %p %p\n", 
            //     lp->write_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>$$ %u %p %p\n", 
            //     lp->read_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);
            // // ldb_size = lineardb_bind_address(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>@@ %u %p %p\n", 
            //     lp->read_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);
        }else {
            ldb_size = lineardb_bind_address(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            // fprintf(stdout, "write: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>## %u %p\n", lp->leftover, &lp->write_block->byte[0]);
            memcpy(__dataof_block(lp->write_block), data, size);
            // fprintf(stdout, "write: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %u type %u\n", __sizeof_block(lp->write_block), lp->write_block->byte[0]);
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>?? %u %p %p\n", 
            //     lp->read_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);
            lp->write_pos += ldb_size;
            lp->leftover = lp->len - (lp->write_pos & (lp->len - 1));
            if (lp->write_block->byte == lp->buf){
                // fprintf(stdout, "write block: >>>>> %s <<<<<============\n", __dataof_block(lp->write_block));
            }
            // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>&& %u %p %p\n", 
            //     lp->read_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);    
            // fprintf(stdout, "write pos ====%u read pos %u\n", lp->write_pos, lp->read_pos);
            return size;
        }
    }

    // fprintf(stdout, "leftover: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>** %u %p %p\n", 
    //     lp->read_block->byte[0], &lp->write_block->byte[0], &lp->read_block->byte[0]);

    return 0;
}

static inline Lineardb* lineardbPipe_hold_block(LineardbPipe *lp)
{
    // if (lp->read_pos < 5800)
    //     fprintf(stdout, "hold block write_pos %u read_pos %u readable == %u\n",lp->write_pos, lp->read_pos, ((uint32_t)(lp->write_pos - lp->read_pos)));
    while (((uint32_t)(lp->write_pos - lp->read_pos)) > 0) {
        lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        // if (lp->read_pos < 5800)
        //     fprintf(stdout, "read block byte[0] = %u = %p\n", lp->read_block->byte[0], &lp->read_block->byte[0]);
        if (lp->read_block->byte[0] == 0){
            // if (lp->read_pos < 5800)
            //     fprintf(stdout, "skip size >>>>>>>>>>>>>>>>>>>>>>> %u\n", ((lp->buf + lp->len) - lp->read_block->byte));
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
            // if (lp->read_pos < 5800)
            //     fprintf(stdout, "write pos >>>>>>>>>>>>>>>>>>>>>>> %u read pos %u\n", lp->write_pos, lp->read_pos);  
            // fprintf(stdout, "hold block: ##### %s #####\n", __byteof_block(lp->read_block));
            // fprintf(stdout, "write_pos %u read_pos %u readable == %u\n",lp->write_pos, lp->read_pos, ((uint32_t)(lp->write_pos - lp->read_pos)));
        }else {
            // fprintf(stdout, "hold block: @@@@@ %s @@@@@\n", __byteof_block(lp->read_block));
            // if (lp->read_pos < 5800)
            //     fprintf(stdout, "read block size ###>>>>>>>>>>>>>>>>>>>>>>> %u\n", __sizeof_block(lp->read_block));
            return lp->read_block;
        }
    }

    return NULL;
}

static inline void lineardbPipe_free_block(LineardbPipe *lp, Lineardb *ldb)
{
    //TODO check
    if (ldb == lp->read_block){
        // if (lp->read_pos < 5800){
        //     fprintf(stdout, "lineardbPipe_free_block size == %u type=%u\n", (__sizeof_block(lp->read_block)), lp->read_block->byte[0]);
        // }
        lp->read_pos += (__sizeof_block(lp->read_block));
        lp->read_block = (Lineardb *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        // if (lp->read_pos < 5800){
        //     fprintf(stdout, "lineardbPipe_free_block size == %u type=%u %p\n", (__sizeof_block(lp->read_block)), lp->read_block->byte[0], &lp->read_block->byte[0]);
        // }
    }else {
        // fprintf(stdout, "lineardbPipe_free_block == %u\n", (uint32_t)(lp->write_pos - lp->read_pos));
    }
}