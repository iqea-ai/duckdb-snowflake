#!/bin/bash

# Setup script for private GitHub repository
# This script helps you push the DuckDB Snowflake extension to your private repo

echo "🚀 Setting up DuckDB Snowflake Extension for Private Repository"
echo "=============================================================="

# Check if git is available
if ! command -v git &> /dev/null; then
    echo "❌ Git is not installed. Please install git first."
    exit 1
fi

# Get repository URL from user
echo ""
echo "📝 Please provide your private repository details:"
echo ""

read -p "Enter your GitHub username: " GITHUB_USERNAME
read -p "Enter your repository name: " REPO_NAME

REPO_URL="https://github.com/${GITHUB_USERNAME}/${REPO_NAME}.git"

echo ""
echo "🔗 Repository URL: $REPO_URL"
echo ""

# Add remote
echo "➕ Adding remote repository..."
git remote add private-repo "$REPO_URL"

# Check if remote was added successfully
if git remote get-url private-repo &> /dev/null; then
    echo "✅ Remote added successfully"
else
    echo "❌ Failed to add remote. Please check your repository URL."
    exit 1
fi

echo ""
echo "🔐 Authentication Options:"
echo "1. Personal Access Token (Recommended for private repos)"
echo "2. SSH Key"
echo "3. GitHub CLI"
echo ""

read -p "Choose authentication method (1-3): " AUTH_METHOD

case $AUTH_METHOD in
    1)
        echo ""
        echo "🔑 Personal Access Token Setup:"
        echo "1. Go to GitHub.com → Settings → Developer settings → Personal access tokens"
        echo "2. Generate a new token with 'repo' permissions"
        echo "3. Copy the token"
        echo ""
        read -p "Enter your personal access token: " -s TOKEN
        echo ""
        
        # Update remote URL with token
        AUTH_URL="https://${TOKEN}@github.com/${GITHUB_USERNAME}/${REPO_NAME}.git"
        git remote set-url private-repo "$AUTH_URL"
        echo "✅ Authentication configured with token"
        ;;
    2)
        echo ""
        echo "🔑 SSH Key Setup:"
        echo "1. Make sure you have SSH keys set up with GitHub"
        echo "2. Update the remote URL to use SSH"
        echo ""
        SSH_URL="git@github.com:${GITHUB_USERNAME}/${REPO_NAME}.git"
        git remote set-url private-repo "$SSH_URL"
        echo "✅ Remote updated to use SSH"
        ;;
    3)
        echo ""
        echo "🔑 GitHub CLI Setup:"
        echo "Make sure you're logged in with: gh auth login"
        echo "✅ Using GitHub CLI authentication"
        ;;
    *)
        echo "❌ Invalid option. Please run the script again."
        exit 1
        ;;
esac

echo ""
echo "📤 Pushing to private repository..."

# Push to the private repository
if git push private-repo main; then
    echo "✅ Successfully pushed to private repository!"
    echo ""
    echo "🎉 Next Steps:"
    echo "1. Go to your GitHub repository: https://github.com/${GITHUB_USERNAME}/${REPO_NAME}"
    echo "2. Go to Settings → Secrets and variables → Actions"
    echo "3. Add these secrets:"
    echo "   - SNOWFLAKE_ACCOUNT: wdbniqg-ct63033"
    echo "   - SNOWFLAKE_USERNAME: ptandra"
    echo "   - SNOWFLAKE_PASSWORD: LM7qsuw6XGbuBQK"
    echo "   - SNOWFLAKE_DATABASE: SNOWFLAKE_SAMPLE_DATA"
    echo "4. Go to Actions tab to see tests running automatically"
    echo ""
    echo "📚 See SETUP_GITHUB_REPO.md for detailed instructions"
else
    echo "❌ Failed to push to repository. Please check:"
    echo "   - Repository exists and you have access"
    echo "   - Authentication is correct"
    echo "   - Repository is not empty (if it's a new repo, add a README first)"
    echo ""
    echo "💡 If it's a new repository, create it on GitHub first with a README"
fi

echo ""
echo "🔧 Manual Commands (if needed):"
echo "git remote add private-repo $REPO_URL"
echo "git push private-repo main"
