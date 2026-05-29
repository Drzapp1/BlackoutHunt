# Always-on hosting for the feedback backend

The feedback backend, Playit agent, and Caddy currently run on your PC, so feedback (and the
email backup) only arrive **while that machine is on**. To receive feedback when your PC is
off, run the backend on an always-on host. It's dependency-free Node, so it runs anywhere.

A deployable copy of just the backend (no secrets) is published at
**https://github.com/Drzapp1/blackouthunt-feedback** — that's what the cloud host builds from.

---

## Recommended: Koyeb (free, always-on, ~5 min)

[Koyeb](https://www.koyeb.com)'s free tier runs one web service continuously (no sleep) and
usually needs no credit card. It gives an `https://<app>.koyeb.app` URL with its own TLS, so
**no Playit or Caddy is needed there** — just the Node app.

### Steps
1. Sign up at **https://app.koyeb.com** (Google or GitHub login).
2. **Create Web Service → GitHub** → authorize → pick the repo
   `Drzapp1/blackouthunt-feedback` (branch `main`). (Or "Public GitHub repository" and paste
   the URL.)
3. **Builder:** Buildpack (auto-detects Node) with run command `node server.mjs` — or choose
   Dockerfile; both are in the repo.
4. **Instance:** Free. **Port:** `8000` (http). **Health check path:** `/health`.
5. **Environment variables** — add these (values are listed privately in the chat / your
   notes; never commit secrets):

   | Variable | Value |
   | --- | --- |
   | `BIND_HOST` | `0.0.0.0` |
   | `ADMIN_TOKEN` | your dashboard token |
   | `FEEDBACK_EMAIL_TO` | your email |
   | `SMTP_HOST` | `smtp.gmail.com` |
   | `SMTP_PORT` | `587` |
   | `SMTP_USER` | your email |
   | `SMTP_PASS` | your Gmail **App Password** |
   | `FEEDBACK_EMAIL_FROM` | your email |
   | `EMAIL_FEEDBACK_KINDS` | `bug,idea,praise,other` |
   | `FEEDBACK_RATE_LIMIT` | `300` |
   | `TELEMETRY_RATE_LIMIT` | `600` |

6. **Deploy.** Copy the assigned URL, e.g. `https://blackouthunt-feedback-<you>.koyeb.app`.
7. (Optional) add `PUBLIC_BASE_URL=https://<that-url>` and redeploy so dashboard links are absolute.
8. **Point the game at it:** set `Config/DefaultGame.ini` →
   `FeedbackBackendBaseUrl="https://<that-url>"` and **re-cook** the game.
9. **Verify:** open `https://<that-url>/admin?key=<ADMIN_TOKEN>`, or
   `curl https://<that-url>/health`. Submit once and confirm the email arrives.

Once Koyeb is live and the game points at it, you can **stop the PC stack** (Node + Playit
agent + Caddy) — Koyeb is the always-on receiver. The game's *multiplayer* Playit tunnel is
unrelated and stays as-is.

### Notes
- **Ephemeral disk:** on the free tier `data/*.jsonl` resets on redeploys, so the dashboard
  history isn't permanent — but the **email backup is the durable per-submission record**, and
  feedback still shows live on the dashboard between redeploys.
- **Bandwidth:** the free tier's 1 GB/mo is far more than tiny JSON feedback needs.
- If your region forces a credit card at signup, use one of the VPS paths below instead.

---

## Alternative A — Cloud VPS, keep the Playit domain (no game re-cook)
Keeps `https://blackouthunt-feedback.playit.plus` so already-cooked builds keep working.

1. Get a free/always-on Linux VPS that never sleeps (Oracle Cloud **Always-Free**, or a
   ~$4/mo Hetzner/DigitalOcean box). Avoid sleep-on-idle free tiers.
2. Install Node 20+, Caddy, and the Playit agent.
3. In the Playit dashboard create a new agent on the VPS and **reassign the
   `BlackoutHuntReporting` HTTPS tunnel** to it. Your PC keeps its own agent for the game's
   UDP tunnel. Restart the VPS agent after reassigning.
4. Copy this folder (minus `data/` and `.env`); set the same env vars as the Koyeb table.
5. Run `node server.mjs`, the Playit agent, and `caddy run --config Caddyfile` as systemd
   services. Stop the feedback stack on your PC.

Result: the existing URL is served 24/7 from the VPS — no game re-cook.

## Alternative B — Cloud VPS, direct (requires game re-cook)
VPS with a public IP; point a domain's `A` record at it; run `node server.mjs` + Caddy
(`yourdomain { reverse_proxy 127.0.0.1:8787 }`) with the same env. Update
`FeedbackBackendBaseUrl` to the new domain and re-cook.

---

## General notes
- **Email volume:** `EMAIL_FEEDBACK_KINDS=bug,idea,praise,other` skips bulk end-of-round
  surveys in email (they're still logged + on the dashboard). Drop it to email everything.
- **Secrets:** keep `SMTP_PASS` and `ADMIN_TOKEN` in the host's environment, never in a
  committed file. `.env` and `data/` are git-ignored.
- The local launcher `Start-FeedbackStack.ps1` is for PC/home hosting; on Linux use systemd.
