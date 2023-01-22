# pypdu

This module provides basic read-only access to the data contained in Prometheus on-disk files from Python.

`pypdu` may be installed from pip (on linux and macOS):

```
pip install pypdu
```

`pypdu` can optionally expose samples in a numpy array if `numpy` is installed.
If you need this, you can either ensure `numpy` is installed, or have it pulled in by `pypdu` as a dependency with:

```
pip install pypdu[numpy]
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


#### Conversion methods

Manipulating large time series as lists-of-lists is likely to perform poorly in Python.
pypdu can expose samples as a thin python wrapper around an underlying C++ type.

This wrapper exposes "list like" operations:
```
>>> series = data["foobar"]
>>> vector = series.samples.as_vector()
>>> vector[0]
{timestamp=1664592572000, value=0.000000}
>>> vector[0].timestamp
1664592572000
```

pypdu also provides a convenience `to_list()`, with the same interface returning pure python types.

These conversions can also apply some common manipulations to the time series:

* Scaling the timestamps to seconds
```
series.samples.as_vector(timestamp_units=pypdu.Seconds)
```
* Filtering NaN values out of the time series
```
series.samples.as_vector(filter_nan_values=True)
```

#### numpy

If numpy is installed, samples can additionally be accessed as a numpy array. This may avoid copying the samples around if your code expects numpy arrays. E.g.,

```
for name, labels, samples in data:
    arr = samples.as_array()
    print(arr.dtype)
    print(arr[0])
```
prints:
```
dtype([('timestamp', '<i8'), ('value', '<f8')])
(1653556688725, 0.)
```

`as_array()` also accepts `timestamp_units` and `filter_nan_values` as above.

If numpy is _not_ available at runtime, this will raise an exception:

```
RuntimeError: Accessing samples as a numpy array requires numpy to be installed
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


#### Calculations

Simple operations `(+ - / *)` may be applied to `Series` objects, computing the result lazily.

```
a = data["foobar"]
b = data["bazqux"]
c = data["spam"]
expression = (a + b) * (c / 100)
for timestamp, value in expression:
    ...
```

Note: the resulting iterable will contain a sample at each timestamp seen in _any_ of the constituent series. Even if all series are scraped with the same interval, if they are offset from each other this can lead to a lot of values. To avoid this, the expression can be resampled at a given interval:

```
for timestamp, value in expression.resample(10000): # 10s in ms
    ...
```

This will lead to one sample _exactly_ every 10000 milliseconds. No interpolation is performed - if a given series did not have a sample at a chosen instant, the most recent value will be used.


###### IRate


```
pypdu.irate(expr)
```

Results in a `Expression` which computes the instantaneous rate of change based on the current and previous sample - roughly equivalent to Prometheus `irate`.

e.g.,

```
a = data["foobar"]
b = data["bazqux"]
rate = pypdu.irate(a+b/100)
for timestamp, rate_value in rate:
    ....
```

###### Sum

As `Expression` supports addition, the standard Python method `sum` can be used to add multiple series together.

However, if working with a very large number of series, `pypdu.sum` may more efficiently construct the `Expression` result (computation of the summed `Samples` is identical, however).

e.g.,

```
series_list = list(data)
py_sum_expr = sum(series_list)
pdu_sum_expr = pypdu.sum(series_list) # may be faster if len(series_list) is large

# but the resulting samples are identical
assert(list(pdu_sum_expr) == list(py_sum_expr))
```

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

The difference between histograms at two points in time can also be calculated:

```
delta = last-first
>>> delta.time_delta
60000
>>> delta.buckets()
[(0.001, 653.0), (0.01, 653.0), (0.1, 653.0), (1.0, 653.0), (10.0, 653.0), (inf, 653.0)]
```

Or the summation of two histograms:

```
total = histA+histB
>>> total.buckets()
[(0.001, 1985.0), (0.01, 1985.0), (0.1, 1985.0), (1.0, 1985.0), (10.0, 1985.0), (inf, 1985.0)]
```

For either of addition or subtraction, the bucket boundaries must exactly match.

#### Serialisation

Time series may be dumped individually to a file or bytes. This may be useful if you need to store some number of series (e.g., in a key-value store), but don't wish to retain the entire Prometheus data directory.

`pypdu.dump`/`pypdu.load` take an `int` file descriptor or, for convenience, a file-like object supporting `fileLike.fileno() -> int`.

These methods be used to read/write data from/to a pipe or socket, not just a file on disk. Note, arbitrary file-like objects which are not backed by a file descriptor are not supported.


If provided a file handle which actually refers to a file on disk, `load` will try to mmap the file. If this fails, it will fall back to reading it like a stream. If mmapping is not desired, it can be disabled with:

```
pypdu.load(fileDescriptor, allow_mmap=False)
```

When `load`ing many series from a _stream_ (socket, pipe, etc), the underlying data for all Series will be read into memory - this may be costly if there are many Series. `pypdu.load_lazy` can instead be used to consume Series from a stream, one at a time.

