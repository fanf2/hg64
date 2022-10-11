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


multithreading
--------------

This version of the code uses C11 atomics to support concurrent
updates by multiple threads.

Compared to previous versions, there is a different API for rank and
quantile calculations: you have to take a snapshot of the histogram
first. These calculations iterate over the histogram, using summary
totals so they don't have to look at every bucket. If the histogram is
updated concurrently, races can make a nonsense of these calculations
because we can't update both the bucket counter and the summary total
in one atomic operation.

Taking a snapshot also reduces the performance regression compared to
the single-threaded `without-popcount` branch: the `hg64_add()` and
`hg64_inc()` functions no longer need to maintain the summary totals:
they just increment one counter. The summary totals are calculated
when you take a snapshot.

The test program hammers the histogram from a varying number of
threads. Although it prints some time measurements, it is not a
realistic test: in real programs I expect histograms to be updated
relatively rarely, so in the typical case the histogram is not in a
fast cache and increments are not contended. However, in the test
program the histogram is in cache and highly contended. This means
that its single-threaded times are too optimistic, and for
multithreaded code they can be too pessimistic.


repositories
------------

  * https://dotat.at/cgi/git/hg64.git
  * https://github.com/fanf2/hg64


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

To find a bucket in its sparse array, `hg64` uses the CLZ (count
leading zeroes) compiler builtin.

C11 atomics are used to support multithreaded updates. Atomic
compare-and-swap is used when extending the histogram with a newly
allocated bin of counters. Counters are incremented atomically with
relaxed memory ordering to avoid unnecessary synchronization.

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
