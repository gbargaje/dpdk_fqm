/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_pie.h"

struct aqm_pie {
};

size_t aqm_pie_get_memory_size(void)
{
	return sizeof(struct aqm_pie);
}

int aqm_pie_init(struct aqm_pie *pie, struct rte_aqm_pie_params *params)
{
	return 0;
}

int aqm_pie_enqueue(struct aqm_pie *pie, struct circular_queue *cq,
		    struct rte_mbuf *pkt)
{
	return 0;
}

int aqm_pie_dequeue(struct aqm_pie *pie, struct circular_queue *cq,
		    struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		    uint32_t *n_bytes_dropped)
{
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
