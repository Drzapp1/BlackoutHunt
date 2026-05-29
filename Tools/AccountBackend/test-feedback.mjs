// Smoke test for the feedback + telemetry ingest and the admin dashboard.
// Dependency-free: boots server.mjs in a throwaway DATA_DIR on an ephemeral port,
// exercises every new route, then tears down. Run: node Tools/AccountBackend/test-feedback.mjs
import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const serverPath = join(here, "server.mjs");
const port = 8000 + Math.floor(Math.random() * 1000);
const base = `http://127.0.0.1:${port}`;
const adminToken = "test-admin-token-123";
const BELL = String.fromCharCode(7); // a control char that must be stripped on ingest

let passed = 0;
let failed = 0;
function check(name, condition) {
  if (condition) {
    passed += 1;
    console.log(`  ok   - ${name}`);
  } else {
    failed += 1;
    console.error(`  FAIL - ${name}`);
  }
}

async function waitForHealth(timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const r = await fetch(`${base}/health`);
      if (r.ok) {
        return await r.json();
      }
    } catch {
      // not up yet
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error("server did not become healthy in time");
}

const dataDir = await mkdtemp(join(tmpdir(), "bh-feedback-test-"));
const child = spawn(process.execPath, [serverPath], {
  env: {
    ...process.env,
    PORT: String(port),
    BIND_HOST: "127.0.0.1",
    PUBLIC_BASE_URL: base,
    ADMIN_TOKEN: adminToken,
    DATA_DIR: dataDir,
    // Pin a low cap so the rate-limit test trips within its 34-request loop, independent
    // of the higher production default in server.mjs.
    FEEDBACK_RATE_LIMIT: "30",
    // Hard-disable the email backup during tests so the suite never sends real mail, even
    // when the developer's shell has SMTP_PASS / FEEDBACK_EMAIL_TO set (inherited via ...process.env).
    SMTP_PASS: "",
    FEEDBACK_EMAIL_TO: "",
  },
  stdio: ["ignore", "pipe", "pipe"],
});
child.stderr.on("data", d => process.stderr.write(`[server] ${d}`));

