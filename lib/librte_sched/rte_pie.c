#include <stdio.h>
#include <rte_pie.h>
#include <rte_random.h>
#include <rte_timer.h>

int
rte_pie_config_init(struct rte_pie_config *config,
		uint32_t target_delay,		//Target Queue Delay
		uint32_t t_update,			//drop rate calculation period
		uint32_t mean_pkt_size,		//Mean packet size in number of packets.
		uint32_t max_burst){

	if(config == NULL)
		return -1;
	if(target_delay == 0)
		return -2;
	if(max_burst == 0)
		return -3;
	if(t_update == 0)
		return -4;
	if(mean_pkt_size == 0)
		mean_pkt_size = 2;

	config->target_delay 	= target_delay * 1000u;		//15000ul;	//15ms in microseconds
	config->t_update		= t_update * 1000u;			//15000ul;	//same as above
	config->alpha			= 2;						//0.125 of 2^5
	config->beta			= 20;						//1.25	of 2^5
	config->mean_pkt_size	= mean_pkt_size;			//(2)	Confirm how it should be
	config->max_burst		= max_burst * 1000u;		//150000ul;	//150ms

	return 0;
}

int rte_pie_data_init(struct rte_pie *pie){

	if(pie == NULL)
		return -1;

	pie->burst_allowance 	= 150000ul;	//max burst
	pie->drop_prob 			= 0;
	pie->cur_qdelay 		= 0;
	pie->old_qdelay			= 0;
	pie->accu_prob			= 0;

	return 0;
}

int rte_pie_drop(struct rte_pie_config *config, struct rte_pie *pie, uint32_t qlen) {
	//Safeguard PIE to be work conserving
	if((pie->cur_qdelay < config->target_delay/2 \
			&& pie->drop_prob < 858993459) \
			|| (qlen <= config->mean_pkt_size)){
		return ENQUEUE;
	}

	//function from rte_random.h return random number less than argument specified.
	uint64_t u = rte_rand() >> 32; //rte_rand_max(MAX_PROB);	//get a 64 bit random number
	if(u < pie->drop_prob)
		return DROP;
	return ENQUEUE;
}

//Called on each packet arrival
int rte_pie_enqueue(struct rte_pie_config *config, struct rte_pie *pie, uint32_t qlen) {

	//burst allowance is multiple of t_update
	if (pie->burst_allowance == 0 && rte_pie_drop(config, pie, qlen) == DROP)
		return DROP;

	if (pie->drop_prob == 0 \
			&& pie->cur_qdelay < config->target_delay/2 \
			&& pie->old_qdelay< config->target_delay/2) {
		pie->burst_allowance = config->max_burst;
	}
	return ENQUEUE;
}
