# pypdu

This module provides basic read-only access to the data contained in Prometheus on-disk files from Python.

`pypdu` may be installed from pip (on linux and macOS):

```
pip install pypdu
```

Example usage:

```
#!/usr/bin/env python3

import pypdu

data = pypdu.load("/path/to/stats_data")

for series in data:
    print(series.name) # equivalent to series.labels["__name__"]
    print(series.labels)
    print(len(series.samples)) # number of samples can be computed
                               # without iterating all of them
    for sample in series.samples:
        print(f"{sample.timestamp} : {sample.value}")
``` 

Or the series and samples can be unpacked:


```
for name, labels, samples in data:
    print(name)
    print(labels)
    print(len(samples))
    for timestamp, value in samples:
        print(f"{timestamp} : {value}")
```

#### Filtering time series

If only a subset of the time series are desired, `pypdu` can filter them based on label values, and avoid parsing unneeded series at all:

```
for series in data.filter({"__name__":"sysproc_page_faults_raw"}):
```

This will usually perform better than filtering "manually" in python after the fact.

Multiple labels can be specified:

```
data.filter({"__name__":"sysproc_page_faults_raw", "proc":"memcached"})
```

ECMAScript regexes can also be used:

```
data.filter({"proc":pypdu.regex("^go.*")})
```

Or even arbitrary Python callbacks:

```
data.filter({"proc":lambda x: x.startswith("go")})
```


As shorthand, when filtering on `__name__` alone, just a string may be provided.

```
data.filter("sysproc_page_faults_raw")
```

#### Single series lookup

If there is only one time series matching your filter, for convenience you can do:

```
foobar_series = data[{"__name__":"foobar"}]
```

This is roughly equivalent to:

```
foobar_series = next(iter(data.filter({"__name__":"foobar"})))
```

If there are multiple time series matching your filter, this will silently discard all but the lexicographically first (sorted by the key and value of all labels).

If none match, a `KeyError` is raised.

All types of filter demonstrated above with `.filter(...)` may be used in this manner also.

#### Histograms

`PrometheusData(...).histograms` allows iterating all histograms represented by the time series in a data directory.

The histograms are exposed as `HistogramTimeSeries`, grouping all the component `..._bucket` time series together. Indexing into this series provides access to the histogram at a single point in time.

e.g.,

```
data = pypdu.load("<...>")

for histSeries in data.histograms:
    print("Labels: ", histSeries.labels)
    print("Number of samples: ", len(histSeries))
    for hist in histSeries:
        print("TS: ", hist.timestamp)
        print(hist.buckets())
```

Iterates over every histogram found in the Prometheus data, then iterates over every sample contained in that time series.

Example output:

```
Labels:  {'__name__': 'cm_http_requests_seconds', 'instance': 'ns_server', 'job': 'ns_server_high_cardinality'}
Number of samples:  3826
TS:  1621268098827
[(0.001, 8.0), (0.01, 25.0), (0.1, 25.0), (1.0, 25.0), (10.0, 25.0), (inf, 25.0)]
TS:  1621268158827
[(0.001, 39.0), (0.01, 118.0), (0.1, 126.0), (1.0, 127.0), (10.0, 127.0), (inf, 127.0)]
TS:  1621268218827
[(0.001, 43.0), (0.01, 132.0), (0.1, 140.0), (1.0, 141.0), (10.0, 141.0), (inf, 141.0)]
TS:  1621268278827
[(0.001, 48.0), (0.01, 145.0), (0.1, 153.0), (1.0, 154.0), (10.0, 154.0), (inf, 154.0)]
TS:  1621268338827
[(0.001, 53.0), (0.01, 158.0), (0.1, 166.0), (1.0, 167.0), (10.0, 167.0), (inf, 167.0)]
TS:  1621268398827
[(0.001, 55.0), (0.01, 171.0), (0.1, 179.0), (1.0, 180.0), (10.0, 180.0), (inf, 180.0)]
TS:  1621268458827
[(0.001, 60.0), (0.01, 191.0), (0.1, 199.0), (1.0, 200.0), (10.0, 200.0), (inf, 200.0)]
TS:  1621268518827
[(0.001, 66.0), (0.01, 204.0), (0.1, 212.0), (1.0, 213.0), (10.0, 213.0), (inf, 213.0)]
TS:  1621268578827
[(0.001, 71.0), (0.01, 217.0), (0.1, 225.0), (1.0, 226.0), (10.0, 226.0), (inf, 226.0)]
TS:  1621268638827
[(0.001, 73.0), (0.01, 230.0), (0.1, 238.0), (1.0, 239.0), (10.0, 239.0), (inf, 239.0)]
...
Labels: ...
```

