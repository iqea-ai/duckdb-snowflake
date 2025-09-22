#!/bin/bash

# Setup script for GitHub repository with automated testing

echo "🚀 Setting up DuckDB Snowflake Extension for GitHub"

# Check if git is initialized
if [ ! -d ".git" ]; then
    echo "❌ Git repository not initialized. Please run 'git init' first."
    exit 1
fi

# Check if GitHub CLI is installed
if ! command -v gh &> /dev/null; then
    echo "⚠️  GitHub CLI not found. Please install it or create the repository manually."
    echo "   Visit: https://cli.github.com/"
    exit 1
fi

# Create GitHub repository
echo "📦 Creating GitHub repository..."
gh repo create sathvikkurap/test1 --public --description "DuckDB Snowflake Extension with Predicate Pushdown and Automated Testing"

# Add remote origin
echo "🔗 Adding remote origin..."
git remote add origin https://github.com/sathvikkurap/test1.git

# Push to GitHub
echo "⬆️  Pushing to GitHub..."
git push -u origin main

echo "✅ Repository setup complete!"
echo ""
echo "🔧 Next steps:"
echo "1. Go to https://github.com/sathvikkurap/test1"
echo "2. Go to Settings > Secrets and variables > Actions"
echo "3. Add these repository secrets:"
echo "   - SNOWFLAKE_ACCOUNT: your_snowflake_account"
echo "   - SNOWFLAKE_USERNAME: your_snowflake_username"
echo "   - SNOWFLAKE_PASSWORD: your_snowflake_password"
echo "   - SNOWFLAKE_DATABASE: SNOWFLAKE_SAMPLE_DATA"
echo ""
echo "4. Go to Actions tab to see automated tests running"
echo "5. Tests will run automatically on every push and pull request"
echo ""
echo "🎉 Your repository is ready with automated testing!"
