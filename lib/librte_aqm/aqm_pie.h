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
#include <rte_random.h>
#include <rte_timer.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIE_DROP 		   	1				/**< return value: Drop the packet */
#define PIE_ENQUEUE		   	0				/**< return value: Enqueue the packet */
#define PIE_FIX_POINT_BITS 	13				/**< Number of bits for fractional part */
#define PIE_PROB_BITS	   	31				/**< Length of drop probability in bits */
#define PIE_MAX_PROB	   	((1LL<<PIE_PROB_BITS)-1)	/**< Max drop probability value */
#define PIE_SCALE			(1LL<<PIE_FIX_POINT_BITS)
/**
 * PIE configuration parameters
 */
struct rte_pie_config {
	uint32_t target_delay;		/**< target_delay */
	uint32_t t_update;		/**< t_update */
	uint32_t alpha;			/**< alpha */
	uint32_t beta;			/**< beta */
	uint32_t mean_pkt_size;		/**< mean_pkt_size */
	uint32_t max_burst;		/**< max_burst */
};

/**
 * PIE runtime data
 */
struct rte_pie_rt {
	int64_t drop_prob;	  /**< The current packet drop probability. Reset to 0 */
	uint32_t burst_allowance; /**< Current burst allowance */
	uint64_t old_qdelay;	  /**< The previous queue delay estimate. Reset to 0 */
	uint64_t cur_qdelay;	  /**< The current queue delay estimate */
	uint64_t accu_prob;	  /**< Accumulated drop probability. Reset to 0 */
	struct rte_timer *pie_timer;
};



struct aqm_pie {
	struct rte_pie_rt pie_rt;
	struct rte_pie_config pie_config;
	struct rte_timer pie_timer;
};

size_t aqm_pie_get_memory_size(void);

int aqm_pie_init(struct aqm_pie *pie, struct rte_aqm_pie_params *params);

int aqm_pie_enqueue(struct aqm_pie *pie, struct circular_queue *cq,
			struct rte_mbuf *pkt);

int aqm_pie_dequeue(struct aqm_pie *pie, struct circular_queue *cq,
			struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			uint32_t *n_bytes_dropped);

int aqm_pie_get_stats(struct aqm_pie *pie, struct rte_aqm_pie_stats *stats);

int aqm_pie_destroy(struct aqm_pie *pie);

/**
 * @brief Callback to calculate the drop probability based on the PIE runtime 
 * data and PIE configuration parameters. Drop probabilty is 
 * auto-tuned as per the recommendations in RFC 8033.
 *
 * @param tim [in] instance of the rte_timer which periodically calls this function
 * @param arg [in,out] data pointer to PIE runtime data and PIE configuration parameters
 */
__rte_unused static void 
rte_pie_calc_drop_prob(__attribute__((unused)) struct rte_timer *tim, 
	void *arg) 
{
	struct aqm_pie *pie = (struct aqm_pie *) arg;
	struct rte_pie_config *config = &pie->pie_config;
	struct rte_pie_rt *pie_rt = &pie->pie_rt;
	int p_isneg;
	
	uint64_t cur_qdelay = pie_rt->cur_qdelay;
	uint64_t old_qdelay = pie_rt->old_qdelay;
	uint64_t target_delay = config->target_delay;
	int64_t oldprob;
	
	int64_t p = config->alpha * (cur_qdelay - target_delay) + \
		config->beta * (cur_qdelay - old_qdelay);

	p_isneg = p < 0;

	if (p_isneg)
		p = -p;

	p *= (PIE_MAX_PROB << 12) / rte_get_tsc_hz();

	//Drop probability auto-tuning logic as per RFC 8033
	if (pie_rt->drop_prob < (PIE_MAX_PROB / 1000000)) {
		p >>= 11 + PIE_FIX_POINT_BITS + 12;
	} else if (pie_rt->drop_prob < (PIE_MAX_PROB / 100000)) {
		p >>= 9 + PIE_FIX_POINT_BITS + 12;
	} else if (pie_rt->drop_prob < (PIE_MAX_PROB / 10000)) {
		p >>= 7 + PIE_FIX_POINT_BITS + 12;
	} else if (pie_rt->drop_prob < (PIE_MAX_PROB / 1000)) {
		p >>= 5 + PIE_FIX_POINT_BITS + 12;
	} else if (pie_rt->drop_prob < (PIE_MAX_PROB / 100)) {
		p >>= 3 + PIE_FIX_POINT_BITS + 12;
	} else if (pie_rt->drop_prob < (PIE_MAX_PROB / 10)) {
		p >>= 1 + PIE_FIX_POINT_BITS + 12;
	} else {
		p >>= PIE_FIX_POINT_BITS + 12;
	}

	oldprob = pie_rt->drop_prob;
	
	if (p_isneg) {
		pie_rt->drop_prob = pie_rt->drop_prob - p;
		if (pie_rt->drop_prob > oldprob) {
			pie_rt->drop_prob = 0;
		}
	} else {
		//Cap Drop Adjustment
		if (pie_rt->drop_prob >= PIE_MAX_PROB / 10 && \
				p > PIE_MAX_PROB / 50 ) {
			p = PIE_MAX_PROB / 50;
		}

		pie_rt->drop_prob += p;

		if (pie_rt->drop_prob < oldprob) {
			pie_rt->drop_prob = PIE_MAX_PROB;
		}
	}

	if (pie_rt->drop_prob < 0) {
		pie_rt->drop_prob = 0;
	} else {
		//Exponentially decay the drop_prob when queue is empty
		if (cur_qdelay/1000 == 0 && old_qdelay/1000 == 0) {
			pie_rt->drop_prob -= pie_rt->drop_prob >> 6;
		}

		if (pie_rt->drop_prob > PIE_MAX_PROB) {
			pie_rt->drop_prob = PIE_MAX_PROB;
		}
	}

	//Update burst allowance
	if (pie_rt->burst_allowance < config->t_update) {
		pie_rt->burst_allowance = 0;
	} else {
		pie_rt->burst_allowance -= config->t_update;
	}

	pie_rt->old_qdelay = cur_qdelay;
}


#ifdef __cplusplus
}
#endif

#endif /* _AQM_PIE_H_ */
