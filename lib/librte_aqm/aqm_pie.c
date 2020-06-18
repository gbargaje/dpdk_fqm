
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_pie.h"

struct aqm_pie {
	struct rte_pie_rt pie_rt;
	struct rte_pie_config pie_config;
	struct rte_timer pie_timer;
};

size_t aqm_pie_get_memory_size(void)
{
	return sizeof(struct aqm_pie);
}

int aqm_pie_init(struct aqm_pie *pie, struct rte_aqm_pie_params *params)
{
	struct rte_pie_params *pie_params = &params->params;

	if (rte_pie_config_init(&pie->pie_config,
				pie_params->target_delay,
				pie_params->t_update,
				pie_params->mean_pkt_size,
				pie_params->max_burst,
				pie_params->alpha,
				pie_params->beta)) {
		RTE_LOG(ERR, AQM, "%s: configuration initialization failure\n",
				__func__);
		return -1;
	}

	unsigned int lcore_id = rte_lcore_id();

	rte_timer_subsystem_init();

	rte_timer_reset(&pie->pie_timer, pie->pie_config.t_update, PERIODICAL,
		lcore_id, rte_pie_calc_drop_prob, (void *) pie);

	return 0;
}

int aqm_pie_enqueue(struct aqm_pie *pie, struct circular_queue *cq,
			struct rte_mbuf *pkt)
{
	uint16_t qlen = circular_queue_get_length_pkts(cq);

	if (rte_pie_enqueue(&pie->pie_config, &pie->pie_rt, qlen)) {
		rte_pktmbuf_free(pkt);
		return -1;
	}

	return circular_queue_enqueue(cq, pkt);
}

int aqm_pie_dequeue(struct aqm_pie *pie, struct circular_queue *cq,
			struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			uint32_t *n_bytes_dropped)
{
	int ret = 0;

	ret = circular_queue_dequeue(cq, pkt);
	if (unlikely(ret)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}

	*n_pkts_dropped = 0;
	*n_bytes_dropped = 0;

	return 0;
}

int aqm_pie_get_stats(struct aqm_pie *pie, struct rte_aqm_pie_stats *stats)
{
	return 0;
}

int aqm_pie_destroy(struct aqm_pie *pie)
{
	return 0;
}
