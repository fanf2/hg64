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
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hg64.h"

/* number of bins is same as number of bits in a value */
#define BINS 64

typedef atomic_uint_fast64_t counter;
typedef _Atomic(counter *) ctr_ptr;

struct hg64 {
	unsigned sigbits;
	struct bin {
		counter total;
		ctr_ptr count;
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

static inline uint64_t
read_counter(counter *ctr) {
	return((uint64_t)atomic_load_explicit(ctr, memory_order_relaxed));
}

static inline void
bump_counter(counter *ctr, uint64_t increment) {
	uint_fast64_t inc = increment;
	atomic_fetch_add_explicit(ctr, inc, memory_order_relaxed);
}

static inline counter *
read_ctr_ptr(hg64 *hg, unsigned b) {
	ctr_ptr *cpp = &hg->bin[b].count;
	return(atomic_load_explicit(cpp, memory_order_acquire));
}

static inline counter *
set_ctr_ptr(hg64 *hg, unsigned b, counter *new_cp) {
	ctr_ptr *cpp = &hg->bin[b].count;
	counter *old_cp = NULL;
	if(atomic_compare_exchange_strong_explicit(cpp, &old_cp, new_cp,
			memory_order_acq_rel, memory_order_acquire)) {
		return(new_cp);
	} else {
		/* lost the race, so use the winner's counters */
		free(new_cp);
		return(old_cp);
	}
}

/**********************************************************************/

hg64 *
hg64_create(unsigned sigbits) {
	if(sigbits < 1 || 6 < sigbits) {
		return(NULL);
	}
	hg64 *hg = malloc(sizeof(*hg));
	hg->sigbits = sigbits;
	/*
	 * it is probably portable to zero-initialize atomics but the
	 * C standard says we shouldn't rely on it; but this loop
	 * should optimize to memset() on most target systems
	 */
	for (unsigned b = 0; b < BINS; b++) {
		atomic_init(&hg->bin[b].total, 0);
		atomic_init(&hg->bin[b].count, NULL);
	}
	return(hg);
}

void
hg64_destroy(hg64 *hg) {
	for(unsigned b = 0; b < BINS; b++) {
		free(read_ctr_ptr(hg, b));
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
		pop += read_counter(&hg->bin[b].total);
	}
	return(pop);
}

size_t
hg64_buckets(hg64 *hg) {
	size_t buckets = 0;
	for(unsigned b = 0; b < BINS; b++) {
		if(read_ctr_ptr(hg, b) != NULL) {
			buckets += BINSIZE(hg);
		}
	}
	return(buckets);
}

size_t
hg64_size(hg64 *hg) {
	return(sizeof(hg64) + sizeof(counter) * hg64_buckets(hg));
}

/**********************************************************************/

static inline uint64_t
key_to_minval(hg64 *hg, unsigned key) {
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
key_to_maxval(hg64 *hg, unsigned key) {
	unsigned sigtop = 1 << hg->sigbits;
	unsigned shift = 63 - (key / sigtop);
	uint64_t range = UINT64_MAX/4 >> shift;
	return(key_to_minval(hg, key) + range);
}

/*
 * This branchless conversion is due to Paul Khuong: see bin_down_of() in
 * http://pvk.ca/Blog/2015/06/27/linear-log-bucketing-fast-versatile-simple/
 */
static inline unsigned
value_to_key(hg64 *hg, uint64_t value) {
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

static inline counter *
bucket_counter(hg64 *hg, unsigned key, bool nullable) {
	/* hot path */
	unsigned sigtop = 1 << hg->sigbits;
	unsigned b = key / sigtop;
	unsigned c = key % sigtop;
	counter *cp = read_ctr_ptr(hg, b);
	if(cp != NULL) {
		return(cp + c);
	}
	/* cold path */
	if(nullable) {
		return(NULL);
	}
	size_t bytes = sizeof(counter) * sigtop;
	cp = malloc(bytes);
	/* see comment in hg64_create() above */
	for (unsigned i = 0; i < sigtop; i++) {
		atomic_init(cp + i, 0);
	}
	cp = set_ctr_ptr(hg, b, cp);
	return(cp + c);
}

static inline counter *
bin_total_counter(hg64 *hg, unsigned key) {
	return(&hg->bin[key >> hg->sigbits].total);
}

static inline void
bump_key(hg64 *hg, unsigned key, uint64_t inc) {
	/* hot path */
	bump_counter(bucket_counter(hg, key, false), inc);
	bump_counter(bin_total_counter(hg, key), inc);
}

static inline uint64_t
get_key_count(hg64 *hg, unsigned key) {
	counter *ctr = bucket_counter(hg, key, true);
	return(ctr == NULL ? 0 : read_counter(ctr));
}

static inline uint64_t
get_bin_total(hg64 *hg, unsigned key) {
	return(read_counter(bin_total_counter(hg, key)));
}

/**********************************************************************/

void
hg64_add(hg64 *hg, uint64_t value, uint64_t inc) {
	if(inc > 0) {
		bump_key(hg, value_to_key(hg, value), inc);
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
		OUTARG(pmin, key_to_minval(hg, key));
		OUTARG(pmax, key_to_maxval(hg, key));
		OUTARG(pcount, get_key_count(hg, key));
		return(true);
	} else {
		return(false);
	}
}

void
hg64_merge(hg64 *target, hg64 *source) {
	for(unsigned sk = 0; sk < KEYS(source); sk++) {
		uint64_t inc = get_key_count(source, sk);
		if(inc > 0) {
			unsigned tk = sk;
			if(source->sigbits > target->sigbits) {
				tk >>= +source->sigbits -target->sigbits;
			}
			if(source->sigbits < target->sigbits) {
				tk <<= -source->sigbits +target->sigbits;
			}
			bump_key(target, tk, inc);
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
		uint64_t count = get_bin_total(hg, key);
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

	uint64_t min = key_to_minval(hg, key);
	uint64_t max = key_to_maxval(hg, key);
	uint64_t count = get_key_count(hg, key);
	return(min + interpolate(max - min, rank, count));
}

uint64_t
hg64_rank_of_value(hg64 *hg, uint64_t value) {
	unsigned binsize = BINSIZE(hg);
	unsigned key = value_to_key(hg, value);
	unsigned k0 = key - key % binsize;
	uint64_t rank = 0;

	for(unsigned k = 0; k < k0; k += binsize) {
		rank += get_bin_total(hg, k);
	}
	for(unsigned k = k0; k < key; k += 1) {
		rank += get_key_count(hg, k);
	}

	uint64_t count = get_key_count(hg, key);
	uint64_t min = key_to_minval(hg, key);
	uint64_t max = key_to_maxval(hg, key);
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
		double min = (double)key_to_minval(hg, key) / 2.0;
		double max = (double)key_to_maxval(hg, key) / 2.0;
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
	unsigned key = value_to_key(hg, value);
	uint64_t min = key_to_minval(hg, key);
	uint64_t max = key_to_maxval(hg, key);
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
		assert(key_to_maxval(hg, key - 1) < key_to_minval(hg, key));
	}

	for(unsigned b = 0; b < BINS; b++) {
		uint64_t total = 0;
		if(read_ctr_ptr(hg, b) != NULL) {
			for(unsigned c = 0; c < BINSIZE(hg); c++) {
				total += get_key_count(hg, BINSIZE(hg) * b + c);
			}
		}
		assert(get_bin_total(hg, BINSIZE(hg) * b) == total);
	}
}

/**********************************************************************/
