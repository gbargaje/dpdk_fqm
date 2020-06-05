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

#ifdef __cplusplus
extern "C" {
#endif

enum rte_aqm_algorithm {
	RTE_AQM_FIFO,
	RTE_AQM_RED,
	RTE_AQM_WRED
};

struct rte_aqm_red_params {
};

struct rte_aqm_red_stats {
};

struct rte_aqm_wred_params {
};

struct rte_aqm_wred_stats {
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_AQM_ALGORITHM_H_ */
