/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _AQM_RED_H_
#define _AQM_RED_H_

/**
 * @file
 * Random Early Detection (RED) AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aqm_red;

size_t aqm_red_get_memory_size(void);

int aqm_red_init(struct aqm_red *red, struct rte_aqm_red_params *params);

int aqm_red_enqueue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf *pkt);

int aqm_red_dequeue(struct aqm_red *red, struct circular_queue *cq,
		    struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
		    uint32_t *n_bytes_dropped);

int aqm_red_get_stats(struct aqm_red *red, struct rte_aqm_red_stats *stats);

int aqm_red_destroy(struct aqm_red *red);

#ifdef __cplusplus
}
#endif

#endif /* _AQM_RED_H_ */
