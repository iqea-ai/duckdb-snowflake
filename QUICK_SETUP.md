# Quick Setup for Private GitHub Repository

## Option 1: Use the Setup Script (Recommended)

```bash
./setup-private-repo.sh
```

This script will guide you through:
- Adding your repository URL
- Setting up authentication
- Pushing the code
- Configuring GitHub secrets

## Option 2: Manual Setup

### 1. Add Your Repository

```bash
# Replace with your actual repository URL
git remote add private-repo https://github.com/sathvikkurap/test1.git

# Or if using SSH
git remote add private-repo git@github.com:sathvikkurap/test1.git
```

### 2. Push to Your Repository

```bash
git push private-repo main
```

### 3. Set Up GitHub Secrets

Go to your GitHub repository → Settings → Secrets and variables → Actions

Add these secrets:
- `SNOWFLAKE_ACCOUNT` = `wdbniqg-ct63033`
- `SNOWFLAKE_USERNAME` = `ptandra`
- `SNOWFLAKE_PASSWORD` = `LM7qsuw6XGbuBQK`
- `SNOWFLAKE_DATABASE` = `SNOWFLAKE_SAMPLE_DATA`

### 4. Verify Tests Run

1. Go to your repository on GitHub
2. Click the "Actions" tab
3. You should see "Snowflake SQL Logic Tests" workflow
4. Click on it to see test results

## What's Included

✅ **Comprehensive Test Suite**: 20+ tests verifying pushdown functionality
✅ **GitHub Actions Workflow**: Automatic test running on every push/PR
✅ **Production Ready**: Proper error handling and CI/CD integration
✅ **Debug Verification**: Tests confirm actual query pushdown is working

## Test Files

- `test/sql/snowflake_pushdown_proof.test` - Main pushdown verification
- `test/sql/snowflake_basic_connectivity.test` - Basic connectivity
- `.github/workflows/snowflake-sql-tests.yml` - CI workflow

## Troubleshooting

**Repository not found**: Make sure the repository exists and you have access
**Authentication failed**: Use Personal Access Token or SSH key
**Tests fail**: Check that GitHub secrets are set correctly

For detailed instructions, see `SETUP_GITHUB_REPO.md`
