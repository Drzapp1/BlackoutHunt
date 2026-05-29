# Blackout Hunt Account Backend

This is a dependency-free Node.js scaffold for account login and progress sync.

It supports:

- Google OAuth/OpenID Connect browser login.
- Microsoft OAuth/OpenID Connect browser login.
- Device polling from the game client.
- Local JSON persistence for player profiles and progress.
- **Player feedback ingest** (bug reports, feature ideas, end-of-round surveys) and **performance/log telemetry**, with an owner-only dashboard so submissions land on your machine and are visible to you.

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

## Feedback & telemetry

The game posts player feedback and (with consent) performance/log diagnostics to the same backend:

| Method | Path | Auth | Purpose |
| --- | --- | --- | --- |
| `POST` | `/feedback` | none | Bug report, feature idea, praise, "other", or end-of-round `survey`. Guests included. |
| `POST` | `/telemetry/session` | none | Per-session performance summary (FPS/hitches), device info, and recent log tail. |
| `GET` | `/admin?key=TOKEN` | admin token | HTML dashboard of all feedback + telemetry, newest first, auto-refresh. |
| `GET` | `/admin/feedback.json?key=TOKEN` | admin token | Raw feedback entries. |
| `GET` | `/admin/telemetry.json?key=TOKEN` | admin token | Raw telemetry entries. |

Submissions are appended to `data/feedback.jsonl` and `data/telemetry.jsonl` (one JSON object per line, easy to grep or import). Ingest is unauthenticated so any player can submit, but it is rate-limited per IP, body-size capped (1 MB), length-clamped, and control characters are stripped. The dashboard is locked behind an admin token.

The **admin token** is read from `ADMIN_TOKEN` in `.env`; if unset, the server generates one into `data/admin-token.txt` and prints the full dashboard URL to the console at startup:

```text
Feedback dashboard: http://127.0.0.1:8787/admin?key=...
```

### Making submissions reach you from other machines

By default the server binds to `127.0.0.1`, so only the host machine can reach it. To receive feedback from other players:

- **LAN / classroom:** start with `BIND_HOST=0.0.0.0` and point each client's `BackendBaseUrl` at the host's LAN address (e.g. `http://192.168.1.10:8787`).
- **Internet:** front the loopback port with the bundled Playit tunnel (see `Docs/ONLINE_SERVICES.md`) and set `BackendBaseUrl`/`PUBLIC_BASE_URL` to the tunnel origin. Keep the admin token private.

Other environment knobs: `PORT`, `BIND_HOST`, `DATA_DIR` (relocate `feedback.jsonl`/`telemetry.jsonl`/`players.json`), `ADMIN_TOKEN`, `PUBLIC_BASE_URL`.

### Test

```powershell
node .\test-feedback.mjs
```

Boots the server in a throwaway data directory on an ephemeral port and exercises every feedback/telemetry/admin route.
