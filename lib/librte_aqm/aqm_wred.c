/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "aqm_red.h"
#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_wred.h"

struct aqm_wred {
};

size_t aqm_wred_get_memory_size(void)
{
	return sizeof(struct aqm_wred);
}

int aqm_wred_init(struct aqm_wred *wred, struct rte_aqm_wred_params *params)
{
	return 0;
}

int aqm_wred_enqueue(struct aqm_wred *wred, struct circular_queue *cq,
		     struct rte_mbuf *pkt)
{
	return 0;
}

int aqm_wred_dequeue(struct aqm_wred *wred, struct circular_queue *cq,
		     struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		     uint32_t *n_bytes_dropped)
{
	return 0;
}

int aqm_wred_get_stats(struct aqm_wred *wred, struct rte_aqm_wred_stats *stats)
{
	return 0;
}

int aqm_wred_destroy(struct aqm_wred *wred)
{
	return 0;
}
