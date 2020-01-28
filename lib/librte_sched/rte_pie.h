#ifndef __RTE_PIE_H_INCLUDED__
#define __RTE_PIE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#define DROP 			0
#define ENQUE 			1
#define MAX_PROB		0xffffffff	//2^32
#define AB_SCALE		5
//#define TARGET_QDELAY	15000		//in microseconds
//#define MAX_BURST 	150000		//in microseconds
//#define T_UPDATE		15000		//in milliseconds
//#define MEAN_PKTSIZE	1500	//to be decided

#include <stdio.h>
#include <rte_common.h>
#include <rte_random.h>
#include <rte_timer.h>

struct rte_pie_params {
	uint32_t target_delay;		//Target Queue Delay
	uint32_t t_update;			//drop rate calculation period
	uint32_t alpha;
	uint32_t beta;
	uint32_t mean_pkt_size;		//Mean packet size in number of packets.
	uint32_t max_burst;
};

struct rte_pie {
	uint64_t drop_prob;			//Drop rate
	uint32_t burst_allowance;	//burst allowance, initialized with value of MAX_BURST.
	uint64_t old_qdelay;		//Old queue delay
	uint64_t cur_qdelay;			//current queue delay
	uint64_t accu_prob;			//Accumulated drop rate.. Derandomization.
};

struct rte_pie_config {
	struct rte_pie_params *pie_params;
	struct rte_pie *pie;
};

void rte_pie_params_init(struct rte_pie_params*);
void rte_pie_init(struct rte_pie *);
int rte_pie_drop(struct rte_pie_config*, uint32_t);
int rte_pie_enque(struct rte_pie_config *, uint32_t);

static inline uint64_t max(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

//Update periodically, T_UPDATE = 15 milliseconds
__rte_unused static void rte_pie_cal_drop_prob(
		__attribute__((unused)) struct rte_timer *tim,	void *arg)
{
	struct rte_pie_config *pie_config = (struct rte_pie_config *) arg;
	struct rte_pie_params *params = pie_config->pie_params;
	struct rte_pie *pie = pie_config->pie;
	uint64_t cur_qdelay = pie->cur_qdelay;			//in hz
	uint64_t old_qdelay = pie->old_qdelay;			//in hz
	uint64_t target_delay = params->target_delay;	//in microseconds

	uint64_t p = (params->alpha * (pie->cur_qdelay- params->target_delay))>>AB_SCALE + \
			(params->beta * (pie->cur_qdelay - pie->old_qdelay))>>AB_SCALE;

	//How alpha and beta are set?
	if 		  (pie->drop_prob < 4294) {				//2^32*0.000001
		p /= 2048;
	} else if (pie->drop_prob < 42949) {		//2^32*0.00001
		p /= 512;
	} else if (pie->drop_prob < 429496) {		//2^32*0.0001
		p /= 128;
	} else if (pie->drop_prob < 4294967) {		//2^32*0.001
		p /= 32;
	} else if (pie->drop_prob < 42949672) {		//2^32*0.01
		p /= 8;
	} else if (pie->drop_prob < 429496729) {	//2^32*0.1
		p /= 2;
	} else {
		p = p;
	}

	//Cap Drop Adjustment
	if (pie->drop_prob >= 429496729 /*0.1*/ && p > 858993459/*0.02*/) {
		p = 858993459;	//0.02
	}
	pie->drop_prob += p;

	//Exponentially decay drop prob when congestion goes away
	if (pie->cur_qdelay < params->target_delay/2 && pie->old_qdelay < params->target_delay/2) {
		//pie->drop_prob *= 0.98;        //1 - 1/64 is sufficient
		pie->drop_prob -= (MAX_PROB>>6);
	}

	//Bound drop probability
/*
	if (pie->drop_prob < 0u)
		pie->drop_prob = 0u;
*/
	if (pie->drop_prob > MAX_PROB)
		pie->drop_prob = MAX_PROB;

	//Burst tolerance
	pie->burst_allowance = max(0, pie->burst_allowance - params->t_update);
	pie->old_qdelay = pie->cur_qdelay;
}


//Called on each packet departure
static inline void rte_pie_deque(struct rte_pie_config *pie_config, uint64_t timestamp) {
	uint64_t now = rte_get_tsc_cycles();			//get the total number of cycles since boot.
	pie_config->pie->cur_qdelay = now - timestamp;	//current queue delay in milliseconds.
}

#ifdef __cplusplus
}
#endif

#endif /* __RTE_PIE_H_INCLUDED__ */

/*
 * //Called on each packet departure
 *static inline void rte_pie_deque(struct rte_pie_config *pie_config, uint64_t timestamp) {
 *		uint64_t now = rte_get_tsc_cycles();		//get the total number of cycles since boot.
 *		//uint64_t cycles_in_second = rte_get_timer_hz(); //get the number of cycles in one second
 *		//now /= cycles_in_second / 1000;	// get the timestamp in milliseconds.
 *		pie_config->pie->cur_qdelay = now - timestamp;	//current queue delay in milliseconds.
 * }
 *
 */
