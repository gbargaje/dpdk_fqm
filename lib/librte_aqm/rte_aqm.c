/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_mbuf.h>

#include "aqm_red.h"
#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "rte_aqm.h"

struct rte_aqm {
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
	enum rte_aqm_algorithm algorithm;
};

size_t rte_aqm_get_memory_size(enum rte_aqm_algorithm algorithm)
{
	size_t memory_size;

	memory_size = sizeof(struct rte_aqm) + circular_queue_get_memory_size();

	switch (algorithm) {
		case RTE_AQM_FIFO:
			break;

		case RTE_AQM_RED:
			memory_size += aqm_red_get_memory_size();
			break;

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	return memory_size;
}

int rte_aqm_init(void *memory, struct rte_aqm_params *params,
		 struct rte_mbuf **queue_base, uint16_t limit)
{
	struct circular_queue *cq = NULL;
	struct rte_aqm *ra = NULL;

	ra = (struct rte_aqm *)memory;
	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));
	memory = (uint8_t *)memory + circular_queue_get_memory_size();

	ra->bytes_dropped_overflow = 0;
	ra->pkts_dropped_overflow = 0;
	ra->bytes_dropped_dequeue = 0;
	ra->pkts_dropped_dequeue = 0;
	ra->bytes_dropped_enqueue = 0;
	ra->pkts_dropped_enqueue = 0;
	ra->bytes_dequeued = 0;
	ra->pkts_dequeued = 0;
	ra->bytes_enqueued = 0;
	ra->pkts_enqueued = 0;
	ra->algorithm = params->algorithm;

	circular_queue_init(cq, queue_base, limit);

	switch (params->algorithm) {
		case RTE_AQM_FIFO:
			break;

		case RTE_AQM_RED:
			return aqm_red_init(memory, params->algorithm_params);

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	return 0;
}

uint8_t rte_aqm_is_empty(void *memory)
{
	struct circular_queue *cq = NULL;

	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));

	return circular_queue_is_empty(cq);
}

uint8_t rte_aqm_is_full(void *memory)
{
	struct circular_queue *cq = NULL;

	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));

	return circular_queue_is_full(cq);
}

void rte_aqm_prefetch_head(void *memory)
{
	struct circular_queue *cq = NULL;

	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));

	circular_queue_prefecth_head(cq);

	return;
}

void rte_aqm_prefetch_tail(void *memory)
{
	struct circular_queue *cq = NULL;

	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));

	circular_queue_prefecth_tail(cq);

	return;
}

int rte_aqm_enqueue(void *memory, struct rte_mbuf *pkt)
{
	struct circular_queue *cq = NULL;
	struct rte_aqm *ra = NULL;
	int ret;

	ra = (struct rte_aqm *)memory;
	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));
	memory = (uint8_t *)memory + circular_queue_get_memory_size();

	if (unlikely(circular_queue_is_full(cq))) {
		ra->bytes_dropped_overflow += pkt->pkt_len;
		ra->pkts_dropped_overflow++;
		rte_pktmbuf_free(pkt);
		return 1;
	}

	switch (ra->algorithm) {
		case RTE_AQM_FIFO:
			ret = circular_queue_enqueue(cq, pkt);
			break;

		case RTE_AQM_RED:
			ret = aqm_red_enqueue(memory, cq, pkt);
			break;

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	if (unlikely(ret == -1)) {
		RTE_LOG(ERR, AQM, "%s: enqueue failure\n", __func__);
		return -1;
	} else if (ret == 0) {
		ra->bytes_enqueued += pkt->pkt_len;
		ra->pkts_enqueued++;
	} else {
		ra->bytes_dropped_enqueue += pkt->pkt_len;
		ra->pkts_dropped_enqueue++;
		return 1;
	}

	return 0;
}

int rte_aqm_dequeue(void *memory, struct rte_mbuf **pkt,
		    uint16_t *n_pkts_dropped, uint32_t *n_bytes_dropped)
{
	struct circular_queue *cq = NULL;
	struct rte_aqm *ra = NULL;
	int ret;

	ra = (struct rte_aqm *)memory;
	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));
	memory = (uint8_t *)memory + circular_queue_get_memory_size();

	*n_pkts_dropped = 0;
	*n_bytes_dropped = 0;

	if (circular_queue_is_empty(cq)) {
		pkt = NULL;
		return 1;
	}

	switch (ra->algorithm) {
		case RTE_AQM_FIFO:
			ret = circular_queue_dequeue(cq, pkt);
			break;

		case RTE_AQM_RED:
			ret = aqm_red_dequeue(memory, cq, pkt, n_pkts_dropped,
					      n_bytes_dropped);
			break;

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	if (unlikely(ret == -1)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}

	ra->bytes_dropped_dequeue += *n_bytes_dropped;
	ra->pkts_dropped_dequeue += *n_pkts_dropped;

	if (ret == 0) {
		ra->bytes_dequeued += (*pkt)->pkt_len;
		ra->pkts_dequeued++;
	} else {
		return 1;
	}

	return 0;
}

int rte_aqm_get_stats(void *memory, struct rte_aqm_stats *stats)
{
	struct circular_queue *cq = NULL;
	struct rte_aqm *ra = NULL;

	ra = (struct rte_aqm *)memory;
	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));
	memory = (uint8_t *)memory + circular_queue_get_memory_size();

	stats->bytes_dropped_overflow = ra->bytes_dropped_overflow;
	stats->pkts_dropped_overflow = ra->pkts_dropped_overflow;
	stats->bytes_dropped_dequeue = ra->bytes_dropped_dequeue;
	stats->pkts_dropped_dequeue = ra->pkts_dropped_dequeue;
	stats->bytes_dropped_enqueue = ra->bytes_dropped_enqueue;
	stats->pkts_dropped_enqueue = ra->pkts_dropped_enqueue;
	stats->bytes_dequeued = ra->bytes_dequeued;
	stats->pkts_dequeued = ra->pkts_dequeued;
	stats->bytes_enqueued = ra->bytes_enqueued;
	stats->pkts_enqueued = ra->pkts_enqueued;
	stats->length_bytes = circular_queue_get_length_bytes(cq);
	stats->length_pkts = circular_queue_get_length_pkts(cq);

	switch (ra->algorithm) {
		case RTE_AQM_FIFO:
			break;

		case RTE_AQM_RED:
			return aqm_red_get_stats(memory,
						 stats->algorithm_stats);

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	return 0;
}

int rte_aqm_destroy(void *memory)
{
	struct circular_queue *cq = NULL;
	struct rte_mbuf *pkt = NULL;
	struct rte_aqm *ra = NULL;

	ra = (struct rte_aqm *)memory;
	cq = (void *)((uint8_t *)memory + sizeof(struct rte_aqm));
	memory = (uint8_t *)memory + circular_queue_get_memory_size();

	while(!circular_queue_is_empty(cq)) {
		circular_queue_dequeue(cq, &pkt);
		rte_pktmbuf_free(pkt);
	}

	switch (ra->algorithm) {
		case RTE_AQM_FIFO:
			break;

		case RTE_AQM_RED:
			return aqm_red_destroy(memory);

		default:
			RTE_LOG(ERR, AQM, "%s: unknown algorithm\n", __func__);
			return -1;
	}

	return 0;
}
