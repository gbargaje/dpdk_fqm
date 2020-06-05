/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _AQM_PIE_H_
#define _AQM_PIE_H_

/**
 * @file
 * Proportional Integral controller Enhanced (PIE) AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aqm_pie;

size_t aqm_pie_get_memory_size(void);

int aqm_pie_init(struct aqm_pie *pie, struct rte_aqm_pie_params *params);

int aqm_pie_enqueue(struct aqm_pie *pie, struct circular_queue *cq,
		    struct rte_mbuf *pkt);

int aqm_pie_dequeue(struct aqm_pie *pie, struct circular_queue *cq,
		    struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		    uint32_t *n_bytes_dropped);

int aqm_pie_get_stats(struct aqm_pie *pie, struct rte_aqm_pie_stats *stats);

int aqm_pie_destroy(struct aqm_pie *pie);

#ifdef __cplusplus
}
#endif

#endif /* _AQM_PIE_H_ */
