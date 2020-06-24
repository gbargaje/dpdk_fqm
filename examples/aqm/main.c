/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>
#include <stdio.h>
#include <signal.h>

#include <rte_aqm.h>
#include <rte_aqm_algorithm.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

static uint8_t port_a = 0;
static uint8_t port_b = 1;

static uint8_t lcore_ab_rx = 1;
static uint8_t lcore_ab_tx = 2;
static uint8_t lcore_ba_rx = 3;
static uint8_t lcore_ba_tx = 4;
static uint8_t lcore_stats = 5;

static volatile bool force_quit;

#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024

#define MAX_PKT_BURST 32
#define MEMPOOL_CACHE_SIZE 256

#define BURST_TX_DRAIN_US 100
#define STATS_UPDATE_MS 100

static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

struct rte_mempool *aqm_pktmbuf_pool = NULL;

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
static struct rte_mbuf **queue;
static void *aqm_memory;

static struct rte_eth_conf default_port_conf = {
	.rxmode= { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

struct aqm_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
	struct rte_aqm_stats aqm_stats;
} __rte_cache_aligned;

struct port_params {
	uint32_t rate_bytes;
	uint32_t qlen_pkts;
	struct rte_aqm_params aqm_params;
};

static struct port_params default_params = {
	.rate_bytes = 1250000,
	.qlen_pkts = 1024,
	.aqm_params = {
		.algorithm = RTE_AQM_FIFO,
		.algorithm_params = NULL,
	}
};

struct aqm_port_statistics port_statistics[RTE_MAX_ETHPORTS];

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
					signum);
		force_quit = true;
	}
}

static void port_stat(uint8_t portid) {
	/* TODO: Stats Printing */

	return;
}

static void ab_rx(void)
{
	printf("Forward path RX on lcore %u\n", rte_lcore_id());

	uint8_t rx_port;
	uint8_t tx_port;
	uint16_t n_pkts_dropped;
	uint32_t nb_rx;
	uint32_t sent;
	uint32_t i;
	uint32_t enq_cnt;
	uint32_t n_bytes_dropped;

	rx_port = port_a;
	tx_port = port_b;
	enq_cnt = 0;

	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;

	while (!force_quit)
	{
		nb_rx = rte_eth_rx_burst(rx_port, 0, pkts_burst, MAX_PKT_BURST);
	
		if (likely(nb_rx))
		{
			port_statistics[rx_port].rx += nb_rx;

			for (i = 0; i < nb_rx; ++i)
			{
				m = pkts_burst[i];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				enq_cnt += rte_aqm_enqueue(aqm_memory, m);
			}
			for (i = 0; i < nb_rx; ++i)
			{
				m = pkts_burst[i];
				if (rte_aqm_dequeue(aqm_memory, &m, &n_pkts_dropped,
								&n_bytes_dropped) == 0)
				{
					sent = rte_eth_tx_buffer(tx_port, 0, tx_buffer[tx_port], m);
					if (unlikely(sent))
						port_statistics[tx_port].tx += sent;
				}
			}
		}
	}

	return;
}

static void ab_tx(void)
{
	uint8_t tx_port;
	uint32_t sent;
	uint64_t prev_tsc;
	uint64_t diff_tsc;
	uint64_t cur_tsc;
	uint64_t drain_tsc;

	tx_port = port_b;

	prev_tsc = 0;
	drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
					BURST_TX_DRAIN_US;

	while (!force_quit)
	{
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;

		if (unlikely(diff_tsc > drain_tsc))
		{
			sent = rte_eth_tx_buffer_flush(tx_port, 0, tx_buffer[tx_port]);
			if (likely(sent))
				port_statistics[tx_port].tx += sent;
		}
	}

	return;
}

static void ba_rx(void)
{
	printf("Reverse path RX on lcore %u\n", rte_lcore_id());

	uint8_t rx_port;
	uint8_t tx_port;
	uint32_t nb_rx;
	uint32_t sent;
	uint32_t i;

	rx_port = port_b;
	tx_port = port_a;

	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;

	while (!force_quit)
	{
		nb_rx = rte_eth_rx_burst(rx_port, 0, pkts_burst, MAX_PKT_BURST);
		
		if (likely(nb_rx))
		{
			port_statistics[rx_port].rx += nb_rx;

			for (i = 0; i < nb_rx; ++i)
			{
				m = pkts_burst[i];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				sent = rte_eth_tx_buffer(tx_port, 0, tx_buffer[tx_port], m);

				if (unlikely(sent))
					port_statistics[tx_port].tx += sent;
			}
		}
	}

	return;
}

