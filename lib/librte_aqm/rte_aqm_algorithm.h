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
#include "rte_codel.h"

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

struct rte_aqm_pie_params {
};

struct rte_aqm_pie_stats {
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
