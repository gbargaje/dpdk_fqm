/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _AQM_CODEL_H_
#define _AQM_CODEL_H_

/**
 * @file
 * Controlled Delay (CoDel) AQM
 */

#include <stdint.h>

#include <rte_mbuf.h>

#include "circular_queue.h"
#include "rte_aqm_algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEL_DROP			1	/**< Return status variable: DROP drop/discard the packet */
#define CODEL_DEQUEUE 			0	/**< Return status variable: DEQUEUE dequeque the packet from queue */
#define MAX_PACKET 			512	/**< Maximum packet size in bytes (SHOULD use interface MTU) - Based on RFC */
#define	REC_INV_SQRT_BITS 		(8 * sizeof(uint16_t))		/**< sizeof_in_bits (rec_inv_sqrt) */
#define	REC_INV_SQRT_SHIFT 		(32 - REC_INV_SQRT_BITS)	/**< Needed shift to get a Q0.32 number from rec_inv_sqrt */

/**
 * CoDel runtime data
 */
struct rte_codel_rt {
	uint32_t	count;			/**< Number of packets dropped since last dropping state */
	uint32_t	lastcount;		/**< Count when going into dropping state */
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

struct aqm_codel {
	struct rte_codel_rt codel_rt;
	struct rte_codel_config codel_config;
};

size_t aqm_codel_get_memory_size(void);

int aqm_codel_init(struct aqm_codel *codel,
		   struct rte_aqm_codel_params *params);

int aqm_codel_enqueue(struct aqm_codel *codel, struct circular_queue *cq,
			struct rte_mbuf *pkt);

int aqm_codel_dequeue(struct aqm_codel *codel, struct circular_queue *cq,
			struct rte_mbuf **pkt, uint16_t *n_pkts_dropped,
			uint32_t *n_bytes_dropped);

int aqm_codel_get_stats(struct aqm_codel *codel,
			struct rte_aqm_codel_stats *stats);

int aqm_codel_destroy(struct aqm_codel *codel);

#ifdef __cplusplus
}
#endif

#endif /* _AQM_CODEL_H_ */
