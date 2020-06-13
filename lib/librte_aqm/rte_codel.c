#include <math.h>
#include <rte_common.h>
#include <rte_codel.h>
#include <stdio.h>
#include <rte_random.h>
#include <rte_timer.h>

#ifdef __INTEL_COMPILER
#pragma warning(disable:2259) /* conversion may lose significant bits */
#endif

/*
 * Run a Newton method step:
 * new_invsqrt = (invsqrt / 2) * (3 - count * invsqrt^2)
 *
 * Here, invsqrt is a fixed point number (< 1.0), 32bit mantissa, aka Q0.32
 */

/**
 * @brief Newton step function
 *
 * @param codel [in,out] data pointer to CoDel runtime data
 */
static void
codel_newton_step(struct rte_codel_rt *codel);

/**
 * @brief CoDel Control Law
 *
 * @param t [in] current drop time of the packet
 * @param interval [in] sliding time window width
 * @param rec_inv_sqrt [in] reciprocal value of sqrt(count) >> 1
 *
 * @return Next drop time of the packet
 */
static u_int64_t 
codel_control_law(u_int64_t t, 
	u_int64_t interval, 
	u_int32_t rec_inv_sqrt);

int
rte_codel_config_init(struct rte_codel_config *config,
	uint64_t target,		
	uint64_t interval)
{
	if (config == NULL)
		return -1;
	if (target <= 0)
		return -2;
	if (interval <= 0)
		return -3;
	uint64_t cycles 		= rte_get_timer_hz();
	config->target 			= target * cycles / 1000u;	//5000ul = 5ms in microseconds
	config->interval		= interval * cycles / 1000u;	//10000ul = 100ms in microseconds

	return 0;
}

int rte_codel_data_init(struct rte_codel_rt *codel) {
	if (codel == NULL)
		return -1;

	codel->first_above_time		= 0;
	codel->drop_next		= 0;
	codel->count			= 0;
	codel->lastcount		= 0;
	codel->ok_to_drop		= CODEL_DEQUEUE;
	codel->dropping_state		= false;

	return 0;
}

static void 
codel_newton_step(struct rte_codel_rt *codel)
{
	uint32_t invsqrt, invsqrt2;
	uint64_t val;

	invsqrt = ((u_int32_t)codel->rec_inv_sqrt) << REC_INV_SQRT_SHIFT;
	invsqrt2 = ((u_int64_t)invsqrt * invsqrt) >> 32;
	val = (3LL << 32) - ((u_int64_t)codel->count * invsqrt2);
	val >>= 2; /* avoid overflow in following multiply */
	val = (val * invsqrt) >> (32 - 2 + 1);
	codel->rec_inv_sqrt = val >> REC_INV_SQRT_SHIFT;
}

static u_int64_t
codel_control_law(u_int64_t t, 
	u_int64_t interval, 
	u_int32_t rec_inv_sqrt)
{
	return (t + (u_int32_t)(((u_int64_t)interval *
	    (rec_inv_sqrt << REC_INV_SQRT_SHIFT)) >> 32));
}

int
rte_codel_dequeue(struct rte_codel_rt *codel, 
	struct rte_codel_config *config, 
	uint32_t qlen, 
	uint64_t timestamp)
{
	if (qlen == 0) {
		codel->first_above_time = 0;	//Since queue is empty, there is nothing to dequeue.
	}
	uint64_t now = rte_get_tsc_cycles();
	codel->sojourn_time = (now - timestamp);
	uint32_t delta;
	codel->ok_to_drop = CODEL_DEQUEUE;

	if ((codel->sojourn_time < config->target) || (qlen <= MAX_PACKET)) {
		codel->first_above_time = 0;	//Since sojourn time is less than TARGET, stay down for atleast INTERVAL
	} else {
		if (codel->first_above_time == 0) {
			codel->first_above_time = now + config->interval;
		} else if (now >= codel->first_above_time) {
			codel->ok_to_drop = CODEL_DROP;
		}
	}

	if (codel->dropping_state) {
		if (codel->ok_to_drop == CODEL_DEQUEUE) {
			codel->dropping_state = false;	// sojourn time below TARGET - leave drop state
		}
		while (now >= codel->drop_next && codel->dropping_state) {
			++codel->count;
			codel_newton_step(codel);
			if (!codel->ok_to_drop) {
				codel->dropping_state = false;
			} else {
				codel->drop_next = codel_control_law(codel->drop_next, config->interval, codel->rec_inv_sqrt);
			}
		}
	} else if (codel->ok_to_drop == CODEL_DROP) {
		codel->dropping_state = true;
		delta = codel->count - codel->lastcount;
		codel->count = 1;
		if ((delta > 1) && (now - codel->drop_next < 16*config->interval)) {
			codel->count = delta;
			codel_newton_step(codel);
		} else {
			codel->rec_inv_sqrt =  ~0U >> REC_INV_SQRT_SHIFT;
		}
		codel->drop_next = codel_control_law(now, config->interval, codel->rec_inv_sqrt);
		codel->lastcount = codel->count;
	}
	return codel->ok_to_drop;
}
