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
static uint64_t
codel_control_law(uint64_t t,
	uint64_t interval,
	uint32_t rec_inv_sqrt);


static int
codel_should_drop(struct rte_codel_rt *codel,
	struct rte_codel_config *config,
	struct rte_mbuf **pkt,
	struct circular_queue *cq,
	uint64_t now);


size_t aqm_codel_get_memory_size(void)
{
	return sizeof(struct aqm_codel);
}

int aqm_codel_init(struct aqm_codel *codel, struct rte_aqm_codel_params *params)
{
	struct rte_codel_params *codel_params = &params->params;
	struct rte_codel_config *config = &codel->codel_config;
	struct rte_codel_rt *codel_rt = &codel->codel_rt;

	if (config == NULL)
		return -1;
	if (codel_params->target <= 0)
		return -2;
	if (codel_params->interval <= 0)
		return -3;

	config->target 			= (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
				codel_params->target * 1000u;
	config->interval		= (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
				codel_params->interval * 1000u;

	if (codel_rt == NULL)
		return -1;

	codel_rt->first_above_time 				= 0;
	codel_rt->drop_next 					= 0;
	codel_rt->count 						= 0;
	codel_rt->lastcount 					= 0;
	codel_rt->dropping_state 				= false;

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

	invsqrt = ((uint32_t)codel->rec_inv_sqrt) << REC_INV_SQRT_SHIFT;
	invsqrt2 = ((uint64_t)invsqrt * invsqrt) >> 32;
	val = (3LL << 32) - ((uint64_t)codel->count * invsqrt2);
	val >>= 2; /* avoid overflow in following multiply */
	val = (val * invsqrt) >> (32 - 2 + 1);
	codel->rec_inv_sqrt = val >> REC_INV_SQRT_SHIFT;
}

static uint64_t
codel_control_law(uint64_t t,
	uint64_t interval,
	uint32_t rec_inv_sqrt)
{
	return (t + (uint32_t)(((uint64_t)interval *
		(rec_inv_sqrt << REC_INV_SQRT_SHIFT)) >> 32));
}


static int
codel_should_drop(struct rte_codel_rt *codel,
	struct rte_codel_config *config,
	struct rte_mbuf **pkt,
	struct circular_queue *cq,
	uint64_t now)
{

	int ret = circular_queue_dequeue(cq, pkt);
	if (unlikely(ret)) {
		RTE_LOG(ERR, AQM, "%s: dequeue failure\n", __func__);
		return -1;
	}

	if(pkt == NULL)	{
		codel->first_above_time = 0;
		return CODEL_DEQUEUE;
	}

	codel->sojourn_time = circular_queue_get_queue_delay(cq) * rte_get_timer_hz()/(1000*1000u);// Sojourn time in units of cpu cycles

	if ((codel->sojourn_time < config->target) || (circular_queue_get_length_bytes(cq) <= MAX_PACKET)) {
		codel->first_above_time = 0;	//Since sojourn time is less than TARGET, stay down for atleast INTERVAL
		return CODEL_DEQUEUE;
	}
	if (codel->first_above_time == 0) {
		/*  Just went above from below. If we stay above
		 * for at least interval we'll say it's ok to drop
		*/
		codel->first_above_time = now + config->interval;
		return CODEL_DEQUEUE;
	}

 	if (now > codel->first_above_time)
		return CODEL_DROP;

	return CODEL_DEQUEUE;
}

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
			  struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			  uint32_t *n_bytes_dropped)
{
	uint16_t pkts_dropped = 0,bytes_dropped = 0;
	int drop;

	if (circular_queue_get_length_pkts(cq) == 0) {
		// If the queue is empty
		codel->codel_rt.dropping_state = false;
		pkt = NULL;
		return 1;
	}

	uint64_t now = rte_rdtsc();
	//Get the drop decision
	drop = codel_should_drop(&codel->codel_rt,&codel->codel_config,pkt,cq,now);
	if(unlikely(drop == -1))
		return -1;

	if (codel->codel_rt.dropping_state) {

		if (drop == CODEL_DEQUEUE)
			codel->codel_rt.dropping_state = false;	// sojourn time below TARGET - leave drop state
		else if(now >= codel->codel_rt.drop_next) {
		/* It's time for the next drop. Drop the current
			 * packet and dequeue the next. The dequeue might
			 * take us out of dropping state.
			 * If not, schedule the next drop.
			 * A large backlog might result in drop rates so high
			 * that the next drop should happen now,
			 * hence the while loop.
			 */
			while (now >= codel->codel_rt.drop_next && codel->codel_rt.dropping_state) {
		    codel->codel_rt.count++;
		    codel_newton_step(&codel->codel_rt);

				// DROP PACKET
				pkts_dropped ++;
		    bytes_dropped += (*pkt)->pkt_len;
				rte_pktmbuf_free(*pkt);

				drop = codel_should_drop(&codel->codel_rt,&codel->codel_config,pkt,cq,now);
				if(unlikely(drop == -1))
					return -1;

				if (drop == CODEL_DEQUEUE)
	              codel->codel_rt.dropping_state = false;
	      else
	              codel->codel_rt.drop_next = codel_control_law(codel->codel_rt.drop_next, codel->codel_config.interval, codel->codel_rt.rec_inv_sqrt);

      }
		}
	} else if (drop == CODEL_DROP) {
		// DROP PACKET
    pkts_dropped ++;
  	bytes_dropped += (*pkt)->pkt_len;
		rte_pktmbuf_free(*pkt);

		drop = codel_should_drop(&codel->codel_rt,&codel->codel_config,pkt,cq,now);
		if(unlikely(drop == -1))
			return -1;
		codel->codel_rt.dropping_state = true;

		if (now - codel->codel_rt.drop_next < 16*codel->codel_config.interval) {
			codel->codel_rt.count = (codel->codel_rt.count - codel->codel_rt.lastcount) | 1;
			codel_newton_step(&codel->codel_rt);
		} else {
			codel->codel_rt.count = 1;
			codel->codel_rt.rec_inv_sqrt =  ~0U >> REC_INV_SQRT_SHIFT;
		}

		codel->codel_rt.lastcount = codel->codel_rt.count;
		codel->codel_rt.drop_next = codel_control_law(now, codel->codel_config.interval, codel->codel_rt.rec_inv_sqrt);
	}

	*n_pkts_dropped = pkts_dropped;
	*n_bytes_dropped = bytes_dropped;

	if(unlikely(pkts_dropped > 0))
		return 1;
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
