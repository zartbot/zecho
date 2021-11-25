#include "common.h"

struct io_lcore_params
{
    struct rte_mempool *mem_pool;
    uint16_t tid;    
};

int lcore_io(struct io_lcore_params *p);

