# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The version recorded here is the **extension version** (what `snowflake_version()`
returns and what the [community-extensions descriptor](https://github.com/duckdb/community-extensions/blob/main/extensions/snowflake/description.yml)
pins). The DuckDB version each release targets is noted separately.

## [0.6.0] - 2026-10-09

Targets DuckDB **v1.5.6**.

### Added
- `SET snowflake_result_queue_size` and `SET snowflake_prefetch_concurrency` bound the ADBC driver's read-ahead. A slow consumer (`CREATE TABLE AS`, `COPY TO`, inserts) no longer lets the driver buffer most of the result outside `memory_limit`. Defaults are unchanged: with neither set, no option reaches the driver ([#66](https://github.com/iqea-ai/duckdb-snowflake/issues/66), reported by housejester, in [#68](https://github.com/iqea-ai/duckdb-snowflake/pull/68))
- `duckdb_functions()` now reports real parameter names, a description, an example and a category for `snowflake_query`, `snowflake_version` and `snowflake_render_pushdown_query`, instead of `col0`/`col1` and NULL ([#67](https://github.com/iqea-ai/duckdb-snowflake/issues/67), reported by rustyconover, in [#72](https://github.com/iqea-ai/duckdb-snowflake/pull/72))
- CI runs live queries on Windows: the shipped `windows_amd64` package goes through the stock CLI with a hard timeout, and a hang uploads a thread dump ([#70](https://github.com/iqea-ai/duckdb-snowflake/issues/70), in [#71](https://github.com/iqea-ai/duckdb-snowflake/pull/71))

### Fixed
- On Windows the CLI no longer hangs on exit after `snowflake_query()`. Pooled connections were released by static destructors at process exit, where the Go driver blocks forever because Windows has already terminated its threads. Connections are now released when the database closes, and nothing calls into the driver during process teardown ([#69](https://github.com/iqea-ai/duckdb-snowflake/issues/69), in [#71](https://github.com/iqea-ai/duckdb-snowflake/pull/71))
- `count(*)`, `SELECT 1 FROM t` and other queries that reference no columns of an attached table no longer fail. With pushdown off they raised `Virtual columns require projection pushdown`; with pushdown on they sent an empty `SELECT` list to Snowflake ([#73](https://github.com/iqea-ai/duckdb-snowflake/issues/73), in [#74](https://github.com/iqea-ai/duckdb-snowflake/pull/74))

### Changed
- Update DuckDB submodule and CI pins to v1.5.6, the community-extensions registry's current build target
- CI no longer triggers on the retired `stable` branch

### Notes
- Users on DuckDB v1.5.5 stay on 0.5.2: the registry freezes each DuckDB version's extension build, so these fixes reach DuckDB v1.5.6 and later.

## [0.5.2] - 2026-08-07

Targets DuckDB **v1.5.5**.

### Fixed
- `snowflake_query()` no longer crashes on statements that begin with a SQL comment. The execution path was chosen from the first token of the user's SQL, so a leading comment (as prepended by dbt, BI tools, and query routers) made that token `/*` or `--`; even a plain `SELECT` then missed the projected-subquery path and DuckDB read projected columns positionally off a full-width stream (`ArrowTypeInfo` type mismatch). Comments are now skipped for classification only — the statement sent to Snowflake keeps them, so query tags still reach query history ([#64](https://github.com/iqea-ai/duckdb-snowflake/pull/64))
- A trailing line comment no longer breaks the passthrough subquery wrap and schema probe: the user query is placed on its own line so the comment cannot swallow the closing parenthesis ([#64](https://github.com/iqea-ai/duckdb-snowflake/pull/64))
- Projecting a single column out of a multi-column DDL/DML result no longer returns the wrong column. `SELECT output_bytes FROM snowflake_query('COPY INTO ...')` returned `rows_unloaded` — silently, since the columns share a type. The bind now disables projection pushdown on its own copy of the table function, so DuckDB scans the full result and applies the projection above the scan ([#64](https://github.com/iqea-ai/duckdb-snowflake/pull/64))

## [0.5.1] - 2026-07-31

Targets DuckDB **v1.5.5**.

### Fixed
- Projected `LIST`/`EXPLAIN`/`CALL` through `snowflake_query()` no longer crash (ArrowTypeInfo type mismatch) or silently return wrong columns: row-returning metadata statements are classified by first keyword and re-targeted at `TABLE(RESULT_SCAN('<query id>'))`, the same machinery that fixed `SHOW`/`DESC` in 0.5.0; the `LAST_QUERY_ID()` session-affinity invariant is now enforced with a connection-identity check ([#59](https://github.com/iqea-ai/duckdb-snowflake/issues/59), in [#61](https://github.com/iqea-ai/duckdb-snowflake/pull/61))
- CI's clang-tidy check now lints the whole `src/` tree instead of only `src/storage/` (the upstream file regex requires a second path separator), and the lint backlog in the previously unchecked files is cleaned up ([#60](https://github.com/iqea-ai/duckdb-snowflake/issues/60), in [#62](https://github.com/iqea-ai/duckdb-snowflake/pull/62))

## [0.5.0] - 2026-07-23

Targets DuckDB **v1.5.5**.

### Added
- Native `GEOMETRY`/`GEOGRAPHY` support: Snowflake geo columns are returned as GeoArrow (EWKB) by the ADBC driver and imported as DuckDB `GEOMETRY`, replacing the prior text passthrough ([#24](https://github.com/iqea-ai/duckdb-snowflake/pull/24), jatorre)
- Connection pool: each scan leases a dedicated ADBC connection, so concurrent scans (e.g. dbt-duckdb models, self-joins) no longer share one non-thread-safe connection and crash ([#49](https://github.com/iqea-ai/duckdb-snowflake/pull/49), guillesd)
- CI smoke test that executes the driver installers on Linux, macOS, and Windows ([#47](https://github.com/iqea-ai/duckdb-snowflake/pull/47))
- CI runs the key-pair test suite: the Snowflake test job now provisions `SNOWFLAKE_PRIVATE_KEY_FILE`, so the ~15 `key_pair` test files execute instead of silently skipping ([#52](https://github.com/iqea-ai/duckdb-snowflake/pull/52))

### Changed
- Update DuckDB submodule and CI pins to v1.5.5, the community-extensions registry's current build target ([#57](https://github.com/iqea-ai/duckdb-snowflake/pull/57); supersedes the v1.5.4 bump in [#46](https://github.com/iqea-ai/duckdb-snowflake/pull/46))
- Install the ADBC Snowflake driver from the ADBC Driver Foundry (`adbc-drivers/snowflake`, `go/v1.11.0`) instead of the deprecated `apache/arrow-adbc` wheels; the Foundry build ships GeoArrow support ([#47](https://github.com/iqea-ai/duckdb-snowflake/pull/47), reported by amoeba [#45](https://github.com/iqea-ai/duckdb-snowflake/issues/45))

### Fixed
- Projected `SHOW`/`DESC` through `snowflake_query()` no longer crashes (ArrowTypeInfo type mismatch) or silently returns swapped columns: the scan re-targets at `TABLE(RESULT_SCAN('<query id>'))` so the projected-subquery machinery applies ([#48](https://github.com/iqea-ai/duckdb-snowflake/issues/48), in [#55](https://github.com/iqea-ai/duckdb-snowflake/pull/55))
- The ADBC driver is discovered via standardized driver manifests (`snowflake.toml` in `ADBC_DRIVER_PATH`, the active conda prefix, per-user and system dirs, and on Windows the `SOFTWARE\ADBC\Drivers\snowflake` registry keys — the only mechanism dbc uses there), so `dbc install snowflake` works with no env var on every platform; the not-found error lists every location searched, and a CI smoke test guards the manifest path ([#50](https://github.com/iqea-ai/duckdb-snowflake/issues/50), in [#56](https://github.com/iqea-ai/duckdb-snowflake/pull/56))
- `snowflake_query()` no longer reads the wrong column (SIGSEGV / silent wrong data) when DuckDB requests a non-prefix projection ([#32](https://github.com/iqea-ai/duckdb-snowflake/issues/32), in [#42](https://github.com/iqea-ai/duckdb-snowflake/pull/42))
- `TIMESTAMP_NTZ` columns read through an attached catalog now use the correct unit instead of being misread as nanoseconds; the bind schema comes from the data path rather than `AdbcStatementExecuteSchema` ([#44](https://github.com/iqea-ai/duckdb-snowflake/issues/44), in [#42](https://github.com/iqea-ai/duckdb-snowflake/pull/42))
- Quote projection-list and `WHERE` column references in pushdown SQL so lowercase identifiers don't fold to uppercase (follow-up to [#38](https://github.com/iqea-ai/duckdb-snowflake/issues/38), in [#42](https://github.com/iqea-ai/duckdb-snowflake/pull/42))

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

Earlier releases predate this CHANGELOG. The version-to-release mapping below was recovered from `duckdb/community-extensions` descriptor history.

- **0.3.0** (registry 2026-03-14, DuckDB v1.5.0) — DuckDB v1.5 compatibility update ([#23](https://github.com/iqea-ai/duckdb-snowflake/pull/23), ref `a2a3aed`)
- The `snowflake_query` column-pruning crash fix ([#25](https://github.com/iqea-ai/duckdb-snowflake/pull/25), merged 2026-03-20) first shipped with 0.4.0 — the registry descriptor was not bumped for it separately.

[0.6.0]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.5.2...v0.6.0
[0.5.2]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.4.1...v0.5.0
[0.4.1]: https://github.com/iqea-ai/duckdb-snowflake/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/iqea-ai/duckdb-snowflake/releases/tag/v0.4.0
