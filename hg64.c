/*
 * hg64 - 64-bit histograms
 *
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 *
 * Permission is hereby granted to use, copy, modify, and/or
 * distribute this software for any purpose with or without fee.
 *
 * This software is provided 'as is', without warranty of any kind.
 * In no event shall the authors be liable for any damages arising
 * from the use of this software.
 *
 * SPDX-License-Identifier: 0BSD OR MIT-0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hg64.h"

/* number of bins is same as number of bits in a value */
#define BINS 64

struct hg64 {
	unsigned sigbits;
	struct bin {
		uint64_t total;
		uint64_t *count;
	} bin[BINS];
};

/*
 * We waste a little extra space in the BINS array that could be saved
 * by omitting exponents that aren't needed by denormal numbers. However
 * we need the exact number of keys for accurate bounds checks.
 */
#define DENORMALS(hg) ((hg)->sigbits - 1)
#define EXPONENTS(hg) (BINS - DENORMALS(hg))
#define MANTISSAS(hg) (1 << (hg)->sigbits)
#define KEYS(hg) (EXPONENTS(hg) * MANTISSAS(hg))

#define BINSIZE(hg) MANTISSAS(hg)

/**********************************************************************/

#define OUTARG(ptr, val) (((ptr) != NULL) && (bool)(*(ptr) = (val)))

static inline uint64_t
interpolate(uint64_t span, uint64_t mul, uint64_t div) {
	double frac = (div == 0) ? 1 : (double)mul / (double)div;
	return((uint64_t)(span * frac));
}

/**********************************************************************/

hg64 *
hg64_create(unsigned sigbits) {
	if(sigbits < 1 || 6 < sigbits) {
		return(NULL);
	}
	hg64 *hg = malloc(sizeof(*hg));
	*hg = (hg64){ .sigbits = sigbits };
	return(hg);
}

void
hg64_destroy(hg64 *hg) {
	for(unsigned b = 0; b < BINS; b++) {
		free(hg->bin[b].count);
	}
	*hg = (hg64){ 0 };
	free(hg);
}

unsigned
hg64_sigbits(hg64 *hg) {
	return(hg->sigbits);
}

uint64_t
hg64_population(hg64 *hg) {
	uint64_t pop = 0;
	for(unsigned b = 0; b < BINS; b++) {
		pop += hg->bin[b].total;
	}
	return(pop);
}

size_t
hg64_buckets(hg64 *hg) {
	size_t buckets = 0;
	for(unsigned b = 0; b < BINS; b++) {
		if(hg->bin[b].count != NULL) {
			buckets += BINSIZE(hg);
		}
	}
	return(buckets);
}

size_t
hg64_size(hg64 *hg) {
	return(sizeof(hg64) + sizeof(uint64_t) * hg64_buckets(hg));
}

/**********************************************************************/

static inline uint64_t
get_minval(hg64 *hg, unsigned key) {
	unsigned sigtop = 1 << hg->sigbits;
	unsigned exponent = (key / sigtop) - 1;
	uint64_t mantissa = key % sigtop + sigtop;
	return(key < sigtop ? key : mantissa << exponent);
}

/*
 * don't shift by 64, and don't underflow exponent; instead,
 * reduce shift by 1 for each hazard and pre-shift UINT64_MAX
 */
static inline uint64_t
get_maxval(hg64 *hg, unsigned key) {
	unsigned sigtop = 1 << hg->sigbits;
	unsigned shift = 63 - (key / sigtop);
	uint64_t range = UINT64_MAX/4 >> shift;
	return(get_minval(hg, key) + range);
}

/*
 * This branchless conversion is due to Paul Khuong: see bin_down_of() in
 * http://pvk.ca/Blog/2015/06/27/linear-log-bucketing-fast-versatile-simple/
 */
static inline unsigned
get_key(hg64 *hg, uint64_t value) {
	/* hot path */
	unsigned sigtop = 1 << hg->sigbits;
	/* ensure that denormal numbers are all in the same bin */
	uint64_t binned = value | sigtop;
	int clz = __builtin_clzll((unsigned long long)(binned));
	/* actually 1 less than the exponent except for denormals */
	unsigned exponent = 63 - hg->sigbits - clz;
	/* mantissa has leading bit set except for denormals */
	unsigned mantissa = value >> exponent;
	/* leading bit of mantissa adds one to exponent */
	return((exponent << hg->sigbits) + mantissa);
}

static inline struct bin *
get_bin(hg64 *hg, unsigned key) {
	return(&hg->bin[key >> hg->sigbits]);
}

static inline uint64_t *
get_counter(hg64 *hg, unsigned key, bool nullable) {
	/* hot path */
	unsigned sigtop = 1 << hg->sigbits;
	struct bin *bin = &hg->bin[key / sigtop];
	if(bin->count != NULL) {
		return(&bin->count[key % sigtop]);
	}
	/* cold path */
	if(nullable) {
		return(NULL);
	}
	size_t bytes = sizeof(uint64_t) * sigtop;
	uint64_t *ptr = malloc(bytes);
	memset(ptr, 0, bytes);
	bin->count = ptr; /* CAS */
	return(&bin->count[key % sigtop]);
}

static inline void
bump_count(hg64 *hg, unsigned key, uint64_t count) {
	/* hot path */
	*get_counter(hg, key, false) += count;
	get_bin(hg, key)->total += count;
}

static inline uint64_t
get_key_count(hg64 *hg, unsigned key) {
	uint64_t *counter = get_counter(hg, key, true);
	return(counter == NULL ? 0 : *counter);
}

static inline uint64_t
get_bin_count(hg64 *hg, unsigned key) {
	return(get_bin(hg, key)->total);
}

