/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_branch_prediction.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>

#include "circular_queue.h"

struct circular_queue {
	struct rte_mbuf **queue_base;
	uint64_t queue_delay_us;
	uint32_t length_bytes;
	uint16_t length_pkts;
	uint16_t limit;
	uint16_t head;
	uint16_t tail;
};

size_t circular_queue_get_memory_size(void)
{
	return sizeof(struct circular_queue);
}

void circular_queue_init(struct circular_queue *cq,
			 struct rte_mbuf **queue_base, uint16_t limit)
{
	cq->queue_base = queue_base;
	cq->queue_delay_us = 0;
	cq->length_bytes = 0;
	cq->length_pkts = 0;
	cq->limit = limit;
	cq->head = 0;
	cq->tail = 0;

	return;
}

uint16_t circular_queue_get_length_pkts(struct circular_queue *cq)
{
	return cq->length_pkts;
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
	return cq->length_pkts == 0;
}

uint8_t circular_queue_is_full(struct circular_queue *cq)
{
	return cq->length_pkts == cq->limit;
}

void circular_queue_prefetch_head(struct circular_queue *cq)
{
	rte_prefetch0(cq->queue_base + cq->head);

	return;
}

void circular_queue_prefetch_tail(struct circular_queue *cq)
{
	rte_prefetch0(cq->queue_base + cq->tail);

	return;
}

int circular_queue_enqueue(struct circular_queue *cq, struct rte_mbuf *pkt)
{
	cq->queue_base[cq->tail++] = pkt;
	pkt->timestamp = rte_rdtsc();
	if (unlikely(cq->tail == cq->limit))
		cq->tail = 0;

	cq->length_pkts++;
	cq->length_bytes += pkt->pkt_len;

	return 0;
}

int circular_queue_dequeue(struct circular_queue *cq, struct rte_mbuf **pkt)
{
	*pkt = cq->queue_base[cq->head++];

	if (unlikely(cq->head == cq->limit))
		cq->head = 0;

	cq->length_pkts--;
	cq->length_bytes -= (*pkt)->pkt_len;
	cq->queue_delay_us = (US_PER_S*(rte_rdtsc()-(*pkt)->timestamp)) / rte_get_timer_hz();
	
	return 0;
}
