<!--
SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
SPDX-License-Identifier: MIT
-->
# curlws

curlws provides a cURL based websocket implementation.

[![Build Status](https://github.com/xmidt-org/curlws/workflows/CI/badge.svg)](https://github.com/xmidt-org/curlws/actions)
[![codecov.io](http://codecov.io/github/xmidt-org/curlws/coverage.svg?branch=main)](http://codecov.io/github/xmidt-org/curlws?branch=main)
[![Coverity](https://img.shields.io/coverity/scan/23251.svg)](https://scan.coverity.com/projects/xmidt-org-curlws)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=xmidt-org_curlws&metric=alert_status)](https://sonarcloud.io/dashboard?id=xmidt-org_curlws)
[![Language Grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/xmidt-org/curlws.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/xmidt-org/curlws/context:cpp)
[![Autobahn Test Suite Compliance](https://img.shields.io/badge/autobahn%20websocket-strict%20compliance-blueviolet)](https://img.shields.io/badge/autobahn%20websocket-strict%20compliance-blueviolet)
[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/xmidt-org/curlws/blob/main/LICENSE)
[![GitHub Release](https://img.shields.io/github/release/xmidt-org/curlws.svg)](CHANGELOG.md)

## Summary

cURL is a leading networking library and tool for the c world, but it lacks
websocket support.  `curlws` provides a complementary library that uses
libcurl for the initial handshakes and underlying socket handling.  Using libcurl
as the foundation provides an extremely robust, tested and powerful framework
for `curlws`.

## Getting started

The best place to start is the [cli](https://github.com/xmidt-org/curlws/tree/main/examples/cli)
in the `examples/cli/` directory.  While a very simple application it shows the
power of this little library.

You can find the api here: https://github.com/xmidt-org/curlws/blob/main/include/curlws/curlws.h

## Design Patterns

#### Single threaded by design

`curlws` is not designed to handle a multi-threaded environment.  It certainly
can be used in one, but you will need to wrap it with the right threading magic
based on your choices and needs.

#### Sane defaults, default secure

Provide sane defaults.  All you need to get started is a URL and a zeroed out
configuration structure.  If there is a choice of secure or insecure, we choose
security and let you degrade as you see fit.

#### Leverage cURL & it's design patterns

While not everything was brought over from cURL, the callbacks and how you drive
the event thread with the `curl_multi_*` code should be familiar to most folks.
That also means there is a way to configure the cURL easy object with whatever
parameters and commands you want that we don't have today.

#### Quality and security first

There is nothing worse than a library that almost works right, but just doesn't.
The authors of `curlws` strive to be spec compliant and do so in flying colors.


## Building and Testing

### Start the Autobahn test server (Only needed to test)

This requires docker.

```
./tools/autobahn.sh
```

### Build the code

```
meson setup --warnlevel 3 --werror build
cd build
ninja all test coverage
firefox meson-logs/coveragereport/index.html
```

#### Meson Configuration Options

See: https://github.com/xmidt-org/curlws/blob/main/meson_options.txt
  
### Testing

To `validate` you will need `jq` installed.
To produce coverage results you need `lcov` installed.

```
ninja all test autobahn_collect autoban_validate coverage
```

### Look at the results

The local autobahn server reports results here: http://localhost:8080

The lcov local code/branch coverage results can be found at `build/meson-logs/coveragereport/index.html`

## Code of Conduct

This project and everyone participating in it are governed by the [XMiDT Code Of Conduct](https://xmidt.io/code_of_conduct/). 
By participating, you agree to this Code.


## Contributing

Refer to [CONTRIBUTING.md](CONTRIBUTING.md).
