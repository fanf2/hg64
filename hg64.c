/*
 * hg64 - 64-bit histograms
 *
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 * You may do anything with this. It has no warranty.
 * <https://creativecommons.org/publicdomain/zero/1.0/>
 * SPDX-License-Identifier: CC0-1.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hg64.h"

/*
 * This data structure is a sparse array of buckets. Keys are 12 bits by
 * default, but the size and accuracy can be reduced by changing
 * KEYBITS. (10 is a reasonable alternative, or 8 for low precision)
 *
 * Each bucket stores a count of values belonging to the bucket's key.
 *
 * The upper MANBITS of the key are used to index the `pack` array. Each
 * `pack` contains a packed sparse array of buckets indexed by the lower
 * `log2(PACKSIZE) == 6` bits of the key.
 *
 * A pack uses `popcount()` to avoid storing empty buckets. The `bmp`
 * is a bitmap indicating which buckets are present. There is also a
 * `count` summing all the buckets in the pack so that we can work with
 * quantiles faster.
 *
 * Values are `uint64_t`. They are mapped to buckets using a simplified
 * floating-point format. The upper six bits of the key are the
 * exponent, indicating the position of the most significant bit in the
 * 64-bit value. The lower MANBITS of the key are the mantissa; any less
 * significant bits in the value are discarded, which rounds the value
 * to its bucket's nominal value. Like IEEE 754, the most significant
 * bit is not included in the mantissa, except for very small values
 * (less than MANSIZE) which use a denormal format.
 */

#ifndef KEYBITS
#define KEYBITS 12
#endif

#define EXPBITS 6 /* log2(VALUEBITS) */
#define MANBITS (KEYBITS - EXPBITS)
#define MANSIZE (1 << MANBITS)
#define KEYSIZE (1 << KEYBITS)

#define PACKSIZE 64 /* same as BMPBITS */
#define PACKS (KEYSIZE / PACKSIZE)

/*
 * We waste a little extra space in the PACKS array that could be saved
 * by omitting exponents that aren't needed by denormal numbers, but the
 * arithmetic gets awkward for smaller KEYBITS. However we need the
 * exact number of keys for accurate bounds checks.
 */
#define DENORMALS (MANBITS - 1)
#define EXPONENTS (KEYSIZE / MANSIZE - DENORMALS)
#define KEYS (EXPONENTS * MANSIZE)

struct hg64 {
	struct pack {
		uint64_t count;
		uint64_t bmp;
		uint64_t *bucket;
	} pack[PACKS];
};

/**********************************************************************/

#define OUTARG(ptr, val) (((ptr) != NULL) && (bool)(*(ptr) = (val)))

static inline uint64_t
interpolate(uint64_t span, uint64_t mul, uint64_t div) {
	double frac = (div == 0) ? 1 : (double)mul / (double)div;
	return((uint64_t)(span * frac));
}

static inline unsigned
popcount(uint64_t bmp) {
	return(__builtin_popcountll((unsigned long long)bmp));
}

/**********************************************************************/

hg64 *
hg64_create(void) {
	hg64 *hg = malloc(sizeof(*hg));
	*hg = (hg64){ 0 };
	return(hg);
}

void
hg64_destroy(hg64 *hg) {
	for(unsigned p = 0; p < PACKS; p++) {
		free(hg->pack[p].bucket);
	}
	*hg = (hg64){ 0 };
	free(hg);
}

uint64_t
hg64_population(hg64 *hg) {
	uint64_t pop = 0;
	for(unsigned p = 0; p < PACKS; p++) {
		pop += hg->pack[p].count;
	}
	return(pop);
}

size_t
hg64_buckets(hg64 *hg) {
	size_t buckets = 0;
	for(unsigned p = 0; p < PACKS; p++) {
		buckets += popcount(hg->pack[p].bmp);
	}
	return(buckets);
}

size_t
hg64_size(hg64 *hg) {
	return(sizeof(*hg) + sizeof(uint64_t) * hg64_buckets(hg));
}

unsigned
hg64_keybits(void) {
	return(KEYBITS);
}

/**********************************************************************/

static inline uint64_t
get_minval(unsigned key) {
	unsigned exponent = key / MANSIZE - 1;
	uint64_t mantissa = key % MANSIZE + MANSIZE;
	return(key < MANSIZE ? key : mantissa << exponent);
}

/* don't shift by 64; reduce shift by 2 and pre-shift UINT64_MAX */
static inline uint64_t
get_maxval(unsigned key) {
	unsigned shift = PACKSIZE - key / MANSIZE - 1;
	uint64_t range = UINT64_MAX/4 >> shift;
	return(get_minval(key) + range);
}

static inline unsigned
get_key(uint64_t value) {
	/* hot path */
	if(value < MANSIZE) {
		return(value); /* denormal */
	} else {
		int clz = __builtin_clzll((unsigned long long)value);
		unsigned exponent = PACKSIZE - MANBITS - clz;
		unsigned mantissa = value >> (exponent - 1);
		return(exponent * MANSIZE + mantissa % MANSIZE);
	}
}

/*
 * Here we have fun indexing into a pack, and expanding if if necessary.
 */
