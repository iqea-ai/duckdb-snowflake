# Security Policy

## Supported Versions

Only the most recent extension release line is supported with security fixes.
The DuckDB column shows the DuckDB version each extension line is built and
tested against.

| Extension version | DuckDB version | Status |
| --- | --- | --- |
| 0.4.x | v1.5.3 | Supported |
| 0.3.x and earlier | v1.5.0 — v1.5.2 | End of life |

When a new minor release ships, the previous line is dropped from support.
The community-extensions registry only serves the latest published version,
so upgrading means re-running `INSTALL snowflake FROM community;` from a
matching DuckDB version.

## Reporting a Vulnerability

Email **security@iqea.ai** with:

- A description of the issue and the impact you observed.
- The extension version (`SELECT snowflake_version();`), the DuckDB version,
  the platform, and the authentication method in use.
- A minimal reproduction — SQL statements, commands, or a small repo.
- Whether the issue is already public and whether you've coordinated
  disclosure with anyone else.

Please **don't** file the report in the public GitHub issue tracker, and
don't open a draft PR with the fix until we've agreed on a disclosure
timeline.

### What to expect

- **Acknowledgement** within 3 business days.
- **Initial assessment** (whether we can reproduce, severity rating, rough
  remediation plan) within 10 business days.
- A coordinated disclosure window proportional to severity — typically
  30–90 days from acknowledgement, longer if a downstream dependency
  (DuckDB core, the ADBC Snowflake driver, Snowflake itself) needs to ship
  first.
- Credit in the release notes and the advisory, unless you'd prefer to
  remain anonymous.

We treat reports about the upstream
[`adbc-drivers/snowflake`](https://github.com/adbc-drivers/snowflake) driver
or DuckDB core itself as out of scope for this repo, but we will help
forward them to the right maintainer if you want.

## Scope

In scope:

- Code in this repository (the extension, the build scripts, the GitHub
  Actions workflows).
- The packaging pipeline that produces the community-extensions binary.

Out of scope:

- Vulnerabilities in DuckDB itself — please report to
  https://github.com/duckdb/duckdb/security.
- Vulnerabilities in the ADBC Snowflake driver — please report to
  https://github.com/adbc-drivers/snowflake.
- Snowflake account misconfigurations (over-permissive roles, leaked keys,
  etc.) — those are a Snowflake operational concern.
