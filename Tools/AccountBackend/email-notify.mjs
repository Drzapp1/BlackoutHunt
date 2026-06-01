// Dependency-free email backup for player feedback.
//
// Sends one plain-text email per feedback submission so every piece of feedback lands in
// your inbox as a durable copy — independent of the dashboard. Uses raw SMTP over implicit
// TLS (port 465), so it works with Gmail (App Password) or any SMTP provider, with zero npm
// dependencies. Disabled automatically when SMTP_PASS or FEEDBACK_EMAIL_TO is empty.
//
// Env:
//   FEEDBACK_EMAIL_TO     recipient (your inbox). Required to enable.
//   SMTP_HOST             default smtp.gmail.com
//   SMTP_PORT             default 465 (implicit TLS)
//   SMTP_USER             SMTP login (for Gmail: your full address)
//   SMTP_PASS             SMTP password. For Gmail use a 16-char App Password. Required to enable.
//   FEEDBACK_EMAIL_FROM   From address (default: SMTP_USER)
//   EMAIL_FEEDBACK_KINDS  comma list of kinds to email (default: bug,idea,praise,other,survey)
//
// CLI:
//   node email-notify.mjs --dry     compose a sample email and print it (no send)
//   node email-notify.mjs --test    send a real test email using the current env/.env
import tls from "node:tls";
import net from "node:net";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export function emailSettings() {
  return {
    host: process.env.SMTP_HOST || "smtp.gmail.com",
    port: Number(process.env.SMTP_PORT || 465),
    user: process.env.SMTP_USER || "",
    pass: process.env.SMTP_PASS || "",
    to: process.env.FEEDBACK_EMAIL_TO || "",
    from: process.env.FEEDBACK_EMAIL_FROM || process.env.SMTP_USER || "",
    kinds: (process.env.EMAIL_FEEDBACK_KINDS || "bug,idea,praise,other,survey")
      .split(",").map(s => s.trim()).filter(Boolean),
  };
}

export function emailConfigured() {
  const s = emailSettings();
  return Boolean(s.host && s.port && s.user && s.pass && s.to && s.from);
}

// RFC 2047 encoded-word so non-ASCII subjects survive.
function encodeSubject(text) {
  const clean = String(text).replace(/[\r\n]+/g, " ").slice(0, 180);
  if (/^[\x20-\x7E]*$/.test(clean)) {
    return clean;
  }
  return `=?UTF-8?B?${Buffer.from(clean, "utf8").toString("base64")}?=`;
}

function wrapBase64(b64) {
  return b64.replace(/(.{1,76})/g, "$1\r\n").trimEnd();
}

function composeMessage(record) {
  const ctx = record.context || {};
  const ctxBits = [ctx.app_version && `v${ctx.app_version}`, ctx.platform, ctx.level, ctx.role, ctx.round_phase,
    ctx.play_seconds ? `${ctx.play_seconds}s` : null].filter(Boolean).join(" · ");
  const who = ctx.player_name ? `${ctx.player_name}${record.contact ? ` (contact: ${record.contact})` : ""}`
    : (record.contact || "anonymous");
  const ratingTxt = record.rating ? `${record.rating}/5` : "—";

  const lines = [];
  lines.push(`Kind:    ${record.kind}`);
  lines.push(`Rating:  ${ratingTxt}`);
  lines.push(`From:    ${who}`);
  lines.push(`When:    ${record.received_at || ""}`);
  if (ctxBits) lines.push(`Context: ${ctxBits}`);
  lines.push("");
  if (record.message) { lines.push("Message:", record.message, ""); }
  if (record.survey) {
    const s = record.survey;
    lines.push("Survey:",
      `  overall ${s.overall || "-"}/5 · difficulty ${s.difficulty || "-"} · ${s.would_recommend ? "would recommend" : "would NOT recommend"}` +
      `${s.round_role ? ` · ${s.round_role}` : ""}${s.round_result ? ` ${s.round_result}` : ""}${s.survived ? " (survived)" : ""}`, "");
  }
  if (record.diagnostics) {
    const d = record.diagnostics;
    if (d.perf) lines.push(`Perf: FPS avg ${d.perf.avg_fps}/min ${d.perf.min_fps}/1%low ${d.perf.p1_low_fps} · ${d.perf.hitches} hitches · ${d.perf.session_seconds}s`);
    if (d.device) lines.push(`Device: ${d.device.cpu || "?"} · ${d.device.gpu || "?"} · ${d.device.ram_gb}GB · ${d.device.os || "?"}`);
    lines.push("");
  }
  lines.push("— Blackout Hunt feedback backend");

  const snippet = (record.message || (record.survey ? `survey overall ${record.survey.overall || "-"}/5` : "feedback"))
    .replace(/\s+/g, " ").slice(0, 80);
  const subject = `[Blackout Hunt] ${record.kind}${record.rating ? ` ${record.rating}★` : ""} — ${snippet}`;
  return { subject, body: lines.join("\n") };
}

