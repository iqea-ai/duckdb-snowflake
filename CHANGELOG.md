# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The version recorded here is the **extension version** (what `snowflake_version()`
returns and what the [community-extensions descriptor](https://github.com/duckdb/community-extensions/blob/main/extensions/snowflake/description.yml)
pins). The DuckDB version each release targets is noted separately.

## [0.4.1] - 2026-06-04

Targets DuckDB **v1.5.3**.

### Changed
- Update DuckDB submodule to v1.5.3 ([#39](https://github.com/iqea-ai/duckdb-snowflake/pull/39))

### Fixed
- Pass password to ADBC driver for Okta native authentication ([#34](https://github.com/iqea-ai/duckdb-snowflake/pull/34), Setzer)
- Use the correct ADBC key for database and make `DATABASE` optional on `CREATE SECRET` ([#35](https://github.com/iqea-ai/duckdb-snowflake/pull/35), Setzer)
- `SnowflakeSecret::Validate()` no longer requires a password for `ext_browser`/`externalbrowser` auth ([#37](https://github.com/iqea-ai/duckdb-snowflake/issues/37), in [#40](https://github.com/iqea-ai/duckdb-snowflake/pull/40))
- Quote Snowflake identifiers in storage SELECT, INFORMATION_SCHEMA enumeration, and the pushdown `FROM` clause so unquoted identifiers don't fold to uppercase ([#38](https://github.com/iqea-ai/duckdb-snowflake/issues/38), in [#40](https://github.com/iqea-ai/duckdb-snowflake/pull/40))
- Persistent Snowflake secrets deserialized at session start before the extension loads now resolve correctly via `KeyValueSecret::TryGetValue` instead of failing the typed cast ([#36](https://github.com/iqea-ai/duckdb-snowflake/issues/36), in [#40](https://github.com/iqea-ai/duckdb-snowflake/pull/40))
- Cache the Arrow schema on `SnowflakeTableEntry` to avoid 300–500ms per-bind round-trips on `CREATE VIEW` and similar paths ([#33](https://github.com/iqea-ai/duckdb-snowflake/issues/33), in [#40](https://github.com/iqea-ai/duckdb-snowflake/pull/40))

### Security
- Restrict workflow `GITHUB_TOKEN` permissions to `contents: read` on the three Main Distribution Pipeline jobs ([#41](https://github.com/iqea-ai/duckdb-snowflake/pull/41))

## [0.4.0] - 2026-04-20

Targets DuckDB **v1.5.2**. First release distributed via the DuckDB community-extensions registry under this version line.

### Changed
- Update DuckDB submodule to v1.5.2 ([#31](https://github.com/iqea-ai/duckdb-snowflake/pull/31))
- Switch the live-Snowflake test suite from password to key-pair authentication

### Fixed
- ADBC option keys for OAuth token, Okta URL, and keep-alive ([#30](https://github.com/iqea-ai/duckdb-snowflake/pull/30))
- PEM parsing regression ([#26](https://github.com/iqea-ai/duckdb-snowflake/issues/26)) and `snowflake_query` segfault ([#21](https://github.com/iqea-ai/duckdb-snowflake/issues/21)), both addressed in [#27](https://github.com/iqea-ai/duckdb-snowflake/pull/27)

## [Earlier]

Earlier releases predate this CHANGELOG and the version-to-release mapping below is reconstructed from git history. The version numbers used by the community-extensions descriptor for these merges are not recorded in this repository — TODO: recover from `duckdb/community-extensions` history if needed.

- 2026-03-20 — Fix `snowflake_query` crash on column-pruning queries with DuckDB v1.5 ([#25](https://github.com/iqea-ai/duckdb-snowflake/pull/25))
- 2026-03-13 — Update extension for DuckDB v1.5 compatibility ([#23](https://github.com/iqea-ai/duckdb-snowflake/pull/23))

[0.4.1]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/iqea-ai/duckdb-snowflake/releases/tag/v0.4.0
