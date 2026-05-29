# Always-on hosting for the feedback backend

The feedback backend, Playit agent, and Caddy currently run on your PC. Feedback (and the
email backup) only arrive **while that machine is on**. To receive feedback when your PC is
off, run the stack on an always-on host. The backend is dependency-free Node, so it runs
anywhere.

## What "always-on" needs
The public address `https://blackouthunt-feedback.playit.plus` must reach a running backend
24/7. Two ways:

### Path A — Cloud VPS, keep the Playit domain (no game re-cook) — recommended
Keeps the baked-in game URL unchanged, so already-cooked builds keep working.

1. **Get a free/cheap always-on Linux VPS** that never sleeps:
   - Oracle Cloud **Always-Free** VM (no monthly cost), or a ~$4/mo Hetzner/DigitalOcean box.
   - Avoid "free tiers that sleep" (e.g. Render free) — a sleeping host misses feedback.
2. **Install Node 20+ and Caddy** on the VPS, and the Playit agent.
3. **Second Playit agent for the VPS:** in the Playit dashboard create/claim a new agent on the
   VPS (Premium allows multiple agents), then **reassign the `BlackoutHuntReporting` HTTPS
   tunnel's agent** to it. Your PC keeps its own agent for the game's UDP tunnel.
   - Restart the VPS agent after reassigning (stale routing table = dropped traffic).
4. **Copy `Tools/AccountBackend/`** to the VPS (everything except `data/` and `.env`).
5. **Set environment on the VPS** (systemd unit `Environment=` or the VPS `.env`):
   - `ADMIN_TOKEN` (reuse yours, or set a new one)
   - `PUBLIC_BASE_URL=https://blackouthunt-feedback.playit.plus`
   - `FEEDBACK_EMAIL_TO=<your-email>@gmail.com`, `SMTP_HOST=smtp.gmail.com`, `SMTP_PORT=587`,
     `SMTP_USER=<your-email>@gmail.com`, `SMTP_PASS=<your Gmail App Password>`
   - `FEEDBACK_RATE_LIMIT=300`, `TELEMETRY_RATE_LIMIT=600`
6. **Run all three as services** (systemd): `node server.mjs`, the Playit agent, and
   `caddy run --config Caddyfile`. Caddy's Caddyfile is the same; it fetches the LE cert over
   the tunnel. (Linux Caddy: `apt install caddy` or the static binary.)
7. **Stop the feedback stack on your PC** so two agents don't both try to serve the tunnel.

Result: `https://blackouthunt-feedback.playit.plus` is served 24/7 from the VPS; feedback and
the email backup arrive whether or not your PC is on. No game re-cook (URL unchanged).

### Path B — Cloud VPS, direct (no Playit for feedback)
Simpler server-side, but the domain (hence the baked game URL) changes → **requires a game
re-cook**.

1. VPS with a public IP; point a domain's DNS `A` record at it (a cheap domain or a free
   subdomain provider).
2. Run `node server.mjs` + Caddy with `yourdomain { reverse_proxy 127.0.0.1:8787 }` — Caddy
   gets the cert directly (no Playit). Set the same env as above.
3. Update `Config/DefaultGame.ini` `FeedbackBackendBaseUrl` to the new domain and re-cook.

## Notes
- **Email volume:** the backup emails one message per feedback. A full class submitting
  end-of-round surveys at once = many emails. To exclude bulk surveys, set
  `EMAIL_FEEDBACK_KINDS=bug,idea,praise,other` on the host. The dashboard/`feedback.jsonl`
  always keep everything regardless.
- **Secrets:** keep `SMTP_PASS` and `ADMIN_TOKEN` in the host's environment or its (untracked)
  `.env` — never commit them. `.env` and `data/` are git-ignored.
- The local launcher `Start-FeedbackStack.ps1` is for PC/home hosting; on Linux use systemd
  units instead.
