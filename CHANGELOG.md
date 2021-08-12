<!--
SPDX-FileCopyrightText: 2021 Comcast Cable Communications Management, LLC
SPDX-License-Identifier: MIT
-->
# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [v1.0.4]

### Changed
- Replace cmake building system with meson & ninja.


## [v1.0.3]

### Fixed
- [Issue #66](https://github.com/xmidt-org/curlws/issues/66) where memory bounds are not respected

### Changed
- Switch over to use an improved version of trower-base64.
- Improve the SHA1 error checking.
- Build and test all SHA1 implementations.
- Update the cmake files to use latest versions from xmidt projects.
- Update github actions to use latest sonarcloud & verify signature.


## [v1.0.2]

### Changed
- Changed the versioning to be for the project, not the .so file.  [Issue #59](https://github.com/xmidt-org/curlws/issues/59)
- Updated the Github tag workflow to validate that the CHANGELOG.md and the cmake
  project version are the same.
- Mark the .whitesource file copyright owner as unsure.

### Added
- curlwsver.h with versioning


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

[Unreleased]: https://github.com/xmidt-org/curlws/compare/v1.0.4..HEAD
[v1.0.4]: https://github.com/xmidt-org/curlws/compare/v1.0.3..v1.0.4
[v1.0.3]: https://github.com/xmidt-org/curlws/compare/v1.0.2..v1.0.3
[v1.0.2]: https://github.com/xmidt-org/curlws/compare/v1.0.1..v1.0.2
[v1.0.1]: https://github.com/xmidt-org/curlws/compare/v1.0.0..v1.0.1
[v1.0.0]: https://github.com/xmidt-org/curlws/compare/v0.0.0..v1.0.0