function readReply(socket, timeoutMs) {
  return new Promise((resolve, reject) => {
    let data = "";
    const onData = chunk => {
      data += chunk.toString("utf8");
      const lines = data.split(/\r?\n/).filter(Boolean);
      const last = lines[lines.length - 1];
      if (last && /^\d{3} /.test(last)) { cleanup(); resolve({ code: parseInt(last.slice(0, 3), 10), text: data.trim() }); }
    };
    const onErr = err => { cleanup(); reject(err); };
    const onTimeout = () => { cleanup(); reject(new Error("SMTP read timeout")); };
    const cleanup = () => { socket.off("data", onData); socket.off("error", onErr); socket.off("timeout", onTimeout); };
    socket.on("data", onData); socket.once("error", onErr); socket.once("timeout", onTimeout);
  });
}

async function expect(socket, codes, label, timeoutMs) {
  const reply = await readReply(socket, timeoutMs);
  if (!codes.includes(reply.code)) throw new Error(`SMTP ${label}: expected ${codes.join("/")}, got ${reply.code} — ${reply.text}`);
  return reply;
}

// Authenticated SMTP exchange over a ready (TLS) socket. alreadyGreeted=true when the
// 220 banner was consumed before STARTTLS (port 587 path).
async function runSmtpSession(socket, s, subject, body, alreadyGreeted, timeoutMs) {
  const write = line => socket.write(line + "\r\n");
  if (!alreadyGreeted) await expect(socket, [220], "greeting", timeoutMs);
  write("EHLO blackouthunt-feedback"); await expect(socket, [250], "EHLO", timeoutMs);
  write("AUTH LOGIN"); await expect(socket, [334], "AUTH", timeoutMs);
  write(Buffer.from(s.user, "utf8").toString("base64")); await expect(socket, [334], "AUTH user", timeoutMs);
  write(Buffer.from(s.pass, "utf8").toString("base64")); await expect(socket, [235], "AUTH pass", timeoutMs);
  write(`MAIL FROM:<${s.user}>`); await expect(socket, [250], "MAIL FROM", timeoutMs);
  write(`RCPT TO:<${s.to}>`); await expect(socket, [250, 251], "RCPT TO", timeoutMs);
  write("DATA"); await expect(socket, [354], "DATA", timeoutMs);
  const headers = [
    `From: Blackout Hunt Feedback <${s.from}>`,
    `To: <${s.to}>`,
    `Subject: ${encodeSubject(subject)}`,
    `Date: ${new Date().toUTCString()}`,
    `MIME-Version: 1.0`,
    `Content-Type: text/plain; charset=utf-8`,
    `Content-Transfer-Encoding: base64`,
  ].join("\r\n");
  const payload = headers + "\r\n\r\n" + wrapBase64(Buffer.from(body, "utf8").toString("base64")) + "\r\n.\r\n";
  socket.write(payload);
  await expect(socket, [250], "message body", timeoutMs);
  write("QUIT");
  socket.end();
}

