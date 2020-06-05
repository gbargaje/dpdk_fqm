/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _AQM_WRED_H_
#define _AQM_WRED_H_

/**
 * @file
 * Weighted Random Early Detection (WRED) AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aqm_wred;

size_t aqm_wred_get_memory_size(void);

int aqm_wred_init(struct aqm_wred *wred, struct rte_aqm_wred_params *params);

int aqm_wred_enqueue(struct aqm_wred *wred, struct circular_queue *cq,
		     struct rte_mbuf *pkt);

int aqm_wred_dequeue(struct aqm_wred *wred, struct circular_queue *cq,
		     struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		     uint32_t *n_bytes_dropped);

int aqm_wred_get_stats(struct aqm_wred *wred, struct rte_aqm_wred_stats *stats);

int aqm_wred_destroy(struct aqm_wred *wred);

#ifdef __cplusplus
}
#endif

#endif /* _AQM_WRED_H_ */
