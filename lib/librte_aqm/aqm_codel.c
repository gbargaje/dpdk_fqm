/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#include "aqm_codel.h"

/*
 * Run a Newton method step:
 * new_invsqrt = (invsqrt / 2) * (3 - count * invsqrt^2)
 *
 * Here, invsqrt is a fixed point number (< 1.0), 32bit mantissa, aka Q0.32
 */

/**
 * @brief Newton step function
 *
 * @param codel [in,out] data pointer to CoDel runtime data
 */
static void
codel_newton_step(struct rte_codel_rt *codel);

/**
 * @brief CoDel Control Law
 *
 * @param t [in] current drop time of the packet
 * @param interval [in] sliding time window width
 * @param rec_inv_sqrt [in] reciprocal value of sqrt(count) >> 1
 *
 * @return Next drop time of the packet
 */
static u_int64_t 
codel_control_law(u_int64_t t, 
	u_int64_t interval, 
	u_int32_t rec_inv_sqrt);

int
aqm_codel_drop(struct rte_codel_rt *codel, 
	struct rte_codel_config *config, 
	uint32_t qlen, 
	uint64_t timestamp);

size_t aqm_codel_get_memory_size(void)
{
	return sizeof(struct aqm_codel);
}

int aqm_codel_init(struct aqm_codel *codel, struct rte_aqm_codel_params *params)
{
	struct rte_codel_params *codel_params = &params->params;
	struct rte_codel_config *config = &codel->codel_config;

	if (config == NULL)
		return -1;
	if (codel_params->target <= 0)
		return -2;
	if (codel_params->interval <= 0)
		return -3;

	uint64_t cycles 		= rte_get_timer_hz();
	config->target 			= codel_params->target * cycles / 1000u;	//5000ul = 5ms in microseconds
	config->interval		= codel_params->interval * cycles / 1000u;	//10000ul = 100ms in microseconds

	return 0;
}

int aqm_codel_enqueue(struct aqm_codel *codel, struct circular_queue *cq,
			  struct rte_mbuf *pkt)
{
	return circular_queue_enqueue(cq, pkt);
}

static void 
codel_newton_step(struct rte_codel_rt *codel)
{
	uint32_t invsqrt, invsqrt2;
	uint64_t val;

	invsqrt = ((u_int32_t)codel->rec_inv_sqrt) << REC_INV_SQRT_SHIFT;
	invsqrt2 = ((u_int64_t)invsqrt * invsqrt) >> 32;
	val = (3LL << 32) - ((u_int64_t)codel->count * invsqrt2);
	val >>= 2; /* avoid overflow in following multiply */
	val = (val * invsqrt) >> (32 - 2 + 1);
	codel->rec_inv_sqrt = val >> REC_INV_SQRT_SHIFT;
}

static u_int64_t
codel_control_law(u_int64_t t, 
	u_int64_t interval, 
	u_int32_t rec_inv_sqrt)
{
	return (t + (u_int32_t)(((u_int64_t)interval *
	    (rec_inv_sqrt << REC_INV_SQRT_SHIFT)) >> 32));
}

int
aqm_codel_drop(struct rte_codel_rt *codel, 
	struct rte_codel_config *config, 
	uint32_t qlen, 
	uint64_t timestamp)
{
	if (qlen == 0) {
		codel->first_above_time = 0;	//Since queue is empty, there is nothing to dequeue.
	}
	uint64_t now = rte_get_tsc_cycles();
	codel->sojourn_time = (now - timestamp);
	uint32_t delta;
	codel->ok_to_drop = CODEL_DEQUEUE;

	if ((codel->sojourn_time < config->target) || (qlen <= MAX_PACKET)) {
		codel->first_above_time = 0;	//Since sojourn time is less than TARGET, stay down for atleast INTERVAL
	} else {
		if (codel->first_above_time == 0) {
			codel->first_above_time = now + config->interval;
		} else if (now >= codel->first_above_time) {
			codel->ok_to_drop = CODEL_DROP;
		}
	}

	if (codel->dropping_state) {
		if (codel->ok_to_drop == CODEL_DEQUEUE) {
			codel->dropping_state = false;	// sojourn time below TARGET - leave drop state
		}
		while (now >= codel->drop_next && codel->dropping_state) {
			++codel->count;
			codel_newton_step(codel);
			if (!codel->ok_to_drop) {
				codel->dropping_state = false;
			} else {
				codel->drop_next = codel_control_law(codel->drop_next, config->interval, codel->rec_inv_sqrt);
			}
		}
	} else if (codel->ok_to_drop == CODEL_DROP) {
		codel->dropping_state = true;
		delta = codel->count - codel->lastcount;
		codel->count = 1;
		if ((delta > 1) && (now - codel->drop_next < 16*config->interval)) {
			codel->count = delta;
			codel_newton_step(codel);
		} else {
			codel->rec_inv_sqrt =  ~0U >> REC_INV_SQRT_SHIFT;
		}
		codel->drop_next = codel_control_law(now, config->interval, codel->rec_inv_sqrt);
		codel->lastcount = codel->count;
	}
	return codel->ok_to_drop;
}

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
			  struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			  uint32_t *n_bytes_dropped)
{
	int ret = 0;
	uint16_t qlen = circular_queue_get_length_pkts(cq);
	ret = circular_queue_dequeue(cq, pkt);

	if (unlikely(ret)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}
	*n_pkts_dropped = 0;
	*n_bytes_dropped = 0;

	if (unlikely(aqm_codel_drop(&codel->codel_rt,
				&codel->codel_config,
				qlen,
				(*pkt)->timestamp))) {
		*n_pkts_dropped = 1;
		*n_bytes_dropped = (*pkt)->pkt_len;
		return 1;
	}

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
