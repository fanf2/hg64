#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hg64.h"

#define OUTARG(ptr, val) (((ptr) != NULL) && (*(ptr) = (val)))

#define PACKS (64 - 6)

struct hg64 {
	uint64_t total;
	size_t buckets;
	struct pack {
		uint64_t total;
		uint64_t bmp;
		uint64_t *bucket;
	} pack[PACKS];
};

/**********************************************************************/

hg64 *
hg64_create(void) {
	hg64 *hg = malloc(sizeof(*hg));
	*hg = (hg64){ 0 };
	return(hg);
}

void
hg64_destroy(hg64 *hg) {
	for(uint8_t exp = 0; exp < PACKS; exp++) {
		free(hg->pack[exp].bucket);
	}
	*hg = (hg64){ 0 };
	free(hg);
}

uint64_t
hg64_population(hg64 *hg) {
	return(hg->total);
}

size_t
hg64_buckets(hg64 *hg) {
	return(hg->buckets);
}

size_t
hg64_size(hg64 *hg) {
	return(sizeof(*hg) + hg->buckets * sizeof(uint64_t));
}

/**********************************************************************/

static inline uint64_t
interpolate(uint64_t range, uint64_t mul, uint64_t div) {
	double frac = (div == 0) ? 1 : (double)mul / (double)div;
	return((uint64_t)(range * frac));
}

static inline uint8_t
popcount(uint64_t bmp) {
	return((uint8_t)__builtin_popcountll((unsigned long long)bmp));
}

static inline uint8_t
get_exponent(uint64_t value) {
	if(value < 64) {
		return(0); /* denormal */
	} else {
		int clz = __builtin_clzll((unsigned long long)value);
		return((uint8_t)(63 - clz - 6));
	}
}

static inline uint8_t
get_mantissa(uint64_t value, uint8_t exponent) {
	return((value >> exponent) & 63);
}

static inline uint64_t
get_range(uint8_t exponent) {
	return(exponent == 0 ? 0 : 1ULL << exponent);
}

static inline uint64_t
get_base(uint8_t exponent, uint8_t mantissa) {
	uint64_t normalized = mantissa | (exponent == 0 ? 0 : 64);
	return(normalized << exponent);
}

static uint64_t *
get_bucket(hg64 *hg, uint8_t exponent, uint8_t mantissa, bool alloc) {
	struct pack *pack = &hg->pack[exponent];
	uint64_t bmp = pack->bmp;
	uint64_t bit = 1ULL << mantissa;
	uint64_t mask = bit - 1;
	uint8_t pos = popcount(bmp & mask);
	if((bmp & bit) == 0) {
		if(!alloc) {
			return(NULL);
		}
		uint8_t count = popcount(bmp);
		size_t alloc =  count + 1;
		size_t move = count - pos;
		size_t size = sizeof(uint64_t);
		uint64_t *ptr = realloc(pack->bucket, alloc * size);
		memmove(ptr + pos + 1, ptr + pos, move * size);
		hg->buckets += 1;
		pack->bmp |= bit;
		pack->bucket = ptr;
		pack->bucket[pos] = 0;
	}
	return(&pack->bucket[pos]);
}

/**********************************************************************/

void
hg64_inc(hg64 *hg, uint64_t value) {
	hg64_add(hg, value, 1);
}

void
hg64_add(hg64 *hg, uint64_t value, uint64_t count) {
	uint8_t exponent = get_exponent(value);
	uint8_t mantissa = get_mantissa(value, exponent);
	uint64_t *bucket = get_bucket(hg, exponent, mantissa, true);
	hg->pack[exponent].total += count;
	hg->total += count;
	*bucket += count;
}

bool
hg64_get(hg64 *hg, size_t i,
	     uint64_t *pmin, uint64_t *pmax, uint64_t *pcount) {
	uint8_t exponent = i / 64;
	uint8_t mantissa = i % 64;
	if(exponent >= PACKS) {
		return(false);
	}
	uint64_t min = get_base(exponent, mantissa);
	uint64_t max = min + get_range(exponent) - 1;
	uint64_t *bucket = get_bucket(hg, exponent, mantissa, false);
	uint64_t count = (bucket == NULL) ? 0 : *bucket;
	OUTARG(pmin, min);
	OUTARG(pmax, max);
	OUTARG(pcount, count);
	return(true);
}

