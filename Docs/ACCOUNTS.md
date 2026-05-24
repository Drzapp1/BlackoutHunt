# Accounts and Progress

Blackout Hunt now has a local account/progress layer plus backend hooks for Google and Microsoft sign-in.

## In-Game Commands

- Local guest profile: `AccountGuest`
- Google browser login: `LoginGoogle`
- Microsoft browser login: `LoginMicrosoft`
- Manually check browser login status: `AccountPollLogin`
- Sync local progress to backend: `AccountSync`
- Sign out to local guest mode: `AccountSignOut`
- Create/update encrypted local credentials: `AccountCreateLocal <username> <password>`
- Sign in with encrypted local credentials: `AccountLoginLocal <username> <password>`
- Remove encrypted local credentials: `AccountForgetLocal`
- Reset local classroom data: `AccountResetLocalClassroomData`

The menu exposes the same actions in the `Account` tab. The first-load start screen also has a compact credentials panel.

## Local Save Files

The game writes local account files under:

- `D:\MainGame\Saved\Account\profile.json`
- `D:\MainGame\Saved\Account\progress.json`
- `D:\MainGame\Saved\Account\local_credentials.enc.json`

`local_credentials.enc.json` stores the local username and password hash inside an AES-encrypted, HMAC-checked payload tied to the current Windows login and project path. Passwords must be 8-128 characters. This is for local classroom convenience, not a replacement for server-side account security.

Round-end progress is recorded on each client when the server ends a round:

- rounds played
- hunter wins
- survivor wins
- survivor escapes
- XP

## Backend

The Unreal client is configured by:

```ini
[/Script/BlackoutHunt.BHAccountSettings]
BackendBaseUrl="http://127.0.0.1:8787"
bEnableExternalAccountLogin=True
LoginPollSeconds=2.0
```

Classroom builds should keep:

```ini
[/Script/BlackoutHunt.BHAccountSettings]
BackendBaseUrl=""
bEnableExternalAccountLogin=False
LoginPollSeconds=2.0
```

With external login disabled, round progress is saved locally and no backend sync is attempted.

The local backend scaffold is in:

`D:\MainGame\Tools\AccountBackend`

Run it with Node.js after setting provider environment variables:

```powershell
node .\Tools\AccountBackend\server.mjs
```

The game opens the system browser for Google or Microsoft sign-in, polls `/auth/device/{device_id}`, then stores the returned game session token locally. Progress sync sends a bearer-authenticated `PUT /player/save` request.

For production, deploy the backend behind HTTPS and register the provider redirect URLs from `Tools\AccountBackend\README.md`. Do not put Google or Microsoft client secrets in the Unreal client.
