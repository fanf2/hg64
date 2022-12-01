/*
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 *
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <math.h>
#include <stdint.h>

#include "random.h"

/*
 * XXXFANF no option to set the seed right now
 */
static uint64_t pcg32_val = 0x853c49e6748fea9bULL;
static uint64_t pcg32_inc = 0xda3e39cb94b95bdbULL;
static const uint64_t pcg32_mul = 6364136223846793005ULL;

static uint32_t
pcg32(void) {
	uint64_t raw = pcg32_val;
	pcg32_val = raw * pcg32_mul + pcg32_inc;
	uint32_t xsh = (uint32_t)(((raw >> 18) ^ raw) >> 27);
	uint32_t rot = raw >> 59;
	return (xsh >> (+rot & 31)) | (xsh << (-rot & 31));
}

uint32_t
rand_lemire(uint32_t limit) {
	uint64_t num = (uint64_t)pcg32() * (uint64_t)limit;
	if ((uint32_t)(num) < limit) {
		uint32_t residue = (uint32_t)(-limit) % limit;
		while ((uint32_t)(num) < residue) {
			num = (uint64_t)pcg32() * (uint64_t)limit;
		}
	}
	return ((uint32_t)(num >> 32));
}

double
rand_uniform(void) {
	return((double)pcg32() / (double)UINT32_MAX);
}

double
rand_exponential(void) {
	return(-log(rand_uniform()));
}

double
rand_pareto(void) {
	return(1 / rand_uniform() - 1.0);
}

double
rand_gamma(unsigned k) {
	double sum = 0.0;
	for(unsigned i = 0; i < k; i++) {
		sum += rand_exponential();
	}
	return(sum / k); /* mean == 1 */
}

double
rand_normal(void) {
	/* irwin-hall uniform sum; 12 gives sigma == 1 */
	double sum = 0.0;
	for(unsigned i = 0; i < 12; i++) {
		sum += rand_uniform();
	}
	return(sum - 6.0);
}

double
rand_lognormal(void) {
	return(exp(rand_normal()));
}

double
rand_chisquared(unsigned k) {
	double sum = 0.0;
	for(unsigned i = 0; i < k; i++) {
		double r = rand_normal();
		sum += r * r;
	}
	return(sum / k); /* mean == 1 */
}