async function postJson(path, payload) {
  return fetch(`${base}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload),
  });
}

try {
  const health = await waitForHealth(15000);
  check("health reports feedback feature", health.features?.feedback === true);
  check("health reports telemetry feature", health.features?.telemetry === true);

  // 1. Bug report with diagnostics + an embedded control char to verify stripping.
  const bug = await postJson("/feedback", {
    kind: "bug",
    message: `The flashlight flickers near the${BELL} substation breaker.`,
    rating: 4,
    contact: "tester@example.com",
    context: { app_version: "0.5.0-beta.1", platform: "Windows", level: "Substation", role: "Survivor", session_id: "sess-abc", player_name: "Ada", play_seconds: 320 },
    diagnostics: {
      device: { cpu: "Ryzen 5", gpu: "RTX 3060", os: "Windows 11", ram_gb: 16 },
      perf: { samples: 600, session_seconds: 300, avg_fps: 72, min_fps: 41, max_fps: 120, p1_low_fps: 38, hitches: 3 },
      recent_logs: ["LogTemp: Warning: flashlight cookie missing", "LogTemp: breaker toggled"],
    },
  });
  const bugJson = await bug.json();
  check("bug submission returns ok", bug.status === 200 && bugJson.ok === true && typeof bugJson.id === "string");

  // 2. Feature idea.
  const idea = await postJson("/feedback", { kind: "idea", message: "Add a co-op spectator camera." });
  check("idea submission returns ok", idea.status === 200);

  // 3. End-of-round survey (no free-text message is allowed for surveys).
  const survey = await postJson("/feedback", {
    kind: "survey",
    survey: { overall: 5, difficulty: "just_right", would_recommend: true, round_role: "Hunter", round_result: "HunterWin", survived: false },
    context: { level: "Facility" },
  });
  check("survey with no message still accepted", survey.status === 200);

  // 4. Empty non-survey message rejected.
  const empty = await postJson("/feedback", { kind: "bug", message: "   " });
  check("empty bug message rejected with 400", empty.status === 400);

  // 5. Malformed JSON rejected.
  const bad = await fetch(`${base}/feedback`, { method: "POST", headers: { "content-type": "application/json" }, body: "{not json" });
  check("malformed JSON rejected with 400", bad.status === 400);

  // 6. Unknown kind is coerced to "other" and accepted.
  const coerced = await postJson("/feedback", { kind: "totally-made-up", message: "hello" });
  check("unknown kind accepted (coerced to other)", coerced.status === 200);

  // 7. Telemetry session.
  const tel = await postJson("/telemetry/session", {
    reason: "session_end",
    app_version: "0.5.0-beta.1",
    platform: "Windows",
    session_id: "sess-abc",
    player_id: "pid-123",
    player_name: "Ada",
    device: { cpu: "Ryzen 5", gpu: "RTX 3060", os: "Windows 11", ram_gb: 16 },
    perf: { samples: 1200, session_seconds: 600, avg_fps: 70, min_fps: 35, max_fps: 144, p1_low_fps: 33, hitches: 5 },
    round_results: [{ role: "Survivor", result: "SurvivorsWin", survived: true, level: "Facility", at: "2026-05-29T10:00:00Z" }],
    recent_logs: ["LogTemp: round ended"],
  });
  check("telemetry session accepted", tel.status === 200);

  // 8. Admin dashboard requires the key.
  const locked = await fetch(`${base}/admin`);
  check("admin without key is 401", locked.status === 401);

  const dash = await fetch(`${base}/admin?key=${adminToken}`);
  const dashHtml = await dash.text();
  check("admin with key renders 200 HTML", dash.status === 200 && dashHtml.includes("player feedback"));
  check("dashboard shows the bug message", dashHtml.includes("flashlight flickers near"));
  check("dashboard has no raw control char", !dashHtml.includes(BELL));
  check("dashboard shows survey block", dashHtml.includes("would recommend"));
  check("dashboard shows perf summary", dashHtml.includes("FPS avg"));

  // 9. Raw JSON feeds, gated by the key.
  const rawLocked = await fetch(`${base}/admin/feedback.json`);
  check("feedback.json without key is 401", rawLocked.status === 401);
  const raw = await fetch(`${base}/admin/feedback.json?key=${adminToken}`);
  const rawJson = await raw.json();
  check("feedback.json returns 4 entries newest-first", raw.status === 200 && rawJson.entries.length === 4 && rawJson.entries[0].kind === "other");
  const storedBug = rawJson.entries.find(e => e.kind === "bug");
  check("control char stripped from stored message", storedBug && !storedBug.message.includes(BELL) && storedBug.message.includes("flashlight flickers near"));

  const rawTel = await fetch(`${base}/admin/telemetry.json?key=${adminToken}`);
  const rawTelJson = await rawTel.json();
  check("telemetry.json returns 1 entry with perf in diagnostics", rawTel.status === 200 && rawTelJson.entries.length === 1 && rawTelJson.entries[0].diagnostics?.perf?.avg_fps === 70);
  check("telemetry record correlates to a player_id", rawTelJson.entries[0]?.context?.player_id === "pid-123");

  // 10. XSS attempt: a <script> payload must be HTML-escaped in the dashboard, never raw.
  await postJson("/feedback", { kind: "other", message: "<script>alert('xss')</script> & \"quotes\"" });
  const xssDash = await (await fetch(`${base}/admin?key=${adminToken}`)).text();
  check("dashboard has no raw <script> tag (XSS escaped)", !xssDash.includes("<script>alert"));
  check("dashboard shows the escaped script text", xssDash.includes("&lt;script&gt;alert"));

  // 11. Newline/tab integrity: a multi-line message stays one JSONL line and round-trips.
  await postJson("/feedback", { kind: "other", message: "line one\nline two\tcol" });
  const jsonlText = await readFile(join(dataDir, "feedback.jsonl"), "utf8");
  const jsonlLines = jsonlText.split("\n").filter(l => l.trim().length > 0);
  let allParse = true;
  for (const l of jsonlLines) { try { JSON.parse(l); } catch { allParse = false; } }
  check("every feedback.jsonl line is valid JSON (no newline-split records)", allParse);
  const multiline = jsonlLines.map(l => JSON.parse(l)).find(e => e.message && e.message.includes("line one"));
  check("multi-line message preserved its newline", multiline && multiline.message.includes("\n"));

  // 12. Oversized body (>1 MB) is rejected and not persisted.
  const beforeCount = (await (await fetch(`${base}/admin/feedback.json?key=${adminToken}`)).json()).entries.length;
  let oversizeRejected = false;
  try {
    const big = "x".repeat(1024 * 1024 + 4096);
    const r = await postJson("/feedback", { kind: "other", message: big });
    oversizeRejected = r.status !== 200;
  } catch {
    oversizeRejected = true; // socket destroyed mid-stream
  }
  const afterCount = (await (await fetch(`${base}/admin/feedback.json?key=${adminToken}`)).json()).entries.length;
  check("oversized body is rejected", oversizeRejected);
  check("oversized body is not persisted", afterCount === beforeCount);

  // 13. Rate limiting trips after the per-window cap (30 feedback / 5 min).
  let saw429 = false;
  for (let i = 0; i < 34; i++) {
    const r = await postJson("/feedback", { kind: "other", message: `spam ${i}` });
    if (r.status === 429) { saw429 = true; break; }
  }
  check("rate limiter returns 429 after the cap", saw429);
} catch (error) {
  failed += 1;
  console.error("test harness error:", error);
} finally {
  child.kill("SIGTERM");
  await rm(dataDir, { recursive: true, force: true });
}

console.log(`\n${passed} passed, ${failed} failed`);
process.exit(failed === 0 ? 0 : 1);
