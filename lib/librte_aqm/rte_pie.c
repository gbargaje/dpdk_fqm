#include <stdio.h>
#include <rte_random.h>
#include <rte_timer.h>

#include <rte_pie.h>

int
rte_pie_config_init(struct rte_pie_config *config,
	uint32_t target_delay,
	uint32_t t_update,
	uint32_t mean_pkt_size,
	uint32_t max_burst,
	double alpha,
	double beta)
{
	if (config == NULL) {
		return -1;
	}
	if (target_delay <= 0) {
		return -2;
	}
	if (max_burst <= 0) {
		return -3;
	}
	if (t_update <= 0) {
		return -4;
	}
	if (mean_pkt_size <= 0) {
		mean_pkt_size = 2;
	}

	config->target_delay    = target_delay  * rte_get_timer_hz() / 1000u;
	config->t_update        = t_update * rte_get_timer_hz() / 1000u;
	config->alpha           = (uint32_t) alpha << 5;
	config->beta            = (uint32_t) beta << 5;
	config->mean_pkt_size   = mean_pkt_size;
	config->max_burst       = max_burst * rte_get_timer_hz() / 1000u;

	return 0;
}

int 
rte_pie_data_init(struct rte_pie_rt *pie_rt) 
{
	if(pie_rt == NULL) {
		return -1;
	}

	pie_rt->burst_allowance	= 150 * rte_get_timer_hz() / 1000u;
	pie_rt->drop_prob	= 0;
	pie_rt->cur_qdelay	= 0;
	pie_rt->old_qdelay	= 0;
	pie_rt->accu_prob	= 0;
	return 0;
}

int 
rte_pie_drop(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen) 
{
	if((pie_rt->cur_qdelay < config->target_delay>>1 \
		&& pie_rt->drop_prob < PIE_MAX_PROB/5) \
		|| (qlen <= 2*config->mean_pkt_size)){
		return PIE_ENQUEUE;
	}

	if(pie_rt->drop_prob == 0) {
		pie_rt->accu_prob = 0;
	}

	pie_rt->accu_prob += pie_rt->drop_prob;

	if(pie_rt->accu_prob < (uint64_t) PIE_MAX_PROB * 17 / 20) {
		return PIE_ENQUEUE;
	}

	if(pie_rt->accu_prob >= (uint64_t) PIE_MAX_PROB * 17 / 2) {
		return PIE_DROP;
	}

	int64_t random = rte_rand() % PIE_MAX_PROB;
	if (random < pie_rt->drop_prob) {
		pie_rt->accu_prob = 0;
		return PIE_DROP;
	}
	return PIE_ENQUEUE;
}

int 
rte_pie_enqueue(struct rte_pie_config *config,
	struct rte_pie_rt *pie_rt,
	uint32_t qlen) 
{
	if (pie_rt->burst_allowance == 0 && rte_pie_drop(config, pie_rt, qlen) == PIE_DROP) {
		return PIE_DROP;
	}

	if (pie_rt->drop_prob == 0 \
		&& pie_rt->cur_qdelay < config->target_delay>>1 \
		&& pie_rt->old_qdelay< config->target_delay>>1) {
		pie_rt->burst_allowance = config->max_burst;
	}
	return PIE_ENQUEUE;
}

void 
rte_pie_dequeue(struct rte_pie_rt *pie_rt,
	uint64_t timestamp) 
{
	uint64_t now = rte_get_tsc_cycles();
	pie_rt->cur_qdelay = now - timestamp;
}

