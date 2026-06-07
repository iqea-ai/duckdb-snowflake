---
name: Bug report
about: Something is broken or behaves unexpectedly
title: ''
labels: bug, needs-repro
assignees: ''
---

## What happened

<!-- A clear, specific description. Paste the error message verbatim if there is one. -->

## What you expected

<!-- What should have happened instead. -->

## Reproduction

<!-- Minimal SQL that reproduces the issue. If the repro requires specific
     table contents, describe the shape (column types, row counts). -->

```sql
-- Your repro here
```

## Environment

- **Extension version** (`SELECT snowflake_version();`):
- **DuckDB version** (`PRAGMA version;`):
- **OS / architecture** (e.g. macOS 14 arm64, Ubuntu 22.04 x86_64):
- **Install method** (community-extensions registry, built from source, packaged zip):
- **ADBC Snowflake driver version** (path + how you installed it):
- **Snowflake authentication method** (password, key pair, OAuth, EXT_BROWSER, Okta, MFA):
- **Snowflake region / cloud** if relevant:

## Logs / debug output

<!-- If you have it: query plan (EXPLAIN), debug output, anything from
     the extension's [DEBUG] lines, stack trace, etc. -->

```
paste output here
```

## Additional context

<!-- Anything else worth knowing — workarounds tried, when this started,
     related issues. -->
