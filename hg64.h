/*
 * hg64 - 64-bit histograms
 *
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

typedef struct hg64 hg64;
typedef struct hg64s hg64s;

/*
 * Allocate a new histogram. `sigbits` must be between 1 and 15
 * inclusive; it is the number of significant bits of each value
 * to use when mapping values to buckets.
 */
hg64 *hg64_create(unsigned sigbits);

/*
 * Free the memory used by a histogram
 */
void hg64_destroy(hg64 *hg);

/*
 * Get the histogram's `sigbits` setting
 */
unsigned hg64_sigbits(hg64 *hg);

/*
 * Calculate the memory used in bytes
 */
size_t hg64_size(hg64 *hg);

/*
 * Add 1 to the value's bucket
 */
void hg64_inc(hg64 *hg, uint64_t value);

/*
 * Add an arbitrary increment to the value's bucket
 */
void hg64_add(hg64 *hg, uint64_t value, uint64_t inc);

/*
 * Get information about a bucket. This can be used as an iterator,
 * by initializing `key` to zero and incrementing by one or using
 * `hg64_next()` until `hg64_get()` returns `false`. The number of
 * iterations is a little less than `1 << (6 + sigbits)`.
 *
 * If `pmin` is non-NULL it is set to the bucket's minimum inclusive value.
 *
 * If `pmax` is non-NULL it is set to the bucket's maximum inclusive value.
 *
 * If `pcount` is non-NULL it is set to the bucket's counter, which
 * can be zero. (Empty buckets are included in the iterator.)
 */
bool hg64_get(hg64 *hg, unsigned key,
		  uint64_t *pmin, uint64_t *pmax, uint64_t *pcount);

/*
 * Skip to the next key, omitting groups of nonexistent buckets.
 */
unsigned hg64_next(hg64 *hg, unsigned key);

/*
 * Get summary statistics about the histogram.
 *
 * If `pmean` is non-NULL it is set to the mean of the recorded data.
 *
 * If `pvar` is non-NULL it is set to the variance of the recorded
 * data. The standard deviation is the square root of the variance.
 */
void hg64_mean_variance(hg64 *hg, double *pmean, double *pvar);

/*
 * Get a snapshot of a histogram for rank and quantile calculations.
 * When you have finished with it, just free() it.
 */
hg64s *hg64_snapshot(hg64 *hg);

/*
 * Get the approximate value at a given rank in the recorded data.
 * The rank must be less than the histogram's population.
 */
uint64_t hg64s_value_at_rank(const hg64s *hs, uint64_t rank);

/*
 * Get the approximate value at a given quantile in the recorded data.
 * The quantile must be >= 0.0 and < 1.0
 *
 * Quantiles are percentiles divided by 100. The median is quantile 1/2.
 */
uint64_t hg64s_value_at_quantile(const hg64s *hs, double quantile);

/*
 * Get the approximate rank of a value in the recorded data.
 * You can query the rank of any value.
 */
uint64_t hg64s_rank_of_value(const hg64s *hs, uint64_t value);

/*
 * Get the approximate quantile of a value in the recorded data.
 */
double hg64s_quantile_of_value(const hg64s *hs, uint64_t value);

/*
 * Increase the counts in `target` by the counts recorded in `source`
 */
void hg64_merge(hg64 *target, hg64 *source);

/* TODO */

/*
 * Serialize the histogram into `buffer`, which has `size` bytes
 * available. Returns the number of bytes required; if the return
 * value is greater than `size` the output has been truncated.
 */
size_t hg64_export(hg64 *hg, uint8_t *buffer, size_t size);
