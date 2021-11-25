#include "io.h"

static inline void
pkt_processing(struct rte_mbuf *pkt, uint16_t port_id, uint16_t queue_id, struct rte_eth_dev_tx_buffer *buffer)
{
    struct rte_ether_hdr *eth_hdr;
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

    // swap mac address
    struct rte_ether_addr tmp_addr = eth_hdr->d_addr;
    eth_hdr->d_addr = eth_hdr->s_addr;
    eth_hdr->s_addr = tmp_addr;
    if (likely(eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
    {
        // swap ip address
        struct rte_ipv4_hdr *ipv4_hdr;
        ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        rte_be32_t tmp_ip = ipv4_hdr->dst_addr;
        ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
        ipv4_hdr->src_addr = tmp_ip;
        if (likely(ipv4_hdr->next_proto_id == IPPROTO_UDP))
        {
            struct rte_udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
            rte_be16_t tmp_port = udp_hdr->dst_port;
            udp_hdr->dst_port = udp_hdr->src_port;
            udp_hdr->src_port = tmp_port;
            udp_hdr->dgram_cksum = 0;
            rte_eth_tx_buffer(port_id, queue_id, buffer, pkt);
            return;
        }

        if (ipv4_hdr->next_proto_id == IPPROTO_TCP)
        {
            struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
            rte_be16_t tmp_port = tcp_hdr->dst_port;
            tcp_hdr->dst_port = tcp_hdr->src_port;
            tcp_hdr->src_port = tmp_port;
            tcp_hdr->cksum = 0;
            rte_eth_tx_buffer(port_id, queue_id, buffer, pkt);
            return;
        }
    }

    rte_pktmbuf_free(pkt);
    return;
}

int lcore_io(struct io_lcore_params *p)
{
    printf("Core %u doing packet RX.\n", rte_lcore_id());
    struct rte_mbuf *pkts[BURST_SIZE];
    struct rte_mbuf *ctrl_pkts[BURST_SIZE_TX];
    struct rte_eth_dev_tx_buffer *tx_buffer;

    // Initialize TX Buffer
    tx_buffer = rte_zmalloc_socket("tx_buffer",
                                   RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE_TX * 2), 0,
                                   rte_eth_dev_socket_id(ETH_PORT_ID));
    if (tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                 ETH_PORT_ID);

    int retval = rte_eth_tx_buffer_init(tx_buffer, BURST_SIZE_TX * 2);
    if (retval < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot set error callback for tx buffer on port %u\n",
                 ETH_PORT_ID);

    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;

    uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
    uint16_t port_id;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
                               BURST_TX_DRAIN_US;
    prev_tsc = 0;
    timer_tsc = 0;

    while (!force_quit)
    {

        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc))
        {
            // drain tx_buffer
            rte_eth_tx_buffer_flush(ETH_PORT_ID, p->tid, tx_buffer);
            prev_tsc = cur_tsc;
        }

        const uint16_t nb_rx = rte_eth_rx_burst(ETH_PORT_ID, p->tid, pkts, BURST_SIZE);
        if (unlikely(nb_rx == 0))
        {
            continue;
        }

        int i;
        /* Prefetch first packets */
        for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        }
        for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET], void *));
            pkt_processing(pkts[i], ETH_PORT_ID, p->tid, tx_buffer);
        }

        /* Process left packets */
        for (; i < nb_rx; i++)
        {
            pkt_processing(pkts[i], ETH_PORT_ID, p->tid, tx_buffer);
        }
    }
    return 0;
}
