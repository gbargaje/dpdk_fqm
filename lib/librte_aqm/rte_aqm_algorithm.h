/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 NITK Surathkal
 */

#ifndef _RTE_AQM_ALGORITHM_H_
#define _RTE_AQM_ALGORITHM_H_

/**
 * @file
 * RTE Active Queue Management
 */

#include <stdint.h>

#include "rte_red.h"

#ifdef __cplusplus
extern "C" {
#endif

enum rte_aqm_algorithm {
	RTE_AQM_FIFO,
	RTE_AQM_RED,
	RTE_AQM_WRED,
	RTE_AQM_PIE,
	RTE_AQM_CODEL
};

struct rte_aqm_red_params {
	struct rte_red_params params;
};

struct rte_aqm_red_stats {
	/* TODO: add stats if required */
};

struct rte_aqm_wred_params {
};

struct rte_aqm_wred_stats {
};

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

struct rte_aqm_pie_params {
	struct rte_pie_params params;
};

struct rte_aqm_pie_stats {
};

/**
 * CoDel configuration parameters passed by user
 *
 */
struct rte_codel_params {
	uint64_t	target; 	/**< Target queue delay */
	uint64_t	interval;	/**< Width of moving time window */
};

struct rte_aqm_codel_params {
	struct rte_codel_params params;
};

struct rte_aqm_codel_stats {
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_AQM_ALGORITHM_H_ */