/**********************************************************************/

void
hg64_add(hg64 *hg, uint64_t value, uint64_t count) {
	if(count > 0) {
		bump_count(hg, get_key(hg, value), count);
	}
}

void
hg64_inc(hg64 *hg, uint64_t value) {
	hg64_add(hg, value, 1);
}

bool
hg64_get(hg64 *hg, unsigned key,
		uint64_t *pmin, uint64_t *pmax, uint64_t *pcount) {
	if(key < KEYS(hg)) {
		OUTARG(pmin, get_minval(hg, key));
		OUTARG(pmax, get_maxval(hg, key));
		OUTARG(pcount, get_key_count(hg, key));
		return(true);
	} else {
		return(false);
	}
}

void
hg64_merge(hg64 *target, hg64 *source) {
	for(unsigned sk = 0; sk < KEYS(source); sk++) {
		uint64_t count = get_key_count(source, sk);
		if(count > 0) {
			unsigned tk = sk;
			if(source->sigbits > target->sigbits) {
				tk >>= source->sigbits - target->sigbits;
			}
			if(target->sigbits > source->sigbits) {
				tk <<= target->sigbits - source->sigbits;
			}
			bump_count(target, tk, count);
		}
	}
}

/**********************************************************************/

uint64_t
hg64_value_at_rank(hg64 *hg, uint64_t rank) {
	unsigned keys = KEYS(hg);
	unsigned binsize = BINSIZE(hg);
	unsigned key = 0;
	while(key < keys) {
		uint64_t count = get_bin_count(hg, key);
		if(rank < count) {
			break;
		}
		rank -= count;
		key += binsize;
	}
	if(key == keys) {
		return(UINT64_MAX);
	}

	unsigned stop = key + binsize;
	while(key < stop) {
		uint64_t count = get_key_count(hg, key);
		if(rank < count) {
			break;
		}
		rank -= count;
		key += 1;
	}
	if(key == keys) {
		return(UINT64_MAX);
	}

	uint64_t min = get_minval(hg, key);
	uint64_t max = get_maxval(hg, key);
	uint64_t count = get_key_count(hg, key);
	return(min + interpolate(max - min, rank, count));
}

uint64_t
hg64_rank_of_value(hg64 *hg, uint64_t value) {
	unsigned binsize = BINSIZE(hg);
	unsigned key = get_key(hg, value);
	unsigned k0 = key - key % binsize;
	uint64_t rank = 0;

	for(unsigned k = 0; k < k0; k += binsize) {
		rank += get_bin_count(hg, k);
	}
	for(unsigned k = k0; k < key; k += 1) {
		rank += get_key_count(hg, k);
	}

	uint64_t count = get_key_count(hg, key);
	uint64_t min = get_minval(hg, key);
	uint64_t max = get_maxval(hg, key);
	return(rank + interpolate(count, value - min, max - min));
}

uint64_t
hg64_value_at_quantile(hg64 *hg, double q) {
	double pop = (double)hg64_population(hg);
	double rank = (q < 0.0 ? 0.0 : q > 1.0 ? 1.0 : q) * pop;
	return(hg64_value_at_rank(hg, (uint64_t)rank));
}

double
hg64_quantile_of_value(hg64 *hg, uint64_t value) {
	uint64_t rank = hg64_rank_of_value(hg, value);
	return((double)rank / (double)hg64_population(hg));
}

/**********************************************************************/

/*
 * https://fanf2.user.srcf.net/hermes/doc/antiforgery/stats.pdf
 */
void
hg64_mean_variance(hg64 *hg, double *pmean, double *pvar) {
	unsigned keys = KEYS(hg);
	double pop = 0.0;
	double mean = 0.0;
	double sigma = 0.0;
	for(unsigned key = 0; key < keys; key++) {
		double min = (double)get_minval(hg, key) / 2.0;
		double max = (double)get_maxval(hg, key) / 2.0;
		double count = (double)get_key_count(hg, key);
		double delta = (min + max - mean);
		if(count != 0.0) {
			pop += count;
			mean += count * delta / pop;
			sigma += count * delta * (min + max - mean);
		}
	}
	OUTARG(pmean, mean);
	OUTARG(pvar, sigma / pop);
}

/**********************************************************************/

static void
validate_value(hg64 *hg, uint64_t value) {
	unsigned key = get_key(hg, value);
	uint64_t min = get_minval(hg, key);
	uint64_t max = get_maxval(hg, key);
	assert(key < KEYS(hg));
	assert(value >= min);
	assert(value <= max);
}

void
hg64_validate(hg64 *hg) {
	uint64_t min = 0, max = 1ULL << 16, step = 1ULL;
	for(uint64_t value = 0; value < max; value += step) {
		validate_value(hg, value);
	}
	min = 1ULL << 30, max = 1ULL << 40, step = 1ULL << 20;
	for(uint64_t value = min; value < max; value += step) {
		validate_value(hg, value);
	}
	max = UINT64_MAX, min = max >> 8, step = max >> 10;
	for(uint64_t value = max; value > min; value -= step) {
		validate_value(hg, value);
	}
	for(unsigned key = 1; key < KEYS(hg); key++) {
		assert(get_maxval(hg, key - 1) < get_minval(hg, key));
	}

	for(unsigned b = 0; b < BINS; b++) {
		uint64_t total = 0;
		struct bin *bin = &hg->bin[b];
		if(bin->count == NULL) {
			assert(bin->total == 0);
			continue;
		}
		for(unsigned c = 0; c < BINSIZE(hg); c++) {
			total += bin->count[c];
		}
		assert(total == bin->total);
	}
}

/**********************************************************************/
