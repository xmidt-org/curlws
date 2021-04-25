# curlws

curlws provides a curl based websocket implementation.

[![Build Status](https://github.com/xmidt-org/curlws/workflows/CI/badge.svg)](https://github.com/xmidt-org/curlws/actions)
[![codecov.io](http://codecov.io/github/xmidt-org/curlws/coverage.svg?branch=main)](http://codecov.io/github/xmidt-org/curlws?branch=main)
[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/xmidt-org/curlws/blob/main/LICENSE)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=xmidt-org_curlws&metric=alert_status)](https://sonarcloud.io/dashboard?id=xmidt-org_curlws)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/xmidt-org/curlws.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/xmidt-org/curlws/context:cpp)
[![GitHub release](https://img.shields.io/github/release/xmidt-org/curlws.svg)](CHANGELOG.md)

## Summary

cURL is a leading networking library and tool for the c world, but it lacks
websocket support.  This project provides a complementary library that uses
cURL for the initial handshakes and then provides a simple websocket experience.

Outside of libcurl and it's dependencies and a functional libc, this library
does not depend on any external libraries.  SHA1 is included unless you choose
to configure the `USE_OPENSSL_SHA` option (see below).

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Details](#details)
- [Install](#install)
- [Contributing](#contributing)

## Code of Conduct

This project and everyone participating in it are governed by the [XMiDT Code Of Conduct](https://xmidt.io/code_of_conduct/). 
By participating, you agree to this Code.

## Details

Add details here.

## Building and Testing

### Start the Autobahn test server (Only needed to test)

This requires docker.

```
./tests/autobahn.sh
```

### Build the code

```
mkdir build
cd build
cmake -DUSE_OPENSSL_SHA=TRUE -DUSE_AUTOBAHN_SERIAL=TRUE
make
```

#### Configuration Options

  * `USE_OPENSSL_SHA` - `TRUE` or `FALSE` (default = `FALSE`) Set this if you want
    to use the SHA1 implementation provided by OpenSsl instead of the built in
    version.  The built in version came from Yubico and is of excellent quality
    so the choice is simply a code configuration/space choice.

  * `USE_AUTOBAHN_SERIAL` - `TRUE` or `FALSE` (default = `FALSE`) Set this if you
    want to run the autobahn test suite sequencially vs. a large number in parallel.
    This is useful for debugging a specific problem, and is more realiable since
    some of the tests include relatively short timeout values.  But running the
    tests sequencially takes a fair bit more time.

### Testing

To `validate` you need `jq` installed.
To produce coverage results you need `lcov` installed.

```
make test validate coverage
```

### Look at the results

The local autobahn server reports results here: http://localhost:8080

The lcov local code/branch coverage results can be found at `build/index.html`


## Contributing

Refer to [CONTRIBUTING.md](CONTRIBUTING.md).
