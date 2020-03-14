#ifndef __RTE_PIE_H_INCLUDED__
#define __RTE_PIE_H_INCLUDED__
#include <bits/stdint-uintn.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DROP 			1
#define ENQUEUE 		0
#define MAX_PROB		0xfffff	//2^16
#define AB_SCALE		5

#include <stdio.h>
#include <rte_common.h>
#include <rte_random.h>
#include <rte_timer.h>

struct rte_pie_params{
	uint32_t target_delay;		//Target Queue Delay
	uint32_t t_update;		//drop rate calculation period
	uint32_t mean_pkt_size;		//(2 * Mean packet size) in number of packets.
	uint32_t max_burst;
};

struct rte_pie_config {
	uint32_t target_delay;		//Target Queue Delay
	uint32_t t_update;		//drop rate calculation period
	uint32_t alpha;
	uint32_t beta;
	uint32_t mean_pkt_size;		//(2*Mean packet size) in number of packets.
	uint32_t max_burst;
	uint64_t cps;			//cycles per second
};

struct rte_pie {
	uint64_t drop_prob;		//Drop rate
	uint32_t burst_allowance;	//burst allowance, initialized with value of MAX_BURST.
	uint64_t old_qdelay;		//Old queue delay
	uint64_t cur_qdelay;		//current queue delay
	uint64_t accu_prob;		//Accumulated drop rate.. Derandomization.
};

//may or may not be required
struct rte_pie_all {
	struct rte_pie_config *pie_config;
	struct rte_pie *pie;
};

int rte_pie_config_init(struct rte_pie_config*, uint32_t, uint32_t, uint32_t, uint32_t);
int rte_pie_data_init(struct rte_pie *);
int rte_pie_drop(struct rte_pie_config *, struct rte_pie *, uint32_t);
int rte_pie_enqueue(struct rte_pie_config *, struct rte_pie *, uint32_t);

static inline uint64_t max(uint64_t a, uint64_t b) {
	return a > b ? a : b;
}

//callback function for the timers
__rte_unused static void rte_pie_calc_drop_prob(
		__attribute__((unused)) struct rte_timer *tim, void *arg)
{
	struct rte_pie_all *pie_all = (struct rte_pie_all *) arg;
	struct rte_pie_config *config = pie_all->pie_config;
	struct rte_pie *pie = pie_all->pie;

	uint64_t cur_qdelay = pie->cur_qdelay * 1000000 / config->cps;			//in hz
	uint64_t old_qdelay = pie->old_qdelay;			//in hz
	uint64_t target_delay = config->target_delay;		//in microseconds

	int64_t a = (int64_t) cur_qdelay - (int64_t) target_delay; 	//in microseconds
	int64_t b = (int64_t) cur_qdelay - (int64_t) old_qdelay;	// in microseconds
	int64_t p = ((config->alpha * a)  >> AB_SCALE) + ((config->beta * b) >> AB_SCALE);
	RTE_LOG(INFO, SCHED, " rte_get_timer_hz: %lu\n", rte_get_timer_hz());
	RTE_LOG(INFO, SCHED, " Value cur_qdelay: %ld, old_qdelay: %ld, td: %ld\n", cur_qdelay, old_qdelay, target_delay);
	RTE_LOG(INFO, SCHED, " Value pie->cdel: %ld, pie->odel: %ld\n",pie-> cur_qdelay,pie-> old_qdelay);
	RTE_LOG(INFO, SCHED, " Value a: %ld, b: %ld, p: %ld \n", a, b, p);	

	if(p < 0)
		p = -p;

	//printf("Entering timer callback with p = %ld\n", p);
	//How alpha and beta are set?
	if 	  (pie->drop_prob < 1) {		//2^32*0.000001
		p /= 2048;
	} else if (pie->drop_prob < 10) {		//2^32*0.00001
		p /= 512;
	} else if (pie->drop_prob < 100) {		//2^32*0.0001
		p /= 128;
	} else if (pie->drop_prob < 1000) {		//2^32*0.001
		p /= 32;
	} else if (pie->drop_prob < 10000) {		//2^32*0.01
		p /= 8;
	} else if (pie->drop_prob < 100000) {		//2^32*0.1
		p /= 2;
	}	//else {
//		p = p;
//	}

	//Cap Drop Adjustment
	if (pie->drop_prob >= 100000 /*0.1*/ && p > 20000/*0.02*/) {
		p = 20000;	//0.02
	}
	pie->drop_prob += p;


	//Exponentially decay drop prob when congestion goes away
	if (cur_qdelay < target_delay/2 && old_qdelay < target_delay/2) {
		//pie->drop_prob *= 0.98;        //1 - 1/64 is sufficient
		pie->drop_prob -= (pie->drop_prob >> 6);
	}

	//Bound drop probability
	if (pie->drop_prob > MAX_PROB)
		pie->drop_prob = MAX_PROB;

	printf("Drop prob = %ld\n", pie->drop_prob);

	//Burst tolerance
	pie->burst_allowance = max(0, pie->burst_allowance - config->t_update);
	pie->old_qdelay = cur_qdelay;
}

//Called on each packet departure
static inline void rte_pie_dequeue(struct rte_pie *pie, uint64_t timestamp) {
	uint64_t now = rte_get_tsc_cycles();			//get the total number of cycles since boot.
	pie->cur_qdelay = now - timestamp;		//current queue delay in milliseconds.
}

#ifdef __cplusplus
}
#endif

#endif /* __RTE_PIE_H_INCLUDED__ */