static void ba_tx(void)
{
	printf("Reverse path TX on lcore %u\n", rte_lcore_id());
	
	uint8_t tx_port;
	uint32_t sent;
	uint64_t prev_tsc;
	uint64_t diff_tsc;
	uint64_t cur_tsc;
	uint64_t drain_tsc;

	tx_port = port_a;

	prev_tsc = 0;
	drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
					BURST_TX_DRAIN_US;

	while (!force_quit)
	{
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;

		if (unlikely(diff_tsc > drain_tsc))
		{
			sent = rte_eth_tx_buffer_flush(tx_port, 0, tx_buffer[tx_port]);

			if (likely(sent))
				port_statistics[tx_port].tx += sent;
		}
	}

	return;
}

static void stats(void)
{
	printf("Statistics on lcore %u\n", rte_lcore_id());

	uint64_t prev_tsc;
	uint64_t diff_tsc;
	uint64_t cur_tsc;
	uint64_t stats_tsc;

	prev_tsc = 0;
	stats_tsc = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S *
					STATS_UPDATE_MS;

	while (!force_quit)
	{
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;

		if (unlikely(diff_tsc > stats_tsc))
		{
			port_stat(port_b);
		}
	}

	return;
}

static int assign_jobs(__rte_unused void *arg)
{
	uint8_t lcore_id;

	lcore_id = rte_lcore_id();

	if (lcore_id == lcore_stats)
		stats();
	else if (lcore_id == lcore_ab_rx)
		ab_rx();
	else if (lcore_id == lcore_ab_tx)
		ab_tx();
	else if (lcore_id == lcore_ba_rx)
		ba_rx();
	else if (lcore_id == lcore_ba_tx)
		ba_tx();

	return 0;
}

static int manage_timer(void)
{
	printf("Timer Management on lcore %u\n", rte_lcore_id());

	return 0;
}

static int parse_args(int argc, char **argv)
{
	/* TODO: Options Parsing */

	return 0;
}

static int port_init(uint8_t portid)
{
	int ret = 0;

	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_conf local_port_conf = default_port_conf;
	struct rte_eth_dev_info dev_info;

	ret = rte_eth_dev_info_get(portid, &dev_info);
	if (ret != 0)
		return ret;

	ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						   &nb_txd);
	if (ret < 0)
		return ret;

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					 rte_eth_dev_socket_id(portid),
					 &rxq_conf,
					 aqm_pktmbuf_pool);
	if (ret < 0)
		return ret;

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	
	ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
			rte_eth_dev_socket_id(portid),
			&txq_conf);
	
	if (ret < 0)
		return ret;

	tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
			rte_eth_dev_socket_id(portid));

	if (tx_buffer[portid] == NULL)
		return -1;

	ret = rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);
	if (ret)
		return ret;

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);

	if (ret)
		return ret;

	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		return ret;

	ret = rte_eth_promiscuous_enable(portid);
	if (ret)
		return ret;

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	uint16_t nb_ports;
	uint16_t aqm_memory_size;
	uint16_t queue_memory_size;
	uint32_t nb_lcores;
	uint32_t nb_mbufs;

	nb_ports = 2;
	nb_lcores = 6;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid application arguments\n");

	nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST +
		nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);

	aqm_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());

	if (aqm_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	ret = port_init(port_a);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to initialize port %u\n", port_a);

	ret = port_init(port_b);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to initialize port %u\n", port_b);

	aqm_memory_size = rte_aqm_get_memory_size(default_params.aqm_params.algorithm);

	queue_memory_size = default_params.qlen_pkts * sizeof(struct rte_mbuf *);

	queue = (struct rte_mbuf **) rte_zmalloc_socket("qbase", queue_memory_size,
					RTE_CACHE_LINE_SIZE, rte_eth_dev_socket_id(port_b));

	if (queue == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate queue memory\n");

	aqm_memory = rte_zmalloc_socket("aqm_memory", aqm_memory_size,
					RTE_CACHE_LINE_SIZE, rte_eth_dev_socket_id(port_b));

	if (queue == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate aqm memory\n");

	if (rte_aqm_init(aqm_memory, &default_params.aqm_params, queue,
					default_params.qlen_pkts))
		rte_exit(EXIT_FAILURE, "Failed to initialize AQM\n");

	rte_eal_mp_remote_launch(assign_jobs, NULL, SKIP_MASTER);

	manage_timer();

	rte_eal_mp_wait_lcore();

	rte_aqm_destroy(aqm_memory);

	rte_eth_dev_stop(port_a);
	rte_eth_dev_stop(port_b);

	rte_eth_dev_close(port_a);
	rte_eth_dev_close(port_b);

	printf("Exiting..\n");
	return 0;
}
