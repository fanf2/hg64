#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * The structure is a trie with a fixed depth. At each level the child
 * nodes are held in a popcount packed array of up to 64 elements.
 * Three levels of trie can be indexed with 3*log(64) = 3*6 = 18 bits.
 *
 * The histogram bucket size gamma is calculated from an accuracy
 * parameter alpha. Values are stored as some integer power of gamma;
 * rounding to an integer is what discards up to alpha of accuracy to
 * assign the value to a bucket.
 *
 * The power of gamma is an 18 bit number, which can cover most of the
 * range of double precision numbers to an accuracy of 0.2%.
 */

#define EXPO_BITS (3 * 6)
#define EXPO_ZERO 0
#define EXPO_INF  ((1 << EXPO_BITS) - 1)
#define EXPO_BIAS (1 << (EXPO_BITS - 1))
#define EXPO_MIN  (0.0 - EXPO_BIAS)
#define EXPO_MAX  (EXPO_BIAS - 1.0)

struct bag0 {
	uint64_t total0;
	uint64_t bmp0;
	uint64_t *bag0;
};

struct bag1 {
	uint64_t total1;
	uint64_t bmp1;
	struct bag0 *bag1;
};

struct bag2 {
	uint64_t total2;
	uint64_t bmp2;
	struct bag1 *bag2;
};

typedef struct histobag {
	double alpha, beta;
	uint64_t buckets;
	uint64_t baggage;
	struct bag2 trie;
} histobag;

static inline uint32_t
key_of_value(histobag *h, double value) {
	assert(0.0 <= value);
	double expo = ceil(log(value) * h->beta);
	return(expo < EXPO_MIN ? EXPO_ZERO :
	       expo > EXPO_MAX ? EXPO_INF :
	       expo + EXPO_BIAS);
}

static double
value_of_key(histobag *h, uint32_t key) {
	double expo = (double)key - EXPO_BIAS;
	return(exp(expo / h->beta) * (1 - h->alpha));
}

static inline uint8_t
popcount(uint64_t bmp) {
	return((uint8_t)__builtin_popcountll((unsigned long long)bmp));
}

static inline uint8_t
least(uint64_t bmp) {
	return((uint8_t)__builtin_ctzll((unsigned long long)bmp));
}

static inline size_t
lesser(uint64_t bmp, uint64_t bit) {
	return(popcount(bmp & (bit - 1)));
}

static inline bool
missing(uint64_t bmp, uint64_t bit) {
	return((bmp & bit) == 0);
}

histobag *
histobag_create(double alpha) {
	double gamma = (1 + alpha) / (1 - alpha);
	double beta = 1 / log(gamma);
	histobag *h = malloc(sizeof(*h));
	*h = (histobag){
		.alpha = alpha,
		.beta = beta,
	};
	return(h);
}

void
histobag_destroy(histobag *h) {
	struct bag2 *bag2 = &h->trie;
	uint8_t pop2 = popcount(bag2->bmp2);
	for(uint8_t i2 = 0; i2 < pop2; i2++) {
		struct bag1 *bag1 = bag2->bag2 + i2;
		uint8_t pop1 = popcount(bag1->bmp1);
		for(uint8_t i1 = 0; i1 < pop1; i1++) {
			struct bag0 *bag0 = bag1->bag1 + i1;
			free(bag0->bag0);
		}
		free(bag1->bag1);
	}
	free(bag2->bag2);
	*h = (histobag){ 0 };
	free(h);
}

void
histobag_validate(histobag *h) {
	uint64_t buckets = 0;
	uint64_t baggage = 0;

	struct bag2 *bag2 = &h->trie;
	uint8_t pop2 = popcount(bag2->bmp2);
	uint64_t total2 = 0;
	for(uint8_t i2 = 0; i2 < pop2; i2++) {
		struct bag1 *bag1 = bag2->bag2 + i2;
		uint8_t pop1 = popcount(bag1->bmp1);
		uint64_t total1 = 0;
		for(uint8_t i1 = 0; i1 < pop1; i1++) {
			struct bag0 *bag0 = bag1->bag1 + i1;
			uint8_t pop0 = popcount(bag0->bmp0);
			uint64_t total0 = 0;
			for(uint8_t i0 = 0; i0 < pop0; i0++) {
				uint64_t *bucket = bag0->bag0 + i0;
				total0 += *bucket;
				buckets++;
			}
			assert(bag0->total0 == total0);
			total1 += total0;
			baggage++;
		}
		assert(bag1->total1 == total1);
		total2 += total1;
		baggage++;
	}
	assert(bag2->total2 == total2);
	assert(h->buckets == buckets);
	assert(h->baggage == baggage);
}