/**********************************************************************/



uint64_t
hg64_value_at_rank(hg64 *hg, uint64_t rank) {
	uint8_t exponent, mantissa;
	uint64_t *bucket = NULL;
	struct pack *pack;

	assert(0 <= rank && rank < hg->total);
	for(exponent = 0; exponent < PACKS; exponent++) {
		pack = &hg->pack[exponent];
		if(rank < pack->total) {
			break;
		}
		rank -= pack->total;
	}
	assert(exponent < PACKS);

	for(mantissa = 0; mantissa < 64; mantissa++) {
		bucket = get_bucket(hg, exponent, mantissa, false);
		if(bucket == NULL) {
			continue;
		} else if(rank < *bucket) {
			break;
		} else {
			rank -= *bucket;
		}
	}
	assert(mantissa < 64);

	uint64_t base = get_base(exponent, mantissa);
	uint64_t range = get_range(exponent);
	uint64_t inter = interpolate(range, rank, *bucket);
	return(base + inter);
}

uint64_t
hg64_rank_of_value(hg64 *hg, uint64_t value) {
	uint8_t exponent = get_exponent(value);
	uint8_t mantissa = get_mantissa(value, exponent);
	uint64_t *bucket = get_bucket(hg, exponent, mantissa, false);

	uint64_t rank = 0;
	for(uint8_t exp = 0; exp < exponent; exp++) {
		rank += hg->pack[exp].total;
	}

	struct pack *pack = &hg->pack[exponent];
	for(uint8_t pos = 0; &pack->bucket[pos] < bucket; pos++) {
		rank += pack->bucket[pos];
	}

	uint64_t bit = 1ULL << mantissa;
	if(pack->bmp & bit) {
		uint64_t base = get_base(exponent, mantissa);
		uint64_t range = get_range(exponent);
		uint64_t inter = value - base;
		rank += interpolate(*bucket, inter, range);
	}

	return(rank);
}

uint64_t
hg64_value_at_quantile(hg64 *hg, double quantile) {
	assert(0.0 <= quantile && quantile <= 1.0);
	return(hg64_value_at_rank(hg, (uint64_t)(quantile * hg->total)));
}

double
hg64_quantile_of_value(hg64 *hg, uint64_t value) {
	uint64_t rank = hg64_rank_of_value(hg, value);
	return((double)rank / (double)hg->total);
}

/**********************************************************************/

void
hg64_mean_variance(hg64 *hg, double *pmean, double *pvar) {
	/* XXXFANF this is not numerically stable */
	double sum = 0.0;
	double squares = 0.0;
	for(uint8_t exp = 0; exp < PACKS; exp++) {
		for(uint8_t man = 0; man < 64; man++) {
			uint64_t value = get_base(exp, man)
				+ get_range(exp) / 2;
			uint64_t *bucket = get_bucket(hg, exp, man, false);
			uint64_t count = (bucket == NULL) ? 0 : *bucket;
			double total = (double)value * (double)count;
			sum += total;
			squares += total * (double)value;
		}
	}

	double mean = sum / hg->total;
	double square_of_mean = mean * mean;
	double mean_of_squares = squares / hg->total;
	double variance = mean_of_squares - square_of_mean;
	OUTARG(pmean, mean);
	OUTARG(pvar, variance);
}

/**********************************************************************/

void
hg64_validate(hg64 *hg) {
	uint64_t total = 0;
	uint64_t buckets = 0;
	for(uint8_t exp = 0; exp < PACKS; exp++) {
		uint64_t subtotal = 0;
		struct pack *pack = &hg->pack[exp];
		uint8_t count = popcount(pack->bmp);
		for(uint8_t man = 0; man < count; man++) {
			assert(pack->bucket[man] != 0);
			subtotal += pack->bucket[man];
		}
		assert((subtotal == 0) == (pack->bucket == NULL));
		assert((subtotal == 0) == (pack->bmp == 0));
		assert(subtotal == pack->total);
		total += subtotal;
		buckets += count;
	}
	assert(hg->total == total);
	assert(hg->buckets == buckets);
}

/**********************************************************************/
