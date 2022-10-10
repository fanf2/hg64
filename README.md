hg64 - a 64-bit histogram / quantile sketch
===========================================

`hg64` is a proof-of-concept implementation in C of a few ideas for
collecting summary statistics of a data stream. My aim is to be able
to record timing information in BIND, such as how long zone transfers
take, or memory statistics such as how effective qp-trie compaction is.

A `hg64` is a histogram of `uint64_t` values. Values are assigned to
buckets by rounding them with less than 1% relative error, or about
two decimal digits of precision. (Some quantile sketches aim to
satisfy a particular rank error requirement; in contrast, `hg64` is
designed around a target value error.)

The size of a `hg64` histogram depends on the range of values in the
data stream, not the length of the data stream. The update performance
is roughly constant, and mostly depends on cache locality.

You can adjust the number of bits used in a `hg64` key by defining the
`KEYBITS` preprocessor macro at compile time. Smaller keys require
less memory, but are less accurate. The default is 12 bits; 10 bits is
a reasonable alternative, or 8 bits for low precision.


caveat
------

This README describes the code in the `trunk` of this repository,
which corresponds to [my blog post about `hg64`][blog].

Since then I have made a simplified version in the `without-popcount`
branch. It uses more memory (it does not tightly pack the histogram
buckets) but it is a stepping-stone to lock-free multithreading.

The `multithreaded` branch is a prototype of a lock-free concurrent
histogram, much closer to something that would be suitable for BIND.

[blog]: https://dotat.at/@/2022-07-15-histogram.html


space requirements
------------------

With the default 12 bit keys, the minimum size of the histogram is 1.3
KiB, and each non-empty bucket uses an additional 8 bytes. There can
be up to 3712 buckets, so the maximum size of the histogram is 30.3
KiB.

It's normal to have a few hundred buckets.

When recording timings that can vary from a few milliseconds to a few
hours, there is a factor of about a million (`2^20`) between the
smallest and the largest times. This range can be covered by about
`20 * 64 = 1280` buckets, or about 11 KiB.

If you reduce the key size by one bit, it halves the minimum size of
the histogram, and halves the maximum number of buckets.


insertion performance
---------------------

A microbenchmark on my MacBook takes 4 or 5 ms to ingest a million
data points (4 or 5 ns per item).

This includes the time it takes the data structure to warm up by
allocating the memory needed to cover the range of values in the data
stream.

This is single-threaded performance; the code does not support
concurrent insertion by multiple threads. In a multithreaded
application, it's best to have a histogram per thread or per CPU,
and merge them when you need to get system-wide counts.

(The more recent code in the `without-popcount` and `multithreaded`
branches both take less than 2.5 ms, nearly twice as fast, still with
only one thread - I have not yet tested multithreded performance.)


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
zeroes) and POPCNT (count set bits) compiler builtins.

Floating point is not used when ingesting data. It is used when
querying summary statistics about the data:

  * When calculating ranks, `hg64` uses floating point multiplication
    and division to interpolate within the range of a bucket.

  * Quantiles are floating-point numbers between 0 and 1.

  * The mean and variance are calculated in floating point, so that
    they are useful for histograms of small values.

  * There is no dependency on `libm`, which is why `hg64` calculates
    the variance not the standard deviation. You can call `sqrt()` if
    you need to.


repositories
------------

  * https://dotat.at/cgi/git/hg64.git
  * https://github.com/fanf2/hg64


about the name
--------------

There are several ways that `hg64` uses the number 64:

  * Obviously, data values are 64 bits

  * The internal number representation has an exponent with 58 values
    and a mantissa with 64 values

  * The data structure is an array of 58 elements, each of which is a
    packed array of up to 64 buckets

  * Each packed array has a 64-wide occupancy bitmap

  * 58 is actually two 64s in disguise: it comes from `64 - log2(64)`

Altering `KEYBITS` changes the number of bits in the mantissa.


contributing
------------

Please send bug reports, suggestions, and patches by email to me, Tony
Finch <<dot@dotat.at>>, or via GitHub. Any contribution that you want
included in this code must be unrestricted (like the licence below),
and must include a `Signed-off-by:` line to certify that you wrote it
or otherwise have the right to pass it on as a open-source patch,
according to the [Developer's Certificate of Origin 1.1][dco].

[dco]: <https://developercertificate.org>


licence
-------

Written by Tony Finch <<dot@dotat.at>> <<fanf@isc.org>>

Permission is hereby granted to use, copy, modify, and/or
distribute this software for any purpose with or without fee.

This software is provided 'as is', without warranty of any kind.
In no event shall the authors be liable for any damages arising
from the use of this software.

    SPDX-License-Identifier: 0BSD OR MIT-0

_[this is a zero-conditions libre software licence](https://dotat.at/0lib.html)_
