# Contributing to duckdb-snowflake

Thanks for your interest in contributing. This document covers the working
agreement: how to build, how to run the tests (including the live-Snowflake
suite), and what we expect on PRs.

For the longer-form developer guide (toolchain prerequisites, platform notes,
internal architecture), see [BUILD.md](BUILD.md).

## Build

The repo uses the DuckDB extension build harness via `extension-ci-tools`.

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/iqea-ai/duckdb-snowflake.git
cd duckdb-snowflake

# Release build (downloads the Snowflake ADBC driver on first run if missing)
make release

# Debug build
make debug
```

The Makefile fetches `adbc_drivers/libadbc_driver_snowflake.so` via
`scripts/download_adbc_driver.sh` if it isn't already present. You don't
normally need to manage that file yourself.

## Test suites

There are two layers of tests:

1. **Build-only smoke tests** that ship with the DuckDB extension harness and
   run in CI as `Main Extension Distribution Pipeline`.
2. **Live-Snowflake integration tests** under `test/sql/` that connect to a
   real Snowflake account. These require credentials and run in a separate
   workflow (`Test with Snowflake`).

### Running the live-Snowflake suite locally

You need a Snowflake account with access to the TPC-H sample data
(`SNOWFLAKE_SAMPLE_DATA.TPCH_SF1`), the ADBC Snowflake driver installed
locally, and an RSA key pair registered against your user.

Set the following environment variables before running the tests:

| Variable | Purpose |
| --- | --- |
| `SNOWFLAKE_ACCOUNT` | Snowflake account identifier (short form, e.g. `xy12345.us-east-1`). |
| `SNOWFLAKE_DATABASE` | Database to run against (e.g. `SNOWFLAKE_SAMPLE_DATA`). |
| `SNOWFLAKE_USERNAME` | User for the password/key-pair tests under `test/sql/snowflake_*.test` that use the `USERNAME`-named secret form. |
| `SNOWFLAKE_PRIVATE_KEY_FILE` | Path to the RSA private key PEM file, used by `snowflake_basic_connectivity`, `snowflake_data_types`, `snowflake_read_operations`, `snowflake_performance`, `snowflake_error_handling`. |
| `SNOWFLAKE_PRIVATE_KEY_PASSWORD_FILE` | Path to a file containing the passphrase for the private key above. |
| `SNOWFLAKE_KEYPAIR_USER` | User for the pushdown and projection tests, which create their own keypair secret. |
| `SNOWFLAKE_PRIVATE_KEY_PATH` | Path to the RSA private key PEM file, used by `test/sql/pushdown/*.test` and `snowflake_query_projection.test`. |
| `SNOWFLAKE_PRIVATE_KEY_PASSPHRASE` | Passphrase value (not file) for the key above. |
| `SNOWFLAKE_ADBC_DRIVER_PATH` | Absolute path to `libadbc_driver_snowflake.so` (used by tests that load it explicitly). |

Tests that need a variable declare it with `require-env`; if a variable is
unset the test is skipped rather than failed.

Then run a subset (DuckDB's unittest binary intercepts patterns ending in
`.test` as file paths, so use a wildcard pattern instead):

```bash
# Whole Snowflake suite
./build/release/test/unittest "*snowflake_*"

# Just the pushdown tests (uses the [pushdown] group tag)
./build/release/test/unittest "[pushdown]"

# One specific test
./build/release/test/unittest "*snowflake_basic_connectivity*"
```

`make test-snowflake` runs the same set as a debug build and enforces that
`SNOWFLAKE_ACCOUNT`, `SNOWFLAKE_USERNAME`, `SNOWFLAKE_PASSWORD`, and
`SNOWFLAKE_DATABASE` are set — be aware that the Makefile target's variable
list predates the move to key-pair auth and is not yet aligned with the
`require-env` declarations inside the individual tests.

### CI

GitHub Actions runs:

- `.github/workflows/MainDistributionPipeline.yml` — DuckDB extension build,
  code-quality (format + clang-tidy), packaging.
- `.github/workflows/test-with-snowflake.yml` — live-Snowflake suite, gated
  on the `SNOWFLAKE_ACCOUNT` secret being available. Skips with a warning if
  the repository variable `SKIP_SNOWFLAKE_TESTS` is set to `true`.

## Code style

Run the DuckDB formatter on any source files you change before opening a
pull request:

```bash
python3 duckdb/scripts/format.py <changed-files>
```

To format everything: `make format-fix`.

`clang-tidy` runs in CI under the `code-quality-check` job.

### Test-file format note

DuckDB's SQLLogicTest parser is strict: `# group:` must come immediately
after `# description:` in the header block, before any continuation comment
lines. CI will fail otherwise.

## Branch and PR conventions

- Branch off `main`.
- Branch names use a short `type/topic` form: `fix/issue-26-pem-parsing`,
  `update/duckdb-v1.5.3`, `chore/repo-polish`. The `type/` prefix maps to
  one of `fix`, `feat`, `update`, `chore`, `docs`.
- Open a draft PR early if you want feedback; mark it ready when CI is
  green.
- Reference any issue the PR closes with `Closes #N` in the description.
- Keep the PR scoped to one logical change. The `Post-1.5.3 cleanup` PR
  (#40, four issues in one commit) is the exception, not the rule — that
  bundling was deliberate because the fixes shared the same regression
  surface.
- Do not push to `main`. Do not force-push to a shared branch unless you
  own it.

### Crediting external contributors

When a maintainer lands a contribution from a forked PR (e.g. via squash-and-
merge), include a `Co-Authored-By:` trailer in the commit message giving
credit to the original author using the email GitHub shows on their commits:

```
Co-Authored-By: Author Name <user@example.com>
```

PRs from external contributors that are merged unchanged don't need a manual
trailer — GitHub's merge UI handles authorship. The trailer matters when a
maintainer cherry-picks, rebases, or rewrites the commit during merge.

Do not add `Co-Authored-By:` trailers for tools, AI assistants, or the
maintainer themself.

## Reporting bugs and requesting features

Use the issue templates under
[`.github/ISSUE_TEMPLATE/`](.github/ISSUE_TEMPLATE/). Bug reports must
include the DuckDB version, the extension version (`SELECT
snowflake_version();`), the Snowflake authentication method, and a minimal
SQL repro.

## Security

Don't file security issues in the public tracker. See
[SECURITY.md](SECURITY.md) for the disclosure process.