static inline uint64_t *
get_bucket(hg64 *hg, unsigned key, bool nullable) {
	/* hot path */
	struct pack *pack = &hg->pack[key / PACKSIZE];
	uint64_t bit = 1ULL << (key % PACKSIZE);
	uint64_t mask = bit - 1;
	uint64_t bmp = pack->bmp;
	unsigned pos = popcount(bmp & mask);
	if(bmp & bit) {
		return(&pack->bucket[pos]);
	}
	/* cold path */
	if(nullable) {
		return(NULL);
	}
	unsigned pop = popcount(bmp);
	size_t need = pop + 1;
	size_t move = pop - pos;
	uint64_t *ptr = realloc(pack->bucket, need * sizeof(uint64_t));
	memmove(ptr + pos + 1, ptr + pos, move * sizeof(uint64_t));
	pack->bmp |= bit;
	pack->bucket = ptr;
	pack->bucket[pos] = 0;
	return(&pack->bucket[pos]);
}

static inline void
bump_count(hg64 *hg, unsigned key, uint64_t count) {
	/* hot path */
	hg->pack[key / PACKSIZE].count += count;
	*get_bucket(hg, key, false) += count;
}

static inline uint64_t
get_key_count(hg64 *hg, unsigned key) {
	uint64_t *bucket = get_bucket(hg, key, true);
	return(bucket == NULL ? 0 : *bucket);
}

static inline uint64_t
get_pack_count(hg64 *hg, unsigned key) {
	return(hg->pack[key / PACKSIZE].count);
}

/**********************************************************************/

void
hg64_add(hg64 *hg, uint64_t value, uint64_t count) {
	if(count > 0) {
		bump_count(hg, get_key(value), count);
	}
}

void
hg64_inc(hg64 *hg, uint64_t value) {
	hg64_add(hg, value, 1);
}

bool
hg64_get(hg64 *hg, unsigned key,
		uint64_t *pmin, uint64_t *pmax, uint64_t *pcount) {
	if(key < KEYS) {
		OUTARG(pmin, get_minval(key));
		OUTARG(pmax, get_maxval(key));
		OUTARG(pcount, get_key_count(hg, key));
		return(true);
	} else {
		return(false);
	}
}

void
hg64_merge(hg64 *target, hg64 *source) {
	for(unsigned key = 0; key < KEYS; key++) {
		uint64_t count = get_key_count(source, key);
		if(count > 0) {
			bump_count(target, key, count);
		}
	}
}

/**********************************************************************/

uint64_t
hg64_value_at_rank(hg64 *hg, uint64_t rank) {
	unsigned key = 0;
	while(key < KEYS) {
		uint64_t count = get_pack_count(hg, key);
		if(rank < count) {
			break;
		}
		rank -= count;
		key += PACKSIZE;
	}
	if(key == KEYS) {
		return(UINT64_MAX);
	}

	unsigned stop = key + PACKSIZE;
	while(key < stop) {
		uint64_t count = get_key_count(hg, key);
		if(rank < count) {
			break;
		}
		rank -= count;
		key += 1;
	}
	if(key == KEYS) {
		return(UINT64_MAX);
	}

	uint64_t min = get_minval(key);
	uint64_t max = get_maxval(key);
	uint64_t count = get_key_count(hg, key);
	return(min + interpolate(max - min, rank, count));
}

uint64_t
hg64_rank_of_value(hg64 *hg, uint64_t value) {
	unsigned key = get_key(value);
	unsigned k0 = key - key % PACKSIZE;
	uint64_t rank = 0;

	for(unsigned k = 0; k < k0; k += PACKSIZE) {
		rank += get_pack_count(hg, k);
	}
	for(unsigned k = k0; k < key; k += 1) {
		rank += get_key_count(hg, k);
	}

	uint64_t count = get_key_count(hg, key);
	uint64_t min = get_minval(key);
	uint64_t max = get_maxval(key);
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
	double pop = 0.0;
	double mean = 0.0;
	double sigma = 0.0;
	for(unsigned key = 0; key < KEYS; key++) {
		double min = (double)get_minval(key) / 2.0;
		double max = (double)get_maxval(key) / 2.0;
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
validate_value(uint64_t value) {
		unsigned key = get_key(value);
		uint64_t min = get_minval(key);
		uint64_t max = get_maxval(key);
		assert(key < KEYS);
		assert(key / PACKSIZE < PACKS);
		assert(value >= min);
		assert(value <= max);
}

void
hg64_validate(hg64 *hg) {
	uint64_t min = 0, max = 1ULL << 16, step = 1ULL;
	for(uint64_t value = 0; value < max; value += step) {
		validate_value(value);
	}
	min = 1ULL << 30, max = 1ULL << 40, step = 1ULL << 20;
	for(uint64_t value = min; value < max; value += step) {
		validate_value(value);
	}
	max = UINT64_MAX, min = max >> 8, step = max >> 10;
	for(uint64_t value = max; value > min; value -= step) {
		validate_value(value);
	}
	for(unsigned key = 1; key < KEYS; key++) {
		assert(get_maxval(key - 1) < get_minval(key));
	}

	for(unsigned p = 0; p < PACKS; p++) {
		uint64_t count = 0;
		struct pack *pack = &hg->pack[p];
		unsigned pop = popcount(pack->bmp);
		for(unsigned pos = 0; pos < pop; pos++) {
			assert(pack->bucket[pos] != 0);
			count += pack->bucket[pos];
		}
		assert((count == 0) == (pack->bucket == NULL));
		assert((count == 0) == (pack->bmp == 0));
		assert(count == pack->count);
	}
}

/**********************************************************************/