`HistogramTimeSeries` (in the above example, this is `histSeries`), can be indexed into - currently
only by a sample index, but in the future, selecting the histogram closest to a given timestamp may be supported.

E.g., the first and last point in time view available for a specific histogram can be found with:

```
first = histSeries[0]
last = histSeries[-1]
```
From which the timestamp and buckets could be read:

```
>>> print(last.timestamp) # time since epoch in ms
1631007596974

>>> print(last.bucket_bounds()))
[0.001, 0.01, 0.1, 1.0, 10.0, inf]

>>> print(last.bucket_values())
[4279.0, 4371.0, 4666.0, 5044.0, 5044.0, 5044.0]

>>> print(last.buckets()) # convenience zip of (bounds, values)
[(0.001, 4279.0), (0.01, 4371.0), (0.1, 4666.0), (1.0, 5044.0), (10.0, 5044.0), (inf, 5044.0)]

```

#### Serialisation

Time series may be dumped individually to a file or bytes. This may be useful if you need to store some number of series (e.g., in a key-value store), but don't wish to retain the entire Prometheus data directory.

`pypdu.dump`/`pypdu.load` take an `int` file descriptor or, for convenience, a file-like object supporting `fileLike.fileno() -> int`.

This could be used to write output to a pipe or socket, not just a file on disk. Note, arbitrary file-like objects which are not backed by a file descriptor are not supported.

When `load`ing many series, the underlying data for all Series will be read into memory - this may be costly if there are many Series. `pypdu.load_lazy` can instead be used to consume Series one at a time.

`pypdu.dumps` creates a `bytes` object, while `pypdu.loads` operates on a [buffer](https://docs.python.org/3/c-api/buffer.html). Anything supporting the buffer protocol exposing a contiguous buffer may be used. This includes `bytes` objects, but also `numpy` arrays and many other types.

A [memoryview](https://docs.python.org/3/library/stdtypes.html#memoryview) may be used to slice a buffer, allowing deserialisation from _part_ of a buffer, without having to copy out the relevant bytes.



```
# fd : int or file-like object with .fileno() method

pypdu.dump(fd, series)
pypdu.dump(fd, [series, series, ...])
pypdu.dump(fd, PrometheusData)

# note, dumps on a lot of series will consume a lot of memory building
# a big bytes object
pypdu.dumps(series) -> bytes
pypdu.dumps([series, series, ...]) -> bytes
pypdu.dumps(PrometheusData) -> bytes

# result of load{,s} depends on what was written
# Deserialised series are entirely in-memory, may consume a lot of
# memory.
pypdu.load(fd) -> Series
pypdu.load(fd) -> [Series, Series,...]

pypdu.loads(buffer) -> Series
pypdu.loads(buffer) -> [Series, Series, ...]

# when loading a lot of series, this is the advised way to avoid
# holding them all in memory at the same time
pypdu.load_lazy(fd) -> Iterable
```

Example dumping and loading multiple series to/from a file:

```
to_serialise = []
for series in pypdu.load("foobar/baz/stats_data"):
    if some_condition(series):
        to_serialise.append(series)

with open("somefile", "wb") as f:
    pypdu.dump(f, to_serialise)
...
with open("somefile", "rb") as f:
    for series in pypdu.load_lazy(f):
        # do something with the loaded series
```

Example dumping and loading a single series to/from stdin/out:

```
data = pypdu.load("foobar/baz/stats_data")
series = data["foobar_series_name"]
pypdu.dump(sys.stdout, series)

...

series = pypdu.load(sys.stdin)
```


#### Alternative installation steps

##### setup.py
`pypdu` may be installed without `pip`. To use, clone the repository as in the [installation instructions](https://github.com/jameseh96/pdu#installing).

Then run:

```
python setup.py install
```

This should also work on Windows, but is untested.

##### manual .so

Alternatively, following the `cmake` steps in the [installation instructions](https://github.com/jameseh96/pdu#installing) to build the project produces a module with a platform-dependent name - for example on MacOS this may be `pypdu.cpython-39-darwin.so`.

This can be found either in `<build dir>/src/pypdu` or in your chosen installation prefix. This can be used without installing with `setup.py`, simply ensure the containing directory is in your `PYTHONPATH`.