bool
histobag_next(histobag *h, double *value, size_t *count) {
	/*
	 * our first call has value == 0.0 in which case we must find
	 * the first bit at each level, so we want the masks to be -1
	 */
	uint64_t first = -(*value == 0.0);
	uint32_t key = key_of_value(h, *value);

	/* walk down the trie for the current value */

	struct bag2 *bag2 = &h->trie;
	uint8_t shift2 = 63 & (key >> 12);
	uint64_t bit2 = 1ULL << shift2;
	assert(!missing(bag2->bmp2, bit2) || first);

	struct bag1 *bag1 = bag2->bag2 + lesser(bag2->bmp2, bit2);
	uint8_t shift1 = 63 & (key >> 6);
	uint64_t bit1 = 1ULL << shift1;
	assert(!missing(bag1->bmp1, bit1) || first);

	struct bag0 *bag0 = bag1->bag1 + lesser(bag1->bmp1, bit1);
	uint8_t shift0 = 63 & (key >> 0);
	uint64_t bit0 = 1ULL << shift0;
	assert(!missing(bag0->bmp0, bit0) || first);

	/* bump to next bit and propagate carries */

	uint64_t mask0 = (bit0 - 1) | bit0;
	uint64_t bmp0 = bag0->bmp0 & (~mask0 | first);
	if(bmp0 != 0) {
		shift0 = least(bmp0);
	} else {
		uint64_t mask1 = (bit1 - 1) | bit1;
		uint64_t bmp1 = bag1->bmp1 & (~mask1 | first);
		if(bmp1 != 0) {
			shift1 = least(bmp1);
		} else {
			uint64_t mask2 = (bit2 - 1) | bit2;
			uint64_t bmp2 = bag2->bmp2 & (~mask2 | first);
			if(bmp2 != 0) {
				shift2 = least(bmp2);
			} else {
				return(false);
			}

			bit2 = 1ULL << shift2;
			bag1 = bag2->bag2 + lesser(bag2->bmp2, bit2);
			shift1 = least(bag1->bmp1);
		}

		bit1 = 1ULL << shift1;
		bag0 = bag1->bag1 + lesser(bag1->bmp1, bit1);
		shift0 = least(bag0->bmp0);
	}

	bit0 = 1ULL << shift0;
	uint64_t *bucket = bag0->bag0 + lesser(bag0->bmp0, bit0);

	key = (shift2 << 12) | (shift1 << 6) | (shift0 << 0);
	*value = value_of_key(h, key);
	*count = *bucket;
	return(true);
}

#define INSERT(bag, bmp, bit) \
	((bag) = insert(bag, sizeof(*(bag)), &(bmp), bit))

static void *
insert(void *bag, size_t elem_size, uint64_t *pbmp, uint64_t bit) {
	uint64_t bmp = *pbmp | bit;
	size_t all = elem_size * popcount(bmp);
	size_t lo = elem_size * lesser(bmp, bit);
	size_t hi = all - lo;
	uint8_t *bytes = realloc(bag, all);
	assert(bytes != NULL);
	memmove(bytes + lo + 1, bytes + lo, hi);
	memset(bytes + lo, 0, elem_size);
	*pbmp = bmp;
	return(bytes);
}

void
histobag_add(histobag *h, double value, size_t count) {
	uint32_t key = key_of_value(h, value);

	struct bag2 *bag2 = &h->trie;
	bag2->total2 += count;

	__builtin_prefetch(bag2->bag2);
	uint8_t shift2 = 63 & (key >> 12);
	uint64_t bit2 = 1ULL << shift2;
	if(missing(bag2->bmp2, bit2)) {
		INSERT(bag2->bag2, bag2->bmp2, bit2);
		h->baggage++;
	}

	struct bag1 *bag1 = bag2->bag2 + lesser(bag2->bmp2, bit2);
	bag1->total1 += count;

	__builtin_prefetch(bag1->bag1);
	uint8_t shift1 = 63 & (key >> 6);
	uint64_t bit1 = 1ULL << shift1;
	if(missing(bag1->bmp1, bit1)) {
		INSERT(bag1->bag1, bag1->bmp1, bit1);
		h->baggage++;
	}

	struct bag0 *bag0 = bag1->bag1 + lesser(bag1->bmp1, bit1);
	bag0->total0 += count;

	__builtin_prefetch(bag0->bag0);
	uint8_t shift0 = 63 & (key >> 0);
	uint64_t bit0 = 1ULL << shift0;
	if(missing(bag0->bmp0, bit0)) {
		INSERT(bag0->bag0, bag0->bmp0, bit0);
		h->buckets++;
	}

	uint64_t *bucket = bag0->bag0 + lesser(bag0->bmp0, bit0);
	*bucket += count;
}
