/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _AQM_CODEL_H_
#define _AQM_CODEL_H_

/**
 * @file
 * Controlled Delay (CoDel) AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aqm_codel;

size_t aqm_codel_get_memory_size(void);

int aqm_codel_init(struct aqm_codel *codel,
		   struct rte_aqm_codel_params *params);

int aqm_codel_enqueue(struct aqm_codel *codel, struct circular_queue *cq,
		      struct rte_mbuf *pkt);

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
		      struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		      uint32_t *n_bytes_dropped);

int aqm_codel_get_stats(struct aqm_codel *codel,
			struct rte_aqm_codel_stats *stats);

int aqm_codel_destroy(struct aqm_codel *codel);

#ifdef __cplusplus
}
#endif

#endif /* _AQM_CODEL_H_ */
