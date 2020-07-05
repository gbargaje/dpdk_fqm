
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_pie.h"

int
aqm_pie_drop(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen);

size_t aqm_pie_get_memory_size(void)
{
	return sizeof(struct aqm_pie);
}

int aqm_pie_init(struct aqm_pie *pie, struct rte_aqm_pie_params *params)
{
	struct rte_pie_params *pie_params = &params->params;
	struct rte_pie_config *config = &pie->pie_config;
	struct rte_pie_rt *pie_rt = &pie->pie_rt;

	if (config == NULL) {
		return -1;
	}
	if (pie_params->target_delay <= 0) {
		return -2;
	}
	if (pie_params->max_burst <= 0) {
		return -3;
	}
	if (pie_params->t_update <= 0) {
		return -4;
	}
	if (pie_params->mean_pkt_size <= 0) {
		pie_params->mean_pkt_size = 2;
	}

	config->target_delay    = pie_params->target_delay * 1000u;
	config->t_update        = pie_params->t_update * 1000u;
	config->alpha           = PIE_SCALE * 0.125;
	config->beta            = PIE_SCALE * 1.25;
	config->mean_pkt_size   = pie_params->mean_pkt_size;
	config->max_burst 	= pie_params->max_burst * 1000u;

	pie_rt->drop_prob = 0;
	pie_rt->burst_allowance = config->max_burst;
	pie_rt->old_qdelay = 0;
	pie_rt->cur_qdelay = 0;
	pie_rt->accu_prob = 0;

	uint64_t t = (rte_get_tsc_hz()+US_PER_S-1)/US_PER_S * pie->pie_config.t_update;
	unsigned int lcore_id = rte_lcore_id();

	rte_timer_subsystem_init();

	rte_timer_reset(&pie->pie_timer, t, PERIODICAL,
		lcore_id, rte_pie_calc_drop_prob, (void *) pie);

	return 0;
}

int 
aqm_pie_drop(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen) 
{
	if((pie_rt->old_qdelay < config->target_delay>>1 \
		&& pie_rt->drop_prob < PIE_MAX_PROB/5) \
		|| (qlen <= 2*config->mean_pkt_size)){
		return PIE_ENQUEUE;
	}

	if(pie_rt->drop_prob == 0) {
		pie_rt->accu_prob = 0;
	}
	pie_rt->accu_prob += pie_rt->drop_prob;

	if(pie_rt->accu_prob < (uint64_t) PIE_MAX_PROB * 17 / 20) {
		return PIE_ENQUEUE;
	}

	if(pie_rt->accu_prob >= (uint64_t) PIE_MAX_PROB * 17 / 2) {
		return PIE_DROP;
	}	

	int64_t random = rte_rand() % PIE_MAX_PROB;

	if (random < pie_rt->drop_prob) {
		pie_rt->accu_prob = 0;
		return PIE_DROP;
	}
	
	return PIE_ENQUEUE;
}

int aqm_pie_enqueue(struct aqm_pie *pie, struct circular_queue *cq,
			struct rte_mbuf *pkt)
{
	uint16_t qlen = circular_queue_get_length_pkts(cq);
	struct rte_pie_config *config = &pie->pie_config;
	struct rte_pie_rt *pie_rt = &pie->pie_rt;

	if (pie_rt->burst_allowance == 0 && aqm_pie_drop(config, pie_rt, qlen) == PIE_DROP) {
		rte_pktmbuf_free(pkt);
		return 1;
	}

	if (pie_rt->drop_prob == 0 \
		&& pie_rt->cur_qdelay < config->target_delay>>1 \
		&& pie_rt->old_qdelay < config->target_delay>>1) {
		pie_rt->burst_allowance = config->max_burst;
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

	pie->pie_rt.cur_qdelay = circular_queue_get_queue_delay(cq);
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
