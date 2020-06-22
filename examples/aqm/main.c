/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_aqm.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>

static uint8_t port_a = 0;
static uint8_t port_b = 1;

static uint8_t lcore_ab_rx = 1;
static uint8_t lcore_ab_tx = 2;
static uint8_t lcore_ba_rx = 3;
static uint8_t lcore_ba_tx = 4;
static uint8_t lcore_stats = 5;

static void ab_rx(void)
{
	printf("Forward path RX on lcore %u\n", rte_lcore_id());

	return;
}

static void ab_tx(void)
{
	printf("Forward path TX on lcore %u\n", rte_lcore_id());

	return;
}

static void ba_rx(void)
{
	printf("Reverse path RX on lcore %u\n", rte_lcore_id());

	return;
}

static void ba_tx(void)
{
	printf("Reverse path TX on lcore %u\n", rte_lcore_id());

	return;
}

static void stats(void)
{
	printf("Statistics on lcore %u\n", rte_lcore_id());

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

int main(int argc, char **argv)
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid application arguments\n");

	rte_eal_mp_remote_launch(assign_jobs, NULL, SKIP_MASTER);

	manage_timer();

	rte_eal_mp_wait_lcore();

	return 0;
}
