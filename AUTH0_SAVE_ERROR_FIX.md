# Fix: Auth0 "We couldn't save the changes" Error

## Common Causes and Solutions

### Solution 1: Set Fields in UI First (Not JSON)

**Try this approach:**

1. **Don't use JSON editor initially** - Set fields in the UI form first
2. Go to Auth0 → Applications → Your App → Addons → SAML2 Web App → Settings
3. Fill in these fields in the **form** (not JSON):
   - **Application Callback URL**: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`
   - **Audience**: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
   - **Name Identifier Format**: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
   - **Name Identifier**: `{email}`
4. Click **Save** (without touching JSON)
5. If that works, then add JSON settings

### Solution 2: Validate JSON Syntax

The JSON might have syntax errors. Use this validated version:

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

**Check for:**
- No trailing commas
- All strings properly quoted
- No comments (`//` or `/* */`)
- Valid JSON structure

### Solution 3: Minimal Configuration First

Try saving with minimal settings first:

**In UI form:**
- Application Callback URL: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`
- Audience: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
- Name Identifier Format: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
- Name Identifier: `{email}`

**Leave JSON empty or minimal:**
```json
{
  "audience": "https://ef33255.us-east-2.aws.snowflakecomputing.com",
  "recipient": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login",
  "destination": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login",
  "nameIdentifierFormat": "urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress"
}
```

Save this first, then add more settings later.

### Solution 4: Check Auth0 Logs

1. Go to Auth0 Dashboard → **Monitoring** → **Logs**
2. Look for errors around the time you tried to save
3. Check for validation errors or API errors

### Solution 5: Clear and Retry

1. **Disable SAML2 Web App addon** (toggle it off)
2. Wait 30 seconds
3. **Re-enable** SAML2 Web App addon
4. Configure settings again
5. Save

### Solution 6: Use Browser Developer Tools

1. Open browser Developer Tools (F12)
2. Go to **Console** tab
3. Try saving again
4. Look for JavaScript errors in console
5. Go to **Network** tab
6. Try saving again
7. Look for failed API requests (red entries)
8. Click on failed request → **Response** tab to see error details

### Solution 7: Try Different Browser

- Auth0 UI can have browser-specific issues
- Try Chrome, Firefox, or Safari
- Try incognito/private mode

### Solution 8: Check URL Format

Make sure URLs:
- Start with `https://`
- No trailing slashes (except `/fed/login`)
- No spaces
- Valid domain format

### Solution 9: Wait and Retry

- Auth0 might be rate-limiting you
- Wait 5-10 minutes
- Try again

### Solution 10: Contact Auth0 Support

If nothing works:
- Check Auth0 Status page: https://status.auth0.com/
- Contact Auth0 support with:
  - Error message
  - Screenshot of settings
  - Time when error occurred

## Recommended Approach

**Try this order:**

1. **First:** Set only UI fields (no JSON):
   - Application Callback URL
   - Audience  
   - Name Identifier Format
   - Name Identifier
   - Click Save

2. **If that works:** Then add minimal JSON:
   ```json
   {
     "audience": "https://ef33255.us-east-2.aws.snowflakecomputing.com",
     "recipient": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login",
     "destination": "https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login"
   }
   ```

3. **If that works:** Then add full JSON configuration

## Quick Test

Try this minimal test:
1. Clear all JSON settings (delete everything)
2. Set only:
   - Application Callback URL: `https://ef33255.us-east-2.aws.snowflakecomputing.com/fed/login`
   - Audience: `https://ef33255.us-east-2.aws.snowflakecomputing.com`
3. Save
4. If this works, the issue is in the JSON configuration