// Send one message over a fresh connection. Port 465 = implicit TLS; any other port
// (e.g. 587) = plaintext connect then STARTTLS upgrade.
function smtpSend(s, subject, body) {
  const timeoutMs = 20000;
  return new Promise((resolve, reject) => {
    if (Number(s.port) === 465) {
      const socket = tls.connect({ host: s.host, port: s.port, servername: s.host });
      socket.setTimeout(timeoutMs, () => socket.destroy(new Error("SMTP timeout")));
      socket.once("error", reject);
      socket.once("secureConnect", () =>
        runSmtpSession(socket, s, subject, body, false, timeoutMs).then(resolve, e => { socket.destroy(); reject(e); }));
      return;
    }
    const plain = net.connect({ host: s.host, port: s.port });
    plain.setTimeout(timeoutMs, () => plain.destroy(new Error("SMTP timeout")));
    plain.once("error", reject);
    plain.once("connect", async () => {
      try {
        await expect(plain, [220], "greeting", timeoutMs);
        plain.write("EHLO blackouthunt-feedback\r\n"); await expect(plain, [250], "EHLO", timeoutMs);
        plain.write("STARTTLS\r\n"); await expect(plain, [220], "STARTTLS", timeoutMs);
        const secure = tls.connect({ socket: plain, servername: s.host });
        secure.setTimeout(timeoutMs, () => secure.destroy(new Error("SMTP TLS timeout")));
        secure.once("error", reject);
        secure.once("secureConnect", () =>
          runSmtpSession(secure, s, subject, body, true, timeoutMs).then(resolve, e => { secure.destroy(); reject(e); }));
      } catch (err) { plain.destroy(); reject(err); }
    });
  });
}

// Serialize emails through one chain so a burst (e.g. a class submitting at once) does not
// open many parallel SMTP connections and trip provider connection limits.
let emailChain = Promise.resolve();
let warnedDisabled = false;

export function queueFeedbackEmail(record) {
  const s = emailSettings();
  if (!emailConfigured()) {
    if (!warnedDisabled) { console.log("feedback email: disabled (set SMTP_PASS + FEEDBACK_EMAIL_TO in .env to enable)"); warnedDisabled = true; }
    return;
  }
  if (!s.kinds.includes(record.kind)) return;
  const { subject, body } = composeMessage(record);
  emailChain = emailChain
    .then(() => smtpSend(s, subject, body))
    .then(() => console.log(`feedback email sent (${record.kind}) -> ${s.to}`))
    .catch(err => console.error("feedback email failed:", err.message));
}

// ---- CLI ----
function loadDotEnvForCli() {
  const envPath = join(dirname(fileURLToPath(import.meta.url)), ".env");
  if (!existsSync(envPath)) return;
  for (const raw of readFileSync(envPath, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const i = line.indexOf("=");
    if (i <= 0) continue;
    const k = line.slice(0, i).trim();
    if (process.env[k] === undefined) process.env[k] = line.slice(i + 1).trim().replace(/^["']|["']$/g, "");
  }
}

const isCliRun = Boolean(process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("email-notify.mjs"));
if (isCliRun) {
  loadDotEnvForCli();
  const sample = {
    kind: "bug", rating: 4, contact: "student@example.com",
    message: "Flashlight flickers near the train station breaker. Also unicode: café señor.",
    context: { app_version: "0.6.0", platform: "Windows", level: "TrainStation", role: "Hider", player_name: "Ada", play_seconds: 312 },
    diagnostics: { perf: { avg_fps: 58, min_fps: 31, p1_low_fps: 34, hitches: 3, session_seconds: 312 }, device: { cpu: "Ryzen 5", gpu: "GTX 1650", ram_gb: 16, os: "Windows 11" } },
    received_at: new Date().toISOString(),
  };
  const { subject, body } = composeMessage(sample);
  if (process.argv.includes("--dry")) {
    const s = emailSettings();
    console.log("Subject:", subject);
    console.log("----\n" + body);
    console.log(`\n[dry run] configured=${emailConfigured()} (host=${s.host} port=${s.port} user=${s.user ? "set" : "EMPTY"} pass=${s.pass ? `set(len ${s.pass.length})` : "EMPTY"} to=${s.to || "EMPTY"})`);
  } else if (process.argv.includes("--test")) {
    if (!emailConfigured()) { console.error("Not configured. Set FEEDBACK_EMAIL_TO + SMTP_PASS in .env first."); process.exit(1); }
    const s = emailSettings();
    smtpSend(s, "[Blackout Hunt] test email — feedback backup is working", "This is a test from email-notify.mjs.\nIf you got this, the direct email backup is configured correctly.\n\n— Blackout Hunt feedback backend")
      .then(() => { console.log(`Test email sent to ${s.to}. Check your inbox (and spam).`); process.exit(0); })
      .catch(err => { console.error("Test email FAILED:", err.message); process.exit(1); });
  } else {
    console.log("Usage: node email-notify.mjs --dry | --test");
  }
}
