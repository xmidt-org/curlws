<!--
SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
SPDX-License-Identifier: MIT
-->
# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [v1.0.1]
- Fix the .so version to be 1.0.1
- Add a CI time check to validate that the .so version matches the release
- Refine codecov.io's coverage statistics.

## [v1.0.0]
- Autobahn compliant websocket implementation
- Example cli: `examples/cli/cli.c`
- Only dependencies: `libcurl` and `libc`
- Single threaded, but re-entrant safe

## [v0.0.0]
- Initial creation

[Unreleased]: https://github.com/xmidt-org/curlws/compare/v1.0.1..HEAD
[v1.0.1]: https://github.com/xmidt-org/curlws/compare/v1.0.0..v1.0.1
[v1.0.0]: https://github.com/xmidt-org/curlws/compare/v0.0.0..v1.0.0
