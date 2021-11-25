#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include "portinit.h"
#include "io.h"
#include "cli_parser.h"



int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;

    config.first_lcore = 36;
    config.num_service_core = 8;
    struct io_lcore_params lp[MAX_SERVICE_CORE];

    int retval = rte_eal_init(argc, argv);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "initialize fail!");

    retval = zecho_args_parser(argc, argv,&config);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "Invalid arguments\n");
    

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 4,
                                        MBUF_CACHE_SIZE, 8,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize eth port */
    if (port_init(ETH_PORT_ID, mbuf_pool, config.num_service_core, config.num_service_core) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", ETH_PORT_ID);
    
    printf("system init finished, starting service process...\n");

    unsigned int lcore_num = config.first_lcore;
    /* Start IO-RX process */   
    for (int i = 0; i < config.num_service_core; ++i)
    {
        lp[i].mem_pool = mbuf_pool;
        lp[i].tid = i;
        rte_eal_remote_launch((lcore_function_t *)lcore_io, &lp[i], lcore_num++);
    }

    rte_eal_wait_lcore(config.first_lcore);
    return 0;
}

