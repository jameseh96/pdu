# pdu | pdump | pyprometheus

This repo contains two small C++ tools, `pdu` and `pdump`. These tools operate on Prometheus on-disk data, and provide insight into per-metric family disk usage and raw sample data respectively.

In addition, `pyprometheus` provides Python bindings supporting basic operations on Prometheus data including iterating all time series, and all samples therein.


Note: this has only been tested with Prometheus as embedded in Couchbase Server. No compatibility guarantees are made.


# pdu

A small tool to break down the disk usage of Prometheus chunk files by metric family.

## Getting Started


The Prometheus data directory probably looks a little like:


```
$ tree stats_data
stats_data
├── 01F5G2GWY1KV51STTKY40V2YB3
│   ├── chunks
│   │   └── 000001
│   ├── index
│   ├── meta.json
│   └── tombstones
├── 01F5GAK90QQ70NFKD1FM5CJNQM
│   ├── chunks
│   │   └── 000001
│   ├── index
│   ├── meta.json
│   └── tombstones
├── chunks_head
│   ├── 000003
│   └── 000004
├── queries.active
└── wal
    ├── 00000008
    ├── 00000009
    ├── 00000010
    ├── 00000011
    └── checkpoint.00000007
        └── 00000000
```

With a directory like that, pdu can be launched with:

```
$ pdu stats_data
12065592 kv_cmd_duration_seconds_bucket
542258   kv_cmd_duration_seconds_sum
436696   kv_cmd_duration_seconds_count
431900   kv_disk_seconds_bucket
259140   kv_sync_write_commit_duration_seconds_bucket
231249   kv_checkpoint_remover_seconds_bucket
214276   scrape_duration_seconds
172918   kv_cursor_get_all_items_time_seconds_bucket
123301   sysproc_page_faults_raw
123147   sysproc_minor_faults_raw
89701    kv_expiry_pager_seconds_bucket
86380    kv_storage_age_seconds_bucket
86380    kv_pending_ops_seconds_bucket
86380    kv_notify_io_seconds_bucket
86380    kv_item_pager_seconds_bucket
86380    kv_bg_wait_seconds_bucket
86380    kv_bg_load_seconds_bucket
72311    sysproc_mem_resident
66095    exposer_request_latencies
...
```

Where each line gives:

```
<bytes used> <metric family>
```

It may be convenient to sort this output, e.g., with

```
$ pdu --sort=size stats_data
```

To display only specific metric families, a filter regex can be used

```
$ pdu --filter=".*foobar.*" stats_data
```

This is applied to the metric family name only, and will not match labels.


The encoding of timestamps and values on disk is variable width; to produce a distribution of the bits used per sample:

```
$ pdu --bitwidth -hp stats_data
total
  Timestamps
  total size: 127 MB
     1b:   16893137   71.02% count,   12.64% size
    16b:    6690807   28.13% count,   80.09% size
    20b:        322    0.00% count,    0.00% size
    24b:       1778    0.01% count,    0.03% size
    48b:     201204    0.85% count,    7.23% size
    68b:        112    0.00% count,    0.01% size
  Values
  total size: 83 MB
     1b:   20926453   87.97% count,   23.93% size
     3b:        533    0.00% count,    0.00% size
     4b:       7521    0.03% count,    0.03% size
     5b:      16161    0.07% count,    0.09% size
     6b:      34733    0.15% count,    0.24% size
     7b:      55783    0.23% count,    0.45% size
     ...

```

#### Options

```
  -d [ --dir ] arg      Prometheus stats directory
  -c [ --total ]        Print total
  -s [ --summary ]      Print only summary
  -h [ --human ]        Use "human-readable" units
  -p [ --percent ]      Display percentage of total usage
  -S [ --sort ] arg     Sort output, valid values: "default", "size"
  -r [ --reverse ]      Reverse sort order
  -b [ --bitwidth ]     Display timestamp/value encoding bit width
                        distributions
  -f [ --filter ] arg   Regex filter applied to metric family names
```

This only considers bytes within chunk files - space used by the index file itself is not included, and WALs are ignored.

Expect the output to be an underestimate!

---

# pdump


`pdump` can be used to dump the raw time series data from a Prometheus data directory.

```
$ pdump stats_data
__name__ scrape_duration_seconds
instance kv
job general

1621268075527 0.00226225
1621268085527 0.00226225
1621268095527 0.00226225
1621268105527 0.00226225
1621268115527 0.00226225
...

```

This is structured as follows

```
labelKey labelValue
labelKey labelValue
labelKey labelValue

timestamp value
timestamp value
timestamp value
...

labelKey labelValue
...
```

Each section is separated by an empty line.

Alternative output formats and filtering options will be implemented in the near future.

---

# pyprometheus

This module provides very basic access to the data contained in Prometheus on-disk files from Python.

To use, follow the below [Installation instructions](#installing). This will produce a platform-dependent module - for example on MacOS this may be named `pyprometheus.cpython-39-darwin.so`.

This can be found either in `<build dir>/src/pyprometheus` or in your chosen installation prefix. To use this, ensure the containing directory is in your `PYTHONPATH`.

Example usage:


```
#!/usr/bin/env python3

import pyprometheus as pyp

data = pyp.load("/path/to/stats_data")

for series, samples in data:
    print(series.labels)
    for timestamp, value in samples:
        print(f"{timestamp} : {value}")
``` 


## Prerequisites

[Conan](https://conan.io/) - [installation instructions](https://docs.conan.io/en/latest/installation.html).

## Installing


```
git clone https://github.com/jameseh96/pdu.git pdu
cd ./pdu
git submodule update --init --recursive
mkdir ./build
cd !$
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
make install
```

## Built With

* [Boost](https://github.com/boostorg/boost)
