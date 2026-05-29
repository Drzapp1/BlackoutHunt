# In-Game Feedback & Telemetry

Blackout Hunt collects player feedback and (with consent) lightweight performance/log
diagnostics, and sends them to **your** machine so you can read them — not just to the
player's local disk.

There are three entry points in-game, all in the pause/main menu's **Feedback** tab
(press <kbd>Esc</kbd>):

- **Bug report / Feature idea / Liked it / Other** — a free-text message, an optional 1–5
  rating, an optional contact, and an "Include diagnostics" checkbox.
- **End-of-round survey** — when a round resolves to a win, the menu opens on the Feedback
  tab with a 20-second survey (overall 1–5, difficulty, would-play-again, optional comment).
  Auto-prompt fires once per session and can be turned off.
- **Performance & log telemetry** — a per-session FPS/hitch summary, recent log tail, and PC
  specs, submitted automatically at round end (and available via the diagnostics checkbox).

## Where the data goes

Everything is posted to the small Node.js backend in [`Tools/AccountBackend`](../Tools/AccountBackend)
— the same server the account/login system already uses. New routes:

| Method | Path | Auth | Purpose |
| --- | --- | --- | --- |
| `POST` | `/feedback` | none | Bug / idea / praise / other / `survey`. Guests included. |
| `POST` | `/telemetry/session` | none | Per-session FPS/hitch summary, device info, recent logs. |
| `GET` | `/admin?key=TOKEN` | admin token | **Your dashboard.** All feedback + telemetry, newest first, auto-refresh. |
| `GET` | `/admin/feedback.json?key=TOKEN` | admin token | Raw feedback entries. |
| `GET` | `/admin/telemetry.json?key=TOKEN` | admin token | Raw telemetry entries. |

Submissions append to `Tools/AccountBackend/data/feedback.jsonl` and `telemetry.jsonl`
(one JSON object per line). Ingest is unauthenticated so any player can submit, but it is
rate-limited per IP, body-size capped (1 MB), length-clamped, and control characters are
stripped; the dashboard is locked behind an admin token.

## Setup (read your players' feedback)

1. **Run the backend** on your machine:
   ```powershell
   cd Tools\AccountBackend
   node .\server.mjs
   ```
   On first run it prints your dashboard URL (with the admin token) to the console:
   ```text
   Feedback dashboard: http://127.0.0.1:8787/admin?key=...
   ```
   The token is also saved to `Tools/AccountBackend/data/admin-token.txt`; you can fix it by
   setting `ADMIN_TOKEN` in `.env`.

2. **Point the game at the backend.** Set the URL in `Config/DefaultGame.ini` (or per-build):
   ```ini
   [/Script/BlackoutHunt.BHAccountSettings]
   BackendBaseUrl="http://127.0.0.1:8787"
   ```
   You can override just the feedback endpoint without touching accounts:
   ```ini
   [/Script/BlackoutHunt.BHFeedbackSettings]
   FeedbackBackendBaseUrl="http://your-host:8787"
   ```

3. **Let other machines reach you** (so it isn't only your own PC submitting):
   - **LAN / classroom:** start the backend with `BIND_HOST=0.0.0.0` and set each client's
     `BackendBaseUrl` to the host's LAN address, e.g. `http://192.168.1.10:8787`.
   - **Internet:** front the loopback port with the bundled Playit tunnel (see
     [`ONLINE_SERVICES.md`](ONLINE_SERVICES.md)) and set `BackendBaseUrl`/`PUBLIC_BASE_URL`
     to the tunnel origin. Keep the admin token private.

4. **Open the dashboard** at `http://<host>:8787/admin?key=<token>`.

If no backend URL is configured, the game does **not** lose feedback: it writes each
submission to `Saved/Feedback/*.json` on the player's machine and tells them so. A failed
network submission is likewise saved there as a backup. Configure a URL to have it reach you.

## Settings & consent

`[/Script/BlackoutHunt.BHFeedbackSettings]` (Project Settings → "Feedback", or
`Config/DefaultGame.ini`):

| Key | Default | Meaning |
| --- | --- | --- |
| `bEnableFeedback` | `True` | Master switch for the whole feature. |
| `bEnablePerformanceTelemetry` | `True` | Collect/send FPS/hitch + log summaries. |
| `bIncludeDiagnosticsByDefault` | `True` | Whether the form's "Include diagnostics" box starts ticked. |
| `bAutoPromptEndOfRoundSurvey` | `True` | Auto-open the survey at round end (once per session). |
| `RecentLogLineCount` | `200` | Size of the in-memory log ring buffer. |
| `MinimumLogVerbosity` | `Warning` | Lowest severity captured (`Error`/`Warning`/`Display`/`Log`/`Verbose`). |
| `HitchThresholdMs` | `100` | A single frame slower than this counts as a hitch. |
| `FeedbackBackendBaseUrl` | `""` | Optional endpoint override; falls back to `BackendBaseUrl`. |

**What is sent:** the typed message, optional rating/contact, and a small context block
(app/engine version, platform, session id, current level/role/round phase, play time, and the
account display name/id if signed in). Diagnostics — only when the box is ticked, or for the
automatic perf summary — add CPU/GPU/RAM/OS, the FPS/hitch summary, and the recent log tail.
No personal data is collected unless the player types it into the message or contact field.

## How it works (code)

- [`UBHFeedbackSubsystem`](../Source/BlackoutHunt/BHFeedbackSubsystem.h) — a
  `GameInstanceSubsystem`. Samples frame time every frame via an `FTSTicker` (avg/min/max FPS,
  1% low from a histogram, hitch count), captures recent log lines through a thread-safe
  `FBHFeedbackLogSink : FOutputDevice` ring buffer, builds the JSON payloads, and POSTs them
  with the same async HTTP path the account subsystem uses.
- [`ABHPlayerController`](../Source/BlackoutHunt/BHPlayerController.h) routes the menu's
  submit actions (`SubmitFeedbackForMenu`, `SubmitRoundSurveyForMenu`), records each round
  result for telemetry in `ClientRecordRoundResult`, and arms/auto-opens the survey in
  `HandleRoundPhaseUiState` when a round resolves to a win.
- [`SBHMainMenu`](../Source/BlackoutHunt/SBHMainMenu.h) renders the **Feedback** tab and the
  end-of-round survey.

Console exec helpers for testing: `FeedbackSend <message>` and `FeedbackSendDiagnostics`.

## Testing

- Backend: `cd Tools/AccountBackend && node test-feedback.mjs` (boots the server in a throwaway
  data dir on an ephemeral port and exercises every route).
- Native: the `BlackoutHunt.Feedback.LogSink` automation test covers the log ring buffer
  (verbosity filtering, oldest-first ordering, wraparound, line clamping).
