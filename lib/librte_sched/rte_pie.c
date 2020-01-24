#include <stdio.h>
#include <rte_pie.h>
#include <rte_random.h>
#include <rte_timer.h>

void rte_pie_params_init(struct rte_pie_params *params){
	params->target_delay 	= 15000ul;			//15ms
	params->t_update		= 15000ul;
	params->alpha			= 2;		//0.125
	params->beta			= 20;		//1.25
	params->mean_pkt_size	= 2;
	params->max_burst		= 150000ul;	//150ms
}

void rte_pie_init(struct rte_pie *pie){
	pie->burst_allowance 	= 150000ul;
	pie->drop_prob 			= 0;
	pie->cur_qdelay 		= 0;
	pie->old_qdelay			= 0;
	pie->accu_prob			= 0;
}

int rte_pie_drop(struct rte_pie_config *pie_config, uint32_t qlen) {
	struct rte_pie_params *params = pie_config->pie_params;
	struct rte_pie *pie = pie_config->pie;

	//Safeguard PIE to be work conserving
	if((pie->cur_qdelay < params->target_delay/2 && pie->drop_prob < 0.2) \
			|| (qlen <= 2 * params->mean_pkt_size)){
		return ENQUE;
	}

	//function from rte_random.h return random number less than argument specified.
	uint64_t u = rte_rand();	//get a 64 bit random number
	if(u < pie->drop_prob)
		return DROP;
	return ENQUE;
}


//Called on each packet arrival
int rte_pie_enque(struct rte_pie_config *pie_config, uint32_t qlen) {
	struct rte_pie_params *params = pie_config->pie_params;
	struct rte_pie *pie = pie_config->pie;

	//burst allowance is multiple of t_update
	if (pie->burst_allowance == 0 && rte_pie_drop(pie_config, qlen) == DROP)
		return DROP;

	if (pie->drop_prob == 0 && pie->cur_qdelay < params->target_delay/2 \
			 && pie->old_qdelay< params->target_delay/2) {
			pie->burst_allowance = params->max_burst;
	}
	return ENQUE;
}
