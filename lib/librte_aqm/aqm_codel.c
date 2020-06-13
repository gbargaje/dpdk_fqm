/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_codel.h"

struct aqm_codel {
	struct rte_codel_rt codel_rt;
	struct rte_codel_config codel_config;
};

size_t aqm_codel_get_memory_size(void)
{
	return sizeof(struct aqm_codel);
}

int aqm_codel_init(struct aqm_codel *codel, struct rte_aqm_codel_params *params)
{
	struct rte_codel_params *codel_params = &params->params;

	if (rte_codel_config_init(&codel->codel_config,
				codel_params->target,
				codel_params->interval)) {
		RTE_LOG(ERR, AQM, "%s: configuration initiliazation failure\n",
				__func__);
		return -1;
	}

	return 0;
}

int aqm_codel_enqueue(struct aqm_codel *codel, struct circular_queue *cq,
			  struct rte_mbuf *pkt)
{
	return circular_queue_enqueue(cq, pkt);
}

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
			  struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			  uint32_t *n_bytes_dropped)
{
	int ret = 0;
	uint16_t qlen = circular_queue_get_length_pkts(cq);
	ret = circular_queue_dequeue(cq, pkt);
	if (unlikely(ret)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}
	*n_pkts_dropped = 0;
	*n_bytes_dropped = 0;

	if (unlikely(rte_codel_dequeue(&codel->codel_rt,
				&codel->codel_config,
				qlen,
				(*pkt)->timestamp))) {
		*n_pkts_dropped = 1;
		*n_bytes_dropped = (*pkt)->pkt_len;
		return 1;
	}

	return 0;
}

int aqm_codel_get_stats(struct aqm_codel *codel,
			struct rte_aqm_codel_stats *stats)
{
	return 0;
}

int aqm_codel_destroy(struct aqm_codel *codel)
{
	return 0;
}
