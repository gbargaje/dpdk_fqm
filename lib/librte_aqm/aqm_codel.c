/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_codel.h"

struct aqm_codel {
};

size_t aqm_codel_get_memory_size(void)
{
	return sizeof(struct aqm_codel);
}

int aqm_codel_init(struct aqm_codel *codel, struct rte_aqm_codel_params *params)
{
	return 0;
}

int aqm_codel_enqueue(struct aqm_codel *codel, struct circular_queue *cq,
		      struct rte_mbuf *pkt)
{
	return 0;
}

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
		      struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		      uint32_t *n_bytes_dropped)
{
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
