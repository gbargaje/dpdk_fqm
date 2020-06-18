
#ifndef __RTE_PIE_H_INCLUDED__
#define __RTE_PIE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Proportional Integral controller Enhanced (PIE)
 *
 *
 ***/

#include <stdio.h>
#include <rte_common.h>
#include <rte_random.h>
#include <rte_timer.h>

#define PIE_DROP 		   0				/**< return value: Drop the packet */
#define PIE_ENQUEUE		   1				/**< return value: Enqueue the packet */
#define PIE_FIX_POINT_BITS 13				/**< Number of bits for fractional part */
#define PIE_PROB_BITS	   31				/**< Length of drop probability in bits */
#define PIE_MAX_PROB	   ((1LL<<PIE_PROB_BITS)-1)	/**< Max drop probability value */

/**
 * PIE configuration parameters passed by user
 *
 */
struct rte_pie_params {
	uint32_t target_delay;	/**< Latency Target (default: 15ms) */
	uint32_t t_update;	/**< A period to calculate the drop probabilty (default: 15ms) */
	uint32_t mean_pkt_size;	/**< Enqueue the packet if number of packets in 
				queue <= mean_pkt_size (in number of packets) */
	uint32_t max_burst;	/**< Max Burst Allowance (default: 150ms) */
	uint32_t alpha;		/**< (default: 1/8) */
	uint32_t beta;		/**< (default: 1 + 1/4) */
};

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


/**
 * PIE structure created to pass to PIE drop probability calculation callback
 */
struct rte_pie {
	struct rte_pie_config *pie_config;	/**< PIE configuration parameters */
	struct rte_pie_rt *pie_rt;		/**< PIE runtime data */
};

/**
 * @brief Configures a single PIE configuration parameter structure.
 *
 * @param config [in,out] config pointer to a PIE configuration parameter structure
 * @param target_delay [in] Latency Target (default: 15 milliseconds)
 * @param t_update [in] A period to calculate drop probability (default: 15 milliseconds)
 * @param mean_pkt_size [in] Enqueue the packet if number of packets in queue <= mean_pkt_size
 * @param max_burst [in] Max Burst Allowance (default: 150ms)
 * @param alpha [in] Weights in the drop probability calculation (1/s): (default: 1/8)
 * @param beta [in] Weights in the drop probability calculation (1/s): (default: 1 + 1/4)
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int 
rte_pie_config_init(struct rte_pie_config *config,
	uint32_t target_delay,
	uint32_t t_update,
	uint32_t mean_pkt_size,
	uint32_t max_burst,
	double alpha,
	double beta);

/**
 * @brief Initialises runtime data
 *
 * @param pie_rt [in,out] data pointer to PIE runtime data
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int 
rte_pie_data_init(struct rte_pie_rt *pie_rt);

/**
 * @brief make a decision to drop or enqueue a packet based on drop probability
 *
 * @param config [in] config pointer to structure defining PIE parameters
 * @param pie_rt [in,out] data pointer to PIE runtime data
 * @param qlen [in] Current length of the queue in number of packets.
 *
 * @return operation status
 * @retval ENQUEUE enqueue the packet
 * @retval DROP drop the packet
 */
int 
rte_pie_drop(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen);

/**
 * @brief Decides if new packet should be enqeued or dropped.
 * Updates runtime data based on delay estimates.
 * Based on the new delay estimates and PIE configuration parameters
 * gives verdict whether to enqueue or drop the packet.
 *
 * @param config [in] config pointer to structure defining PIE parameters
 * @param pie_rt [in,out] data pointer to PIE runtime data
 * @param qlen [in] Length of the queue in number of packets.
 *
 * @return Operation status
 * @retval ENQUEUE enqueue the packet
 * @retval DROP drop the packet based on the current burst allowance and 
 * operation status of rte_pie_drop.
 */
int 
rte_pie_enqueue(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen);

/**
 * @brief records the current queue delay on every packet dequeue.
 * Updates runtime data based on packet dequeue(departure) time.
 *
 * @param pie_rt [in,out] data pointer to PIE runtime data.
 * @param timestamp [in] dequeue time of the most recently dequeued packet.
 */
void 
rte_pie_dequeue(struct rte_pie_rt *pie_rt,
	uint64_t timestamp);

/**
 * @brief calculates maximum number among two unsigned 64 bit integers.
 *
 * @param a [in] first input number
 * @param b [in] second input number
 *
 * @return Maximum number
 */
static inline uint64_t 
max(uint64_t a, uint64_t b) 
{
	return a > b ? a : b;
}

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
	struct rte_pie *pie = (struct rte_pie *) arg;
	struct rte_pie_config *config = pie->pie_config;
	struct rte_pie_rt *pie_rt = pie->pie_rt;
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
	pie_rt->drop_prob += p;

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

#endif /* __RTE_PIE_H_INCLUDED__ */

