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
data.filter({"proc":pyp.regex("^go.*")})
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