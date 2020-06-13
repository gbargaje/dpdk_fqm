#ifndef __RTE_CODEL_H_INCLUDED__
#define __RTE_COEL_H_INCLUDED__
#include <bits/stdint-uintn.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Controlled Delay (CoDel)
 *
 *
 ***/

#include <stdio.h>
#include<stdbool.h>
#include <rte_common.h>
#include <rte_random.h>
#include <rte_timer.h>
#include <rte_mbuf.h>

#define CODEL_DROP 				1	/**< Return status variable: DROP drop/discard the packet */
#define CODEL_DEQUEUE 			0	/**< Return status variable: DEQUEUE dequeque the packet from queue */
#define MAX_PACKET 				512	/**< Maximum packet size in bytes (SHOULD use interface MTU) */
#define	REC_INV_SQRT_BITS 		(8 * sizeof(u_int16_t))		/**< sizeof_in_bits (rec_inv_sqrt) */
#define	REC_INV_SQRT_SHIFT 		(32 - REC_INV_SQRT_BITS)	/**< Needed shift to get a Q0.32 number from rec_inv_sqrt */

/**
 * CoDel configuration parameters passed by user
 *
 */
struct rte_codel_params {
	uint64_t	target; 	/**< Target queue delay */
	uint64_t	interval;	/**< Width of moving time window */
};

/**	
 * CoDel runtime data
 */
struct rte_codel_rt {
	uint32_t	count;			/**< Number of packets dropped since last dropping state */
	uint32_t	lastcount;		/**< Count when going into dropping state */
	int 		ok_to_drop;		/**< Flag to check whether to drop or not */
	bool 		dropping_state;		/**< Set true if CoDel is in dropping state */
	uint16_t 	rec_inv_sqrt;   	/**< For control_law: reciprocal value of sqrt(count) >> 1 */
	uint64_t	first_above_time;	/**< Time when sojourn time goes continuously above target */
	uint64_t	drop_next;		/**< Time when we should drop the next packet */
	uint64_t	sojourn_time;		/**< Sojourn time as in queue delay */
};

/**
 * CoDel configuration parameters
 */
struct rte_codel_config {
	uint64_t	target;			/**< target */
	uint64_t	interval;		/**< interval */
};

/**
 * @brief Initialises runtime data
 *
 * @param codel [in,out] data pointer to CoDel runtime data
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int 
rte_codel_data_init(struct rte_codel_rt *codel);

/**
 * @brief Configures a single CoDel configuration parameter structure.
 *
 * @param config [in,out] config pointer to a CoDel configuration parameter structure
 * @param target [in]  Target Queue Delay (default: 5 milliseconds)
 * @param interval [in] Width of the moving time window (default: 100 milliseconds)
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int 
rte_codel_config_init(struct rte_codel_config *config,
	uint64_t target,
	uint64_t interval);

/**
 * @brief Decides if new packet should be dequeued or dropped
 *
 * @param codel [in,out] Data pointer to CoDel runtime data
 * @param config [in,out] config pointer to a CoDel configuration parameter structure
 * @param qlen [in] Queue size in number of packets
 * @param timestamp [in] Current timestamp in cpu cycles
 *
 * @return Operation status
 * @retval 0 drop the packet 
 * @retval 1 dequeue the packet  
 */
int
rte_codel_dequeue(struct rte_codel_rt *codel_rt,
	struct rte_codel_config *config,
	uint32_t qlen,
	uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif /* __RTE_CODEL_H_INCLUDED__ */
