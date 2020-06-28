/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _RTE_AQM_H_
#define _RTE_AQM_H_

/**
 * @file
 * RTE Active Queue Management (AQM)
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rte_aqm_params {
	enum rte_aqm_algorithm algorithm;
	void *algorithm_params;
};

struct rte_aqm_stats {
	uint64_t bytes_dropped_overflow;
	uint64_t pkts_dropped_overflow;
	uint64_t bytes_dropped_dequeue;
	uint64_t pkts_dropped_dequeue;
	uint64_t bytes_dropped_enqueue;
	uint64_t pkts_dropped_enqueue;
	uint64_t bytes_dequeued;
	uint64_t pkts_dequeued;
	uint64_t bytes_enqueued;
	uint64_t pkts_enqueued;
	uint32_t length_bytes;
	uint16_t length_pkts;
	uint64_t queue_delay;
	void *algorithm_stats;
};

__rte_experimental
size_t rte_aqm_get_memory_size(enum rte_aqm_algorithm algorithm);

__rte_experimental
int rte_aqm_init(void *memory, struct rte_aqm_params *params,
		 struct rte_ring *queue_base, uint16_t limit);

__rte_experimental
uint8_t rte_aqm_is_empty(void *memory);

__rte_experimental
uint8_t rte_aqm_is_full(void *memory);

__rte_experimental
int rte_aqm_enqueue(void *memory, struct rte_mbuf *pkt);

__rte_experimental
int rte_aqm_dequeue(void *memory, struct rte_mbuf **pkt,
		    uint16_t *n_pkts_dropped, uint32_t *n_bytes_dropped);

__rte_experimental
int rte_aqm_get_stats(void *memory, struct rte_aqm_stats *stats);

__rte_experimental
int rte_aqm_destroy(void *memory);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_AQM_H_ */
