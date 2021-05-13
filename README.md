# pdu

A small tool to break down the disk usage of Prometheus chunk files by metric family.

## Getting Started

Note: this has only been tested with Prometheus as embedded in Couchbase Server. No compatibility guarantees are made.


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
12065592 : kv_cmd_duration_seconds_bucket
542258   : kv_cmd_duration_seconds_sum
436696   : kv_cmd_duration_seconds_count
431900   : kv_disk_seconds_bucket
259140   : kv_sync_write_commit_duration_seconds_bucket
231249   : kv_checkpoint_remover_seconds_bucket
214276   : scrape_duration_seconds
172918   : kv_cursor_get_all_items_time_seconds_bucket
123301   : sysproc_page_faults_raw
123147   : sysproc_minor_faults_raw
89701    : kv_expiry_pager_seconds_bucket
86380    : kv_storage_age_seconds_bucket
86380    : kv_pending_ops_seconds_bucket
86380    : kv_notify_io_seconds_bucket
86380    : kv_item_pager_seconds_bucket
86380    : kv_bg_wait_seconds_bucket
86380    : kv_bg_load_seconds_bucket
72311    : sysproc_mem_resident
66095    : exposer_request_latencies
...
```

It may be convenient to sort this output, e.g., with

```
$ pdu post_stats | sort -hr | head -n 40
```


## Prerequisites

[Conan](https://conan.io/) - [installation instructions](https://docs.conan.io/en/latest/installation.html).

## Installing


```
git clone https://github.com/jameseh96/pdu.git pdu
cd ./pdu
mkdir ./build
cd !$
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
make install
```

## Built With

* [Boost](https://github.com/boostorg/boost)
