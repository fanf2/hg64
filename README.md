hg64 - a 64-bit histogram / quantile sketch
===========================================

`hg64` is a proof-of-concept implementation in C of a few ideas for
collecting summary statistics of a data stream. My aim is to be able
to record timing information in BIND, such as how long zone transfers
take, or how long it takes to compact a qp-trie.

A `hg64` is a histogram of `uint64_t` values. Values are assigned to
buckets by rounding them with about 1.5% relative accuracy, or about
two decimal digits of precision.


space requirements
------------------

The minimum size of the histogram is 1.3 KiB, and each non-empty
bucket uses an additional 8 bytes. There can be up to 3712 buckets,
so the maximum size of the histogram is 30.3 KiB.

It's normal to have a few hundred buckets.

When recording timings that can vary from a few milliseconds to a few
hours, there is a factor of about a million (`2^20`) between the
smallest and the largest times. This range can be covered by about
`20 * 64 = 1280` buckets, or about 11 KiB.


insertion performance
---------------------

On my MacBook it takes about 4-6 ms to ingest a million data points
(4-6 ns per item).

This includes the time it takes the data structure to warm up by
allocating the memory needed to cover the range of values in the data
stream.

This is single-threaded performance; the code does not support
concurrent insertion by multiple threads.


building
--------

Run `make`, which should create a `test` program that performs a few
simple tests on `hg64`.

There are not many requirements beyond standard C:

  * The test harness uses `clock_gettime()` from POSIX.

  * The `hg64` code itself uses a couple of special compiler builtins
    described below.


CPU features
------------

To find a bucket in its sparse array, `hg64` uses CLZ (count leading
zeroes) and POPCNT (population count) compiler builtins.

Floating point is not used when ingesting data. It is used when
querying summary statistics about the data:

  * When calculating ranks and quantiles, `hg64` uses floating point
    to interpolate within the range of a bucket.

  * The mean and variance are calculated in floating point, so that
    they are useful for histograms of small values.

  * There is no dependency on `libm`, which is why `hg64` calculates
    the variance not the standard deviation. You can call `sqrt()` if
    you need to.


contributing
------------

Please send bug reports, suggestions, and patches by email to me,
Tony Finch <<dot@dotat.at>>. Any contribution that you want
included in this code must be licensed under the [CC0 1.0 Public
Domain Dedication][CC0], and must include a `Signed-off-by:` line
to certify that you wrote it or otherwise have the right to pass
it on as a open-source patch, according to the [Developer's
Certificate of Origin 1.1][dco].

[cc0]: <https://creativecommons.org/publicdomain/zero/1.0/>
[dco]: <https://developercertificate.org>


licence
-------

> Written by Tony Finch <<dot@dotat.at>> <<fanf@isc.org>>
> You may do anything with this. It has no warranty.  
> <https://creativecommons.org/publicdomain/zero/1.0/>  
> SPDX-License-Identifier: CC0-1.0
