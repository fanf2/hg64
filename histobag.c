#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "histobag.h"

#define OUTARG(ptr, val) (((ptr) != NULL) && (*(ptr) = (val)))

#define BAGS (64 - 6)

struct histobag {
	uint64_t total;
	size_t baggage;
	struct bag {
		uint64_t total;
		uint64_t bmp;
		uint64_t *bucket;
	} bag[BAGS];
};

/**********************************************************************/

histobag *
histobag_create(void) {
	histobag *h = malloc(sizeof(*h));
	*h = (histobag){ 0 };
	return(h);
}

void
histobag_destroy(histobag *h) {
	for(uint8_t exp = 0; exp < BAGS; exp++) {
		free(h->bag[exp].bucket);
	}
	*h = (histobag){ 0 };
	free(h);
}

uint64_t
histobag_population(histobag *h) {
	return(h->total);
}

size_t
histobag_buckets(histobag *h) {
	return(h->baggage);
}

size_t
histobag_size(histobag *h) {
	return(sizeof(*h) + h->baggage * sizeof(uint64_t));
}

/**********************************************************************/

static inline uint64_t
interpolate(uint64_t range, uint64_t mul, uint64_t div) {
	double frac = (double)mul / (double)div;
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
	return((1ULL << exponent) >> 1);
}

static inline uint64_t
get_base(uint8_t exponent, uint8_t mantissa) {
	uint64_t normalized = mantissa | (exponent == 0 ? 0 : 64);
	return(normalized << exponent);
}

static uint64_t *
get_bucket(histobag *h, uint8_t exponent, uint8_t mantissa, bool alloc) {
	struct bag *bag = &h->bag[exponent];
	uint64_t bmp = bag->bmp;
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
		uint64_t *ptr = realloc(bag->bucket, alloc * size);
		memmove(ptr + pos + 1, ptr + pos, move * size);
		h->baggage += 1;
		bag->bmp |= bit;
		bag->bucket = ptr;
		bag->bucket[pos] = 0;
	}
	return(&bag->bucket[pos]);
}

static uint8_t
bucket_position(histobag *h, uint8_t exponent, uint8_t mantissa) {
	struct bag *bag = &h->bag[exponent];
	uint64_t mask = (1ULL << mantissa) - 1;
	return(popcount(bag->bmp & mask));
}

/**********************************************************************/

bool
histobag_get(histobag *h, size_t i,
	     uint64_t *pmin, uint64_t *pmax, uint64_t *pcount) {
	uint8_t exponent = i / 64;
	uint8_t mantissa = i % 64;
	if(exponent >= BAGS) {
		return(false);
	}
	uint64_t min = get_base(exponent, mantissa);
	uint64_t max = min + get_range(exponent) - 1;
	uint64_t *bucket = get_bucket(h, exponent, mantissa, false);
	uint64_t count = (bucket == NULL) ? 0 : *bucket;
	OUTARG(pmin, min);
	OUTARG(pmax, max);
	OUTARG(pcount, count);
	return(true);
}

void
histobag_add(histobag *h, uint64_t value, uint64_t count) {
	if(count == 0) {
		return;
	}
	uint8_t exponent = get_exponent(value);
	uint8_t mantissa = get_mantissa(value, exponent);
	uint64_t *bucket = get_bucket(h, exponent, mantissa, true);
	h->bag[exponent].total += count;
	h->total += count;
	*bucket += count;
}

/**********************************************************************/

uint64_t
histobag_value_at_rank(histobag *h, uint64_t rank) {
	uint8_t exponent, mantissa, position;
	struct bag *bag;

	assert(0 <= rank && rank <= h->total);
	for(exponent = 0; exponent < BAGS; exponent++) {
		bag = &h->bag[exponent];
		if(rank <= bag->total) {
			break;
		}
		rank -= bag->total;
	}
	assert(exponent < BAGS);

	uint64_t *bucket = bag->bucket;
	for(mantissa = 0; mantissa < 64; mantissa++) {
		position = bucket_position(h, exponent, mantissa);
		if(rank <= bucket[position]) {
			break;
		}
		rank -= bucket[position];
	}
	assert(mantissa < 64);

	uint64_t base = get_base(exponent, mantissa);
	uint64_t range = get_range(exponent);
	uint64_t inter = interpolate(range, rank, bucket[position]);
	return(base + inter);
}

uint64_t
histobag_rank_of_value(histobag *h, uint64_t value) {
	uint8_t exponent = get_exponent(value);
	uint8_t mantissa = get_mantissa(value, exponent);
	uint8_t position = bucket_position(h, exponent, mantissa);

	uint64_t rank = 0;
	for(uint8_t exp = 0; exp < exponent; exp++) {
		rank += h->bag[exp].total;
	}

	struct bag *bag = &h->bag[exponent];
	for(uint8_t pos = 0; pos < position; pos++) {
		rank += bag->bucket[pos];
	}

	uint64_t bit = 1ULL << mantissa;
	if(bag->bmp & bit) {
		uint64_t base = get_base(exponent, mantissa);
		uint64_t range = get_range(exponent);
		uint64_t inter = value - base;
		rank += interpolate(bag->bucket[position], inter, range);
	}

	return(rank);
}

uint64_t
histobag_value_at_quantile(histobag *h, double quantile) {
	assert(0.0 <= quantile && quantile <= 1.0);
	return(histobag_value_at_rank(h, (uint64_t)(quantile * h->total)));
}

/**********************************************************************/

void
histobag_mean_sd(histobag *h, double *pmu, double *psd) {
	/* XXXFANF this is not numerically stable */
	double sum = 0.0;
	double squares = 0.0;
	for(uint8_t exp = 0; exp < BAGS; exp++) {
		for(uint8_t man = 0; man < 64; man++) {
			uint64_t value = get_base(exp, man)
				+ get_range(exp) / 2;
			uint64_t *bucket = get_bucket(h, exp, man, false);
			uint64_t count = (bucket == NULL) ? 0 : *bucket;
			double total = (double)value * (double)count;
			sum += total;
			squares += total * (double)value;
		}
	}

	double mean = sum / h->total;
	double square_of_mean = mean * mean;
	double mean_of_squares = squares / h->total;
	double sigma = sqrt(mean_of_squares - square_of_mean);
	OUTARG(pmu, mean);
	OUTARG(psd, sigma);
}

/**********************************************************************/

void
histobag_validate(histobag *h) {
	uint64_t total = 0;
	uint64_t baggage = 0;
	for(uint8_t exp = 0; exp < BAGS; exp++) {
		uint64_t subtotal = 0;
		struct bag *bag = &h->bag[exp];
		uint8_t count = popcount(bag->bmp);
		for(uint8_t man = 0; man < count; man++) {
			assert(bag->bucket[man] != 0);
			subtotal += bag->bucket[man];
		}
		assert((subtotal == 0) == (bag->bucket == NULL));
		assert((subtotal == 0) == (bag->bmp == 0));
		assert(subtotal == bag->total);
		total += subtotal;
		baggage += count;
	}
	assert(h->total == total);
	assert(h->baggage == baggage);
}

/**********************************************************************/