```
for series in pypdu.load_lazy(someSocket):
    # series are read and deserialised on demand while iterating
```

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

#### pypdu.json

For performance, pypdu provides a json encoder capable of efficiently dumping pypdu types.
It can also dump typical python types (everything supported by the builtin `json`), but is not a drop in replacement in terms of arguments.

```
data = pypdu.load(...)
series = data["foobar"]
pypdu.json.dumps(series)
```

will produce:

```
{
    "metric": {
        "__name__": "some_metric_name",
        "label_foo": "label_foo_value",
    },
    "values": [
        [
            1664592572000,
            0.0
        ],
        [
            1664592582000,
            0.0
        ],
        [
            1664592592000,
            0.0
        ],
```

`dumps` also supports samples, sample vectors, and expressions:

```
>>> pypdu.json.dumps(series.samples)
"[[1664592572000, 0.0], [1664592582000, 0.0],...]"
>>> pypdu.json.dumps(series.samples.as_vector(timestamp_units=pypdu.Seconds))
"[[1664592572, 0.0], [1664592582, 0.0],...]"
>>> pypdu.json.dumps((series + 1) * 2)
"[[1664592572000, 2.0], [1664592582000, 2.0],...]"
>>> pypdu.json.dumps(((series + 1) * 2).as_vector(timestamp_units=pypdu.Seconds))
"[[1664592572, 2.0], [1664592582, 2.0],...]"
```


#### XOR Chunks

For specific use cases, access to the raw [XOR encoded](https://github.com/prometheus/prometheus/blob/release-2.26/tsdb/chunkenc/xor.go) ([chunk documentation](https://github.com/prometheus/prometheus/blob/release-2.26/tsdb/docs/format/chunks.md)) chunk data may be required.


To find the chunk objects for a given series:
```
>>> data = pypdu.load("some_stats_dir")
>>> series = data["foobar_series_name"]
>>> series.chunks
[<pypdu.Chunk object at 0x11c29c270>, <pypdu.Chunk object at 0x11c29dbb0>, ...]
```

To access the XOR encoded sample data:

```
>>> chunk = series.chunks[0]
# without copying
>>> memoryview(chunk)
<memory at 0x11c227880>
# with a copy into a python bytes object
>>> chunk.as_bytes()
b'\x00y\xc8\xe0\x8e\...'
```

Most users will not need to do this as `samples` can be read from a `pypdu.Series()`, with the chunks handled transparently.


#### Runtime version checking

The `pypdu` version can be specified at install time (e.g., in `requirements.txt`), but you can also verify the correct version is available at runtime (maybe someone is building locally and forgot to update some dependencies!).

```
>>> import pypdu
>>> pypdu.__version__
'0.0.12a3'
>>> pypdu.__git_rev__
'a096f0d'
>>> pypdu.__git_tag__
''
>>> pypdu.require(0, 0, 0)
>>> pypdu.require(0, 0, 12)
>>> pypdu.require(0, 1, 0)
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
RuntimeError: Current pypdu version 0.0.12a3 does not meet required 0.1.0
>>> pypdu.require(0, 0, 12, "a3")
>>> pypdu.require(0, 0, 12, "a4")
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
RuntimeError: Current pypdu version 0.0.12a3 does not meet required 0.0.12a4
```

If using a feature introduced in version `X.Y.Z`, `pypdu.require(X, Y, Z)` will raise an exception if an older version is in use.
This exception can be caught, if you want to provide a more specific error message (e.g., "Remember to update dependencies by running ...").


#### Alternative installation steps

##### pip install from source

If a wheel is not available for your platform or architecture, `pypdu` can be built and installed with:

```
pip install git+https://github.com/jameseh96/pdu.git
```

or for a specific version:

```
pip install git+https://github.com/jameseh96/pdu.git@vX.Y.Z
e.g.,
pip install git+https://github.com/jameseh96/pdu.git@v0.0.19
```

Building `pypdu` will require the dependencies listed in the [installation instructions](https://github.com/jameseh96/pdu#installing).

`pypdu` is relatively platform independent, but has not been tested on platforms/architectures that don't have a wheel built (e.g., Windows, MacOS+Apple Silicon) - be prepared for potential issues at build and runtime.

##### setup.py
`pypdu` may be installed without `pip`. To use, clone the repository as in the [installation instructions](https://github.com/jameseh96/pdu#installing).

Then run:

```
python setup.py install
```

##### manual .so

Alternatively, following the `cmake` steps in the [installation instructions](https://github.com/jameseh96/pdu#installing) to build the project produces a module with a platform-dependent name - for example on MacOS this may be `pypdu.cpython-39-darwin.so`.

This can be found either in `<build dir>/src/pypdu` or in your chosen installation prefix. This can be used without installing with `setup.py`, simply ensure the containing directory is in your `PYTHONPATH`.