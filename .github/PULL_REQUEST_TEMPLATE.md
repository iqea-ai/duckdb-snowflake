<!--
Thanks for the PR. A few asks to keep the review fast:
- Scope this PR to one logical change. Bundles are the exception, not the rule.
- Make sure the live-Snowflake test workflow passed (or note why it didn't).
- Keep CHANGELOG.md up to date — add an entry under the unreleased section.
-->

## Summary

<!-- One paragraph: what does this change, and why. -->

## Related issues

<!-- "Closes #N" for any issue this fully resolves; "Refs #N" otherwise. -->

## Type of change

<!-- Check one. -->
- [ ] Bug fix
- [ ] New feature / enhancement
- [ ] Documentation only
- [ ] Build, CI, or repo tooling
- [ ] DuckDB version bump

## Testing

<!--
Describe what you ran. For changes that touch the query path, please include
results from both:
  ./build/release/test/unittest "*snowflake_*"
  ./build/release/test/unittest "[pushdown]"
If you couldn't run the live-Snowflake suite locally, say so explicitly.
-->

## Checklist

- [ ] Code formatted with `python3 duckdb/scripts/format.py <changed-files>` (or `make format-fix`).
- [ ] `CHANGELOG.md` updated if user-visible behavior changed.
- [ ] Docs (`README.md`, `BUILD.md`, `docs/`) updated if relevant.
- [ ] No `Co-Authored-By:` trailers for tools or AI assistants.
