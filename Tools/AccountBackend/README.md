# Blackout Hunt Account Backend

This is a dependency-free Node.js scaffold for account login and progress sync.

It supports:

- Google OAuth/OpenID Connect browser login.
- Microsoft OAuth/OpenID Connect browser login.
- Device polling from the game client.
- Local JSON persistence for player profiles and progress.

## Run

Copy `.env.example` to `.env`, fill in the providers you want to enable, then:

```powershell
node .\server.mjs
```

The server loads `Tools\AccountBackend\.env` automatically. Values already set in the shell still take priority.
OAuth login state is signed with `STATE_SECRET`; if that value is empty, the server creates a local `data/state-secret.txt` so login callbacks still survive a backend restart.

For Google, you can enter credentials locally with:

```powershell
.\Configure-GoogleOAuth.ps1
```

Or open the browser setup page while the backend is running:

```text
http://127.0.0.1:8787/setup/google
```

Use `Web application` as the Google OAuth client type. Then copy the generated `Client ID` value, usually ending in `.apps.googleusercontent.com`, and the generated `Client secret` value into the prompts.

The game defaults to:

```ini
[/Script/BlackoutHunt.BHAccountSettings]
BackendBaseUrl="http://127.0.0.1:8787"
```

For real deployment, put this behind HTTPS and set `PUBLIC_BASE_URL` to that public origin. Register these redirect URIs in the Google and Microsoft developer consoles:

- `https://your-domain.example/auth/google/callback`
- `https://your-domain.example/auth/microsoft/callback`

Local development redirect URIs:

- `http://127.0.0.1:8787/auth/google/callback`
- `http://127.0.0.1:8787/auth/microsoft/callback`

Do not ship provider client secrets in the Unreal client.
