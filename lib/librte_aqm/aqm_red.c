/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_branch_prediction.h>
#include <rte_cycles.h> /* TODO: Check if required for time calculation */
#include <rte_log.h>
#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"
#include "rte_red.h"

#include "aqm_red.h"

struct aqm_red {
	struct rte_red_config config;
	struct rte_red rt_data;
};

size_t aqm_red_get_memory_size(void)
{
	return sizeof(struct aqm_red);
}

int aqm_red_init(struct aqm_red *red, struct rte_aqm_red_params *params)
{
	if (rte_red_config_init(&red->config, params->params.wq_log2,
				params->params.min_th, params->params.max_th,
				params->params.maxp_inv) != 0) {
		RTE_LOG(ERR, AQM, "%s: configuration initialization failure\n",
			__func__);
		return -1;
	}

	if (rte_red_rt_data_init(&red->rt_data) != 0) {
		RTE_LOG(ERR, AQM, "%s: run-time data initialization failure\n",
			__func__);
		return -1;
	}

	return 0;
}

int aqm_red_enqueue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf *pkt)
{
	uint16_t qlen;

	qlen = circular_queue_get_length_pkts(cq);

	/* TODO: Correct Argument for time? */
	if (rte_red_enqueue(&red->config, &red->rt_data, qlen,
			    rte_get_tsc_cycles()) != 0) {
		rte_pktmbuf_free(pkt);
		return 1;
	}

	return circular_queue_enqueue(cq, pkt);
}

int aqm_red_dequeue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		    uint32_t *n_bytes_dropped)
{
	int ret;

	ret = circular_queue_dequeue(cq, pkt);
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}

	*n_pkts_dropped = 0;
	*n_bytes_dropped = 0;

	if (circular_queue_is_empty(cq)) {
		/* TODO: Correct Argument for time? */
		rte_red_mark_queue_empty(&red->rt_data, rte_get_tsc_cycles());
	}

	return 0;
}

int aqm_red_get_stats(struct aqm_red *red, struct rte_aqm_red_stats *stats)
{
	/* TODO: add stats for RED, If not required then mark parameters
	 *       with __rte_unused
	 */

	return 0;
}

int aqm_red_destroy(struct aqm_red *red)
{
	/* TODO: destroy RED run-time data. If not required then mark
	 * parameters with __rte_unused
	 */

	return 0;
}
