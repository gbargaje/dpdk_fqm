/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_branch_prediction.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_prefetch.h>

#include "circular_queue.h"

struct circular_queue {
	struct rte_ring *queue_base;
	uint64_t queue_delay_us;
	uint32_t length_bytes;
};

size_t circular_queue_get_memory_size(void)
{
	return sizeof(struct circular_queue);
}

void circular_queue_init(struct circular_queue *cq,
			 struct rte_ring *queue_base, uint16_t limit)
{
	cq->queue_base = queue_base;
	cq->queue_delay_us = 0;
	cq->length_bytes = 0;

	return;
}

uint16_t circular_queue_get_length_pkts(struct circular_queue *cq)
{
	return rte_ring_count(cq->queue_base);
}

uint32_t circular_queue_get_length_bytes(struct circular_queue *cq)
{
	return cq->length_bytes;
}

uint64_t circular_queue_get_queue_delay(struct circular_queue *cq)
{
	return cq->queue_delay_us;
}

uint8_t circular_queue_is_empty(struct circular_queue *cq)
{
	return rte_ring_empty(cq->queue_base);
}

uint8_t circular_queue_is_full(struct circular_queue *cq)
{
	return rte_ring_full(cq->queue_base);
}

int circular_queue_enqueue(struct circular_queue *cq, struct rte_mbuf *pkt)
{
	rte_ring_enqueue(cq->queue_base, pkt);

	pkt->timestamp = rte_rdtsc();
	cq->length_bytes += pkt->pkt_len;

	return 0;
}

int circular_queue_dequeue(struct circular_queue *cq, struct rte_mbuf **pkt)
{
	rte_ring_dequeue(cq->queue_base, (void *) pkt);

	cq->length_bytes -= (*pkt)->pkt_len;
	cq->queue_delay_us = (US_PER_S*(rte_rdtsc()-(*pkt)->timestamp)) / rte_get_timer_hz();
	
	return 0;
}
