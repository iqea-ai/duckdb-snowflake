#!/usr/bin/env bash
#
# Create the issue/PR label taxonomy for iqea-ai/duckdb-snowflake.
#
# Run this once against the repo (you need the `gh` CLI authenticated with
# write access). It is idempotent: `gh label create --force` will create the
# label if it doesn't exist and update color/description if it does.
#
# Usage:
#   ./scripts/labels.sh                       # uses the current repo from git remote
#   GH_REPO=iqea-ai/duckdb-snowflake ./scripts/labels.sh
#

set -euo pipefail

if ! command -v gh >/dev/null 2>&1; then
  echo "error: gh CLI not found on PATH" >&2
  exit 1
fi

REPO_FLAG=()
if [[ -n "${GH_REPO:-}" ]]; then
  REPO_FLAG=(--repo "$GH_REPO")
fi

create() {
  local name="$1"
  local color="$2"
  local description="$3"
  gh label create "$name" \
    --color "$color" \
    --description "$description" \
    --force \
    "${REPO_FLAG[@]}"
}

# Type
create "bug"              "d73a4a" "Something is broken or behaves unexpectedly"
create "enhancement"      "a2eeef" "New feature or capability request"
create "documentation"    "0075ca" "Docs, examples, READMEs, comments"

# Triage / lifecycle
create "needs-repro"      "fbca04" "Cannot reproduce yet — waiting on more info from reporter"
create "upstream-blocked" "5319e7" "Fix or feature depends on DuckDB core, ADBC driver, or Snowflake"
create "wontfix"          "ffffff" "Closed without action — out of scope or by design"

# Onboarding
create "good first issue" "7057ff" "Scoped, well-understood, friendly to a first-time contributor"

echo "Done."
