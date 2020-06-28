/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _CIRCULAR_QUEUE_H_
#define _CIRCULAR_QUEUE_H_

/**
 * @file
 * Circular Queue for AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ring.h>

#ifdef __cplusplus
extern "C" {
#endif

struct circular_queue;

size_t circular_queue_get_memory_size(void);

void circular_queue_init(struct circular_queue *cq,
			 struct rte_ring *queue_base, uint16_t limit);

uint16_t circular_queue_get_length_pkts(struct circular_queue *cq);

uint32_t circular_queue_get_length_bytes(struct circular_queue *cq);

uint64_t circular_queue_get_queue_delay(struct circular_queue *cq);

uint8_t circular_queue_is_empty(struct circular_queue *cq);

uint8_t circular_queue_is_full(struct circular_queue *cq);
/*
void circular_queue_prefetch_head(struct circular_queue *cq);

void circular_queue_prefetch_tail(struct circular_queue *cq);
*/
int circular_queue_enqueue(struct circular_queue *cq, struct rte_mbuf *pkt);

int circular_queue_dequeue(struct circular_queue *cq, struct rte_mbuf **pkt);

#ifdef __cplusplus
}
#endif

#endif /* _CIRCULAR_QUEUE_H_ */
