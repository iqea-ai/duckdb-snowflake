# Fix: "Sorry Try Again Later" Error in Auth0 SAML

## Problem
When enabling SAML2 Web App in Auth0, you get "sorry try again later" error. The SAML response still shows `Destination="https://manage.auth0.com/tester/samlp"` instead of Snowflake.

## Solution: Set Application Callback URL in UI (Not Just JSON)

The JSON configuration alone isn't enough. You **must** also set the Application Callback URL in the Auth0 UI.

### Step 1: Go to Auth0 SAML Settings

1. Auth0 Dashboard → **Applications** → Your Application
2. **Addons** tab → **SAML2 Web App** → **Settings** (gear icon)

### Step 2: Set Application Callback URL (CRITICAL)

In the **Settings** section (not the JSON editor), find:

**Application Callback URL**: 

Set this to:
```
https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login
```

**Important:** This must be set in the UI field, not just in the JSON!

### Step 3: Set Other UI Fields

Also set these in the UI (not just JSON):

- **Audience**: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
- **Name Identifier Format**: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
- **Name Identifier**: `{email}`

### Step 4: Update JSON Settings (Optional but Recommended)

Scroll down to the **Settings** JSON editor and paste:

```json
{
  "audience": "https://ef33255.us-east-2.aws.snowflakecomputing.com",
  "recipient": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login",
  "mappings": {
    "email": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress",
    "name": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name",
    "given_name": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname",
    "family_name": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname",
    "upn": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/upn"
  },
  "createUpnClaim": true,
  "passthroughClaimsWithNoMapping": false,
  "mapUnknownClaimsAsIs": false,
  "mapIdentities": false,
  "signatureAlgorithm": "rsa-sha256",
  "digestAlgorithm": "sha256",
  "destination": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login",
  "lifetimeInSeconds": 3600,
  "signResponse": true,
  "typedAttributes": true,
  "includeAttributeNameFormat": true,
  "nameIdentifierFormat": "urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress",
  "nameIdentifierProbes": [
    "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress"
  ],
  "authnContextClassRef": "urn:oasis:names:tc:SAML:2.0:ac:classes:PasswordProtectedTransport",
  "logout": {
    "callback": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/logout",
    "slo_enabled": true
  },
  "binding": "urn:oasis:names:tc:SAML:2.0:bindings:HTTP-POST"
}
```

### Step 5: Save Settings

1. Click **Save** at the bottom
2. Wait a few seconds
3. Try enabling again

## Common Issues

### Issue 1: JSON Syntax Error
- Make sure there are no trailing commas
- All strings are properly quoted
- No comments in JSON

### Issue 2: Callback URL Not Set
- **Most Common:** The Application Callback URL field in the UI is empty or wrong
- Must be: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`

### Issue 3: Audience Mismatch
- Audience must match exactly: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
- No trailing slashes

### Issue 4: Auth0 Rate Limiting
- If you've tried multiple times, wait 5-10 minutes
- Clear browser cache
- Try in incognito mode

## Verification

After saving, test again:
1. Click **Debug** in Auth0 SAML settings
2. Check the SAML response
3. The `Destination` should now be: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`
4. Not: `https://manage.auth0.com/tester/samlp`

## If Still Not Working

1. **Check Auth0 Logs:**
   - Dashboard → **Monitoring** → **Logs**
   - Look for errors related to SAML

2. **Verify Integration in Snowflake:**
   ```sql
   DESCRIBE INTEGRATION auth0_saml;
   ```
   - Make sure `ENABLED = true`
   - Verify `SAML2_SSO_URL` matches Auth0 SSO URL

3. **Try Disabling and Re-enabling:**
   - Turn off SAML2 Web App addon
   - Wait 30 seconds
   - Turn it back on
   - Re-configure settings

4. **Check for Conflicting Settings:**
   - Make sure UI fields and JSON settings don't conflict
   - If using JSON, some UI fields might be ignored

## Quick Checklist

- [ ] Application Callback URL set in UI: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`
- [ ] Audience set in UI: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
- [ ] Name Identifier Format: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
- [ ] Name Identifier: `{email}`
- [ ] JSON settings pasted correctly (no syntax errors)
- [ ] Clicked Save
- [ ] Waited a few seconds after saving
- [ ] SAML2 Web App addon is enabled










