/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"
#include "rte_red.h"

#include "aqm_red.h"

struct aqm_red {
};

size_t aqm_red_get_memory_size(void)
{
	return sizeof(struct aqm_red);
}

int aqm_red_init(struct aqm_red *red, struct rte_aqm_red_params *params)
{
	return 0;
}

int aqm_red_enqueue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf *pkt)
{
	return 0;
}

int aqm_red_dequeue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		    uint32_t *n_bytes_dropped)
{
	return 0;
}

int aqm_red_get_stats(struct aqm_red *red, struct rte_aqm_red_stats *stats)
{
	return 0;
}

int aqm_red_destroy(struct aqm_red *red)
{
	return 0;
}
