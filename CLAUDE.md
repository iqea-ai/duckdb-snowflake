# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A DuckDB extension (C++17) that queries Snowflake from within DuckDB. It does **not** talk to Snowflake directly — all wire communication goes through the Arrow **ADBC Snowflake driver**, a shared library loaded dynamically at runtime. Data crosses the boundary as Apache Arrow and is fed into DuckDB's native Arrow scan machinery. Access is **read-only**.

Targets **DuckDB v1.5.4** (submodule branch `v1.5-variegata`). Both `duckdb/` and `extension-ci-tools/` are git submodules pinned to that branch.

## Build & test

Uses the standard DuckDB extension build (extension-ci-tools Makefile). The Makefile auto-downloads the ADBC driver into `adbc_drivers/libadbc_driver_snowflake.so` on first build via `scripts/download_adbc_driver.sh`.

```bash
make release          # release build → build/release/
make debug            # debug build → build/debug/ (defines DEBUG_SNOWFLAKE, enables DPRINT logging)
make test             # run SQL tests, release
make test_debug       # run SQL tests, debug
make clean
make download-adbc    # (re)download the ADBC driver only
```

Running a single test — patterns must **not** end in `.test` (DuckDB's runner treats those as file paths and silently matches nothing). Use name wildcards or group tags:

```bash
./build/release/test/unittest "*snowflake_basic_connectivity*"
./build/release/test/unittest "[pushdown]"
```

Most tests require a **live Snowflake account** with TPC-H sample data and skip otherwise. They read credentials from env vars (`require-env` in the test files):

```bash
export SNOWFLAKE_ACCOUNT=...      # short form "ORG-ACCOUNT", NOT the .snowflakecomputing.com URL
export SNOWFLAKE_USERNAME=...
export SNOWFLAKE_PASSWORD=...
export SNOWFLAKE_DATABASE=SNOWFLAKE_SAMPLE_DATA
export SNOWFLAKE_ADBC_DRIVER_PATH=/path/to/libadbc_driver_snowflake.so
```

C++ style is enforced by `.clang-format` (see also `.clang-tidy`, `.clangd`).

## Two query entry points

Everything funnels through the same Arrow → ADBC pipeline, but there are two distinct front doors with different pushdown behavior:

1. **`snowflake_query(sql, secret_name)`** table function (`src/snowflake_scan.cpp`) — passthrough. The user's SQL is sent **as-is**; filters/projections are never rewritten (`filter_pushdown_enabled = false`). Use for full control and Snowflake-native features (`SAMPLE`, `LIMIT` pushdown).

2. **`ATTACH '' AS db (TYPE snowflake, SECRET s, READ_ONLY)`** storage extension (`src/storage/`) — exposes Snowflake schemas/tables as a DuckDB catalog so you can write `db.schema.table`. Filter/projection pushdown is **off by default**; enable with `enable_pushdown true` in the ATTACH options. When on, DuckDB's optimizer decides which filters to actually push.

## Architecture / where things live

- **`src/snowflake_extension.cpp`** — extension entry point (`LoadInternal`). Registers `snowflake_version()`, the `snowflake_scan` table function, and the storage extension. Registration of the ADBC-dependent pieces is guarded by `#ifdef ADBC_AVAILABLE`; without it, `snowflake_scan` is a stub that throws. Also registers `snowflake_render_pushdown_query` — a **test-only** scalar exposing the query builder.

- **`src/snowflake_client.{cpp,hpp}`** — wraps one ADBC database+connection. `SnowflakeClient::Connect` resolves the driver path by searching, in order: extension dir, `SNOWFLAKE_ADBC_DRIVER_PATH`, system lib dirs, bare filename. Note the driver is named `libadbc_driver_snowflake.so` **on every platform** (yes, `.so` on Windows and macOS too) — rename `.dll`/`.dylib` accordingly.

- **`src/snowflake_client_manager.{cpp,hpp}`** — process-wide connection **pool**, a singleton keyed by `SnowflakeConfig`. `Acquire()` returns a `ConnectionLease` (RAII) that has exclusive use of one ADBC connection and returns it to a per-config idle list on destruction. ADBC connections are **not thread-safe**, so the "one live lease per connection" invariant is load-bearing. There is deliberately **no cap on concurrently active leases** (a single query can hold several open scan streams — e.g. `UNION ALL` with `preserve_insertion_order=false`); only idle connections are bounded (`MAX_IDLE_PER_CONFIG`). Invalidate a lease after an auth/session error so the stale connection is dropped, not pooled.

- **`src/snowflake_arrow_utils.{cpp,hpp}`** — the bridge to DuckDB's Arrow scanner. `SnowflakeArrowStreamFactory` owns the `ConnectionLease` for the life of a scan and produces `ArrowArrayStream`s. Destruction order matters: the ADBC statement/stream are released **before** the lease returns the connection. Schema is fetched via `SnowflakeGetArrowSchemaViaQuery` (runs the query with a 1-row limit) rather than `AdbcStatementExecuteSchema`, because ExecuteSchema mis-reports geoarrow WKB tags and TIMESTAMP units (issues #44). For the passthrough SELECT path, projection is applied by **wrapping the user query as a subquery** and the produced stream must expose exactly the projected columns in order — DuckDB reads Arrow children **positionally** (issue #32: wrong-column reads / SIGSEGV otherwise).

- **`src/snowflake_query_builder.{cpp,hpp}`** — builds pushdown SQL for the ATTACH path by constructing a DuckDB AST from the pre-parsed `TableFilterSet` + column names, then serializing. `QuoteSnowflakeIdentifier` handles Snowflake's uppercase-folding identifier rules (quote anything non-`A-Z0-9_$` or lowercase).

- **`src/snowflake_config.{cpp,hpp}`** — `SnowflakeConfig` struct + connection-string parsing + `SnowflakeConfigHash` (used as the pool map key). `SnowflakeAuthType`: PASSWORD, OAUTH, KEY_PAIR, EXT_BROWSER, OKTA, MFA.

- **`src/snowflake_secrets.cpp` + `src/snowflake_secret_provider.cpp`** — the `CREATE SECRET (TYPE snowflake, ...)` integration; `GetCredentials(context, name)` turns a secret into a `SnowflakeConfig`. Both entry points and ATTACH resolve credentials this way.

- **`src/snowflake_types.cpp` / `snowflake_arrow_utils`** — Snowflake/Arrow → DuckDB type mapping (including GEOMETRY/GEOGRAPHY, TIMESTAMP_NTZ, high-precision DECIMAL; `use_high_precision=false` collapses `DECIMAL(p,0)` to INT64).

- **`src/storage/`** — the ATTACH catalog hierarchy mirroring DuckDB's storage API: `snowflake_storage` (registers the extension + `SnowflakeAttach`), `snowflake_catalog`, `snowflake_catalog_set`, `snowflake_schema_{entry,set}`, `snowflake_table_{entry,set}`. `snowflake_transaction.cpp` provides the (read-only) transaction manager.

- **`third_party/adbc/driver_manager.cpp`** — vendored ADBC driver manager. It was removed from DuckDB core in v1.5, so this extension bundles its own.

## Adding sources

New `.cpp` files must be added to `EXTENSION_SOURCES` in `CMakeLists.txt` — there is no glob. Headers live in `src/include/` (and `src/include/storage/`), already on the include path.

## Docs

- `BUILD.md` — developer build/setup detail.
- `docs/AUTHENTICATION.md`, `docs/AUTHENTICATION_SETUP.md` — per-auth-method setup (password, key-pair, OAuth, external browser, Okta, MFA).
- `docs/UPDATING.md` — bumping the DuckDB/driver versions.
- `test/README.md` — full test harness reference.
- `scripts/` — `install-adbc-driver.{sh,bat}` (end-user driver install), `package_extension_with_driver.py` (bundles the driver into the extension zip).
