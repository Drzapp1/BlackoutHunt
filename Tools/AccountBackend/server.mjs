import { createServer } from "node:http";
import { createHash, createHmac, randomBytes, timingSafeEqual } from "node:crypto";
import { appendFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, URL, URLSearchParams } from "node:url";
import { queueFeedbackEmail, emailConfigured } from "./email-notify.mjs";

const backendDir = dirname(fileURLToPath(import.meta.url));
const dotEnvPath = join(backendDir, ".env");

function parseDotEnvValue(value) {
  const trimmed = value.trim();
  if (trimmed.length >= 2 && trimmed.startsWith("\"") && trimmed.endsWith("\"")) {
    return trimmed.slice(1, -1)
      .replace(/\\n/g, "\n")
      .replace(/\\r/g, "\r")
      .replace(/\\t/g, "\t")
      .replace(/\\"/g, "\"")
      .replace(/\\\\/g, "\\");
  }

  if (trimmed.length >= 2 && trimmed.startsWith("'") && trimmed.endsWith("'")) {
    return trimmed.slice(1, -1);
  }

  return trimmed;
}

function loadDotEnv(filePath) {
  if (!existsSync(filePath)) {
    return;
  }

  for (const rawLine of readFileSync(filePath, "utf8").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#")) {
      continue;
    }

    const normalized = line.startsWith("export ") ? line.slice("export ".length).trim() : line;
    const separator = normalized.indexOf("=");
    if (separator <= 0) {
      continue;
    }

    const key = normalized.slice(0, separator).trim();
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(key) || process.env[key] !== undefined) {
      continue;
    }

    process.env[key] = parseDotEnvValue(normalized.slice(separator + 1));
  }
}

function readDotEnvValues(filePath) {
  const values = new Map();
  if (!existsSync(filePath)) {
    return values;
  }

  for (const rawLine of readFileSync(filePath, "utf8").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#")) {
      continue;
    }

    const normalized = line.startsWith("export ") ? line.slice("export ".length).trim() : line;
    const separator = normalized.indexOf("=");
    if (separator <= 0) {
      continue;
    }

    const key = normalized.slice(0, separator).trim();
    if (/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(key)) {
      values.set(key, parseDotEnvValue(normalized.slice(separator + 1)));
    }
  }

  return values;
}

async function writeDotEnvValues(filePath, updates) {
  const current = readDotEnvValues(filePath);
  for (const [key, value] of Object.entries(updates)) {
    current.set(key, value);
  }

  const orderedKeys = [
    "PORT",
    "PUBLIC_BASE_URL",
    "STATE_SECRET",
    "GOOGLE_CLIENT_ID",
    "GOOGLE_CLIENT_SECRET",
    "MICROSOFT_CLIENT_ID",
    "MICROSOFT_CLIENT_SECRET",
  ];
  const lines = [];

  for (const key of orderedKeys) {
    if (current.has(key)) {
      lines.push(`${key}=${current.get(key) || ""}`);
    }
  }

  for (const [key, value] of current.entries()) {
    if (!orderedKeys.includes(key)) {
      lines.push(`${key}=${value || ""}`);
    }
  }

  await writeFile(filePath, `${lines.join("\n")}\n`, { mode: 0o600 });
}

loadDotEnv(dotEnvPath);

const port = Number(process.env.PORT || 8787);
const publicBaseUrl = (process.env.PUBLIC_BASE_URL || `http://127.0.0.1:${port}`).replace(/\/+$/, "");
const dataDir = process.env.DATA_DIR || join(backendDir, "data");
const playersPath = join(dataDir, "players.json");
const stateSecretPath = join(dataDir, "state-secret.txt");
const adminTokenPath = join(dataDir, "admin-token.txt");
const feedbackPath = join(dataDir, "feedback.jsonl");
const telemetryPath = join(dataDir, "telemetry.jsonl");
const loginStateTtlMs = 15 * 60 * 1000;

// Bind to loopback by default. Set BIND_HOST=0.0.0.0 to accept feedback from other
// machines on a LAN (classroom), or front the loopback port with the Playit tunnel so
// remote students can reach it without exposing the host directly. See ONLINE_SERVICES.md.
const bindHost = process.env.BIND_HOST || "127.0.0.1";

// Ingest guards. Bodies are already capped at 1 MB by readBody().
const maxFeedbackChars = 8000;
const maxContactChars = 200;
const maxLogLines = 500;
const maxLogLineChars = 1000;
const maxStoredEntries = 5000; // newest-N kept in memory for the dashboard; the .jsonl file keeps everything.
const feedbackKinds = new Set(["bug", "idea", "praise", "other", "survey"]);

// Per-IP ingest rate limits (fixed window). Behind a tunnel/reverse proxy the whole
// audience can share one apparent IP (a classroom NAT, or Caddy->loopback when Playit
// does not pass the real client IP through), so the feedback cap is set high enough for
// a full class submitting end-of-round surveys at once. Tune via env. clampInt is hoisted.
const feedbackRateLimit = clampInt(process.env.FEEDBACK_RATE_LIMIT, 1, 100000, 300);
const telemetryRateLimit = clampInt(process.env.TELEMETRY_RATE_LIMIT, 1, 100000, 600);
const rateLimitWindowMs = clampInt(process.env.RATE_LIMIT_WINDOW_MS, 1000, 24 * 60 * 60 * 1000, 5 * 60 * 1000);

const pendingStates = new Map();
const deviceAuth = new Map();
const sessions = new Map();
const rateBuckets = new Map();
let players = {};
let stateSecret = "";
let adminToken = "";
let writeChain = Promise.resolve();

function json(res, status, body) {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store",
  });
  res.end(payload);
}

function html(res, status, body) {
  res.writeHead(status, {
    "content-type": "text/html; charset=utf-8",
    "cache-control": "no-store",
  });
  res.end(body);
}

function redirect(res, location) {
  res.writeHead(302, { location, "cache-control": "no-store" });
  res.end();
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    let bytes = 0;
    req.on("data", chunk => {
      // chunk is a Buffer (no encoding set), so chunk.length is bytes, not UTF-16 units.
      bytes += chunk.length;
      if (bytes > 1024 * 1024) {
        reject(new Error("request body too large"));
        req.destroy();
        return;
      }
      body += chunk;
    });
    req.on("end", () => resolve(body));
    req.on("error", reject);
  });
}

async function loadPlayers() {
  if (!existsSync(playersPath)) {
    players = {};
    return;
  }

  players = JSON.parse(await readFile(playersPath, "utf8"));
}

async function savePlayers() {
  await mkdir(dataDir, { recursive: true });
  await writeFile(playersPath, JSON.stringify(players, null, 2));
}

async function loadStateSecret() {
  if (process.env.STATE_SECRET) {
    stateSecret = process.env.STATE_SECRET;
    return;
  }

  if (existsSync(stateSecretPath)) {
    stateSecret = (await readFile(stateSecretPath, "utf8")).trim();
    if (stateSecret) {
      return;
    }
  }

  stateSecret = randomBytes(32).toString("base64url");
  await writeFile(stateSecretPath, `${stateSecret}\n`, { mode: 0o600 });
}

async function loadAdminToken() {
  if (process.env.ADMIN_TOKEN) {
    adminToken = process.env.ADMIN_TOKEN;
    return;
  }

  if (existsSync(adminTokenPath)) {
    adminToken = (await readFile(adminTokenPath, "utf8")).trim();
    if (adminToken) {
      return;
    }
  }

  adminToken = randomBytes(24).toString("base64url");
  await writeFile(adminTokenPath, `${adminToken}\n`, { mode: 0o600 });
}

// Append-only JSONL writes, serialized through a single promise chain so two
// concurrent requests cannot interleave partial lines.
function appendJsonLine(path, record) {
  writeChain = writeChain
    .then(() => appendFile(path, `${JSON.stringify(record)}\n`))
    .catch(error => console.error(`append ${path} failed:`, error.message));
  return writeChain;
}

async function readJsonLines(path, limit) {
  if (!existsSync(path)) {
    return [];
  }

  const text = await readFile(path, "utf8");
  const out = [];
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) {
      continue;
    }
    try {
      out.push(JSON.parse(line));
    } catch {
      // Skip a corrupt line rather than failing the whole view.
    }
  }

  return limit && out.length > limit ? out.slice(out.length - limit) : out;
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[<>&"']/g, c => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    "\"": "&quot;",
    "'": "&#39;",
  }[c]));
}

// Strip control characters (keeping tab and newline) and clamp length.
const controlCharsRegex = /[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/g;
function cleanText(value, maxChars) {
  const text = String(value ?? "").replace(controlCharsRegex, "").trim();
  return text.length > maxChars ? text.slice(0, maxChars) : text;
}

function clampInt(value, min, max, fallback) {
  const n = Number(value);
  if (!Number.isFinite(n)) {
    return fallback;
  }
  return Math.min(max, Math.max(min, Math.round(n)));
}

function clientIp(req) {
  const forwarded = String(req.headers["x-forwarded-for"] || "").split(",")[0].trim();
  return forwarded || req.socket?.remoteAddress || "unknown";
}

// Simple fixed-window per-IP limiter. Keeps abusive clients from filling the log.
function rateLimited(req, bucket, limit, windowMs) {
  const key = `${bucket}:${clientIp(req)}`;
  const now = Date.now();
  const entry = rateBuckets.get(key);
  if (!entry || now - entry.start >= windowMs) {
    // Opportunistically drop expired buckets so a host on 0.0.0.0 (many client IPs) or
    // behind a tunnel does not accumulate entries without bound.
    if (rateBuckets.size > 1000) {
      for (const [existingKey, existingEntry] of rateBuckets) {
        if (now - existingEntry.start >= windowMs) {
          rateBuckets.delete(existingKey);
        }
      }
    }
    rateBuckets.set(key, { start: now, count: 1 });
    return false;
  }
  entry.count += 1;
  return entry.count > limit;
}

function adminAuthorized(req, url) {
  if (!adminToken) {
    return false;
  }
  const header = req.headers.authorization || "";
  const bearer = /^Bearer\s+(.+)$/i.exec(header);
  const provided = (bearer && bearer[1]) || url.searchParams.get("key") || "";
  return Boolean(provided) && constantTimeEquals(provided, adminToken);
}

function sanitizeDiagnostics(raw) {
  if (!raw || typeof raw !== "object") {
    return undefined;
  }

  const out = {};
  if (raw.device && typeof raw.device === "object") {
    out.device = {
      cpu: cleanText(raw.device.cpu, 200),
      gpu: cleanText(raw.device.gpu, 200),
      os: cleanText(raw.device.os, 200),
      ram_gb: clampInt(raw.device.ram_gb, 0, 4096, 0),
    };
  }
  if (raw.perf && typeof raw.perf === "object") {
    const p = raw.perf;
    out.perf = {
      samples: clampInt(p.samples, 0, 1e9, 0),
      session_seconds: clampInt(p.session_seconds, 0, 1e9, 0),
      avg_fps: clampInt(p.avg_fps, 0, 100000, 0),
      min_fps: clampInt(p.min_fps, 0, 100000, 0),
      max_fps: clampInt(p.max_fps, 0, 100000, 0),
      p1_low_fps: clampInt(p.p1_low_fps, 0, 100000, 0),
      hitches: clampInt(p.hitches, 0, 1e9, 0),
    };
  }
  if (Array.isArray(raw.recent_logs)) {
    out.recent_logs = raw.recent_logs
      .slice(0, maxLogLines)
      .map(line => cleanText(line, maxLogLineChars))
      .filter(Boolean);
  }
  return out;
}

function sanitizeContext(raw) {
  if (!raw || typeof raw !== "object") {
    return {};
  }
  return {
    app_version: cleanText(raw.app_version, 64),
    engine_version: cleanText(raw.engine_version, 64),
    platform: cleanText(raw.platform, 64),
    session_id: cleanText(raw.session_id, 128),
    player_name: cleanText(raw.player_name, 80),
    player_id: cleanText(raw.player_id, 128),
    level: cleanText(raw.level, 64),
    role: cleanText(raw.role, 32),
    round_phase: cleanText(raw.round_phase, 32),
    play_seconds: clampInt(raw.play_seconds, 0, 1e9, 0),
  };
}

function requireEnv(provider, name) {
  const value = process.env[name];
  if (!value) {
    throw new Error(`${provider} is not configured: missing ${name}`);
  }
  return value;
}

function providerEnvNames(provider) {
  if (provider === "google") {
    return ["GOOGLE_CLIENT_ID", "GOOGLE_CLIENT_SECRET"];
  }

  if (provider === "microsoft") {
    return ["MICROSOFT_CLIENT_ID", "MICROSOFT_CLIENT_SECRET"];
  }

  return [];
}

function isProviderConfigured(provider) {
  return providerEnvNames(provider).every(name => Boolean(process.env[name]));
}

function providerConfig(provider) {
  if (provider === "google") {
    return {
      clientId: requireEnv("Google", "GOOGLE_CLIENT_ID"),
      clientSecret: requireEnv("Google", "GOOGLE_CLIENT_SECRET"),
      authorizeUrl: "https://accounts.google.com/o/oauth2/v2/auth",
      tokenUrl: "https://oauth2.googleapis.com/token",
      userInfoUrl: "https://openidconnect.googleapis.com/v1/userinfo",
      scope: "openid email profile",
      redirectUri: `${publicBaseUrl}/auth/google/callback`,
    };
  }

  if (provider === "microsoft") {
    return {
      clientId: requireEnv("Microsoft", "MICROSOFT_CLIENT_ID"),
      clientSecret: requireEnv("Microsoft", "MICROSOFT_CLIENT_SECRET"),
      authorizeUrl: "https://login.microsoftonline.com/common/oauth2/v2.0/authorize",
      tokenUrl: "https://login.microsoftonline.com/common/oauth2/v2.0/token",
      userInfoUrl: "https://graph.microsoft.com/oidc/userinfo",
      scope: "openid profile email User.Read",
      redirectUri: `${publicBaseUrl}/auth/microsoft/callback`,
    };
  }

  throw new Error(`unknown provider: ${provider}`);
}

function playerIdFor(provider, subject) {
  return createHash("sha256").update(`${provider}:${subject}`).digest("hex");
}

function newToken() {
  return randomBytes(32).toString("base64url");
}

function hmacBase64Url(value) {
  return createHmac("sha256", stateSecret).update(value).digest("base64url");
}

function constantTimeEquals(left, right) {
  const leftBuffer = Buffer.from(left);
  const rightBuffer = Buffer.from(right);
  return leftBuffer.length === rightBuffer.length && timingSafeEqual(leftBuffer, rightBuffer);
}

function createLoginState(provider, deviceId) {
  const payload = Buffer.from(JSON.stringify({
    provider,
    device_id: deviceId,
    nonce: randomBytes(12).toString("base64url"),
    created_at: Date.now(),
  }), "utf8").toString("base64url");

  return `${payload}.${hmacBase64Url(payload)}`;
}

function readLoginState(state) {
  const [payload, signature, ...extra] = String(state || "").split(".");
  if (!payload || !signature || extra.length || !stateSecret) {
    return null;
  }

  if (!constantTimeEquals(signature, hmacBase64Url(payload))) {
    return null;
  }

  let parsed;
  try {
    parsed = JSON.parse(Buffer.from(payload, "base64url").toString("utf8"));
  } catch {
    return null;
  }

  const ageMs = Date.now() - Number(parsed.created_at || 0);
  if (ageMs < 0 || ageMs > loginStateTtlMs) {
    return null;
  }

  const provider = parsed.provider;
  const deviceId = parsed.device_id;
  if (!/^(google|microsoft)$/.test(provider) || !/^[a-zA-Z0-9_-]{8,128}$/.test(deviceId || "")) {
    return null;
  }

  return { provider, deviceId, createdAt: parsed.created_at };
}

function successPage(displayName) {
  const escapedName = String(displayName || "Player").replace(/[<>&"]/g, c => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    "\"": "&quot;",
  }[c]));

  return `<!doctype html>
<meta charset="utf-8">
<title>Blackout Hunt Login</title>
<body style="font-family: system-ui, sans-serif; background:#101417; color:#e9f2ef; padding:32px">
  <h1>Signed in</h1>
  <p>You are signed in as ${escapedName}. Return to Blackout Hunt.</p>
</body>`;
}

function setupPage(provider, message) {
  const escapedProvider = String(provider || "provider").replace(/[<>&"]/g, c => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    "\"": "&quot;",
  }[c]));
  const escapedMessage = String(message || "Provider is not configured.").replace(/[<>&"]/g, c => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    "\"": "&quot;",
  }[c]));

  return `<!doctype html>
<meta charset="utf-8">
<title>Blackout Hunt Login Setup</title>
<body style="font-family: system-ui, sans-serif; background:#101417; color:#e9f2ef; padding:32px; line-height:1.45">
  <h1>${escapedProvider} login is not configured</h1>
  <p>${escapedMessage}</p>
  <p>Add the provider credentials to <code>Tools\\AccountBackend\\.env</code>, restart <code>node .\\server.mjs</code>, then try again.</p>
</body>`;
}

function googleSetupPage(message = "") {
  const redirectUri = `${publicBaseUrl}/auth/google/callback`;
  const escapedMessage = String(message || "").replace(/[<>&"]/g, c => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    "\"": "&quot;",
  }[c]));

  return `<!doctype html>
<meta charset="utf-8">
<title>Blackout Hunt Google Setup</title>
<body style="font-family: system-ui, sans-serif; background:#101417; color:#e9f2ef; padding:32px; line-height:1.45; max-width:760px">
  <h1>Google login setup</h1>
  ${escapedMessage ? `<p style="color:#ffb3a7">${escapedMessage}</p>` : ""}
  <ol>
    <li>Open <a style="color:#9bd7ff" href="https://console.cloud.google.com/apis/credentials" target="_blank">Google Cloud Credentials</a>.</li>
    <li>Create an OAuth client with type <strong>Web application</strong>.</li>
    <li>Add this authorized redirect URI: <code>${redirectUri}</code></li>
    <li>Copy the generated <strong>Client ID</strong> and <strong>Client secret</strong> into this local form.</li>
  </ol>
  <form method="post" action="/setup/google" style="display:grid; gap:12px; margin-top:24px">
    <label>
      Client ID
      <input name="client_id" autocomplete="off" required style="display:block; width:100%; box-sizing:border-box; padding:10px; margin-top:4px">
    </label>
    <label>
      Client secret
      <input name="client_secret" autocomplete="off" required type="password" style="display:block; width:100%; box-sizing:border-box; padding:10px; margin-top:4px">
    </label>
    <button style="padding:10px 14px; width:max-content">Save Google credentials</button>
  </form>
</body>`;
}

function setupSavedPage() {
  return `<!doctype html>
<meta charset="utf-8">
<title>Blackout Hunt Google Setup</title>
<body style="font-family: system-ui, sans-serif; background:#101417; color:#e9f2ef; padding:32px; line-height:1.45">
  <h1>Google login configured</h1>
  <p>The backend saved the credentials and activated them for this running process.</p>
  <p>Return to Blackout Hunt and press Google again.</p>
</body>`;
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const text = await response.text();
  let body = {};
  if (text) {
    body = JSON.parse(text);
  }
  if (!response.ok) {
    throw new Error(body.error_description || body.error || `HTTP ${response.status}`);
  }
  return body;
}

async function exchangeCode(provider, code) {
  const config = providerConfig(provider);
  const tokenBody = new URLSearchParams({
    client_id: config.clientId,
    client_secret: config.clientSecret,
    code,
    grant_type: "authorization_code",
    redirect_uri: config.redirectUri,
  });

  const tokenJson = await fetchJson(config.tokenUrl, {
    method: "POST",
    headers: { "content-type": "application/x-www-form-urlencoded" },
    body: tokenBody,
  });

  if (!tokenJson.access_token) {
    throw new Error("provider token response did not include an access token");
  }

  const userInfo = await fetchJson(config.userInfoUrl, {
    headers: { authorization: `Bearer ${tokenJson.access_token}` },
  });

  const subject = userInfo.sub || userInfo.id;
  if (!subject) {
    throw new Error("provider userinfo response did not include a stable subject");
  }

  return {
    provider,
    provider_subject: subject,
    email: userInfo.email || userInfo.preferred_username || "",
    display_name: userInfo.name || userInfo.displayName || userInfo.given_name || "Player",
    avatar_url: userInfo.picture || "",
  };
}

function authPlayer(req) {
  const header = req.headers.authorization || "";
  const match = /^Bearer\s+(.+)$/i.exec(header);
  if (!match) {
    return null;
  }

  const playerId = sessions.get(match[1]);
  return playerId ? players[playerId] : null;
}

async function handleGoogleSetup(req, res) {
  const form = new URLSearchParams(await readBody(req));
  const clientId = String(form.get("client_id") || "").trim();
  const clientSecret = String(form.get("client_secret") || "").trim();

  if (!clientId || !clientSecret) {
    html(res, 400, googleSetupPage("Client ID and client secret are both required."));
    return;
  }

  if (/^(web\s*application|desktop\s*app|android|ios)$/i.test(clientId)) {
    html(res, 400, googleSetupPage("That is the OAuth client type. Paste the generated Client ID instead."));
    return;
  }

  if (!clientId.endsWith(".apps.googleusercontent.com")) {
    html(res, 400, googleSetupPage("That does not look like a Google OAuth Client ID. It usually ends with .apps.googleusercontent.com."));
    return;
  }

  await writeDotEnvValues(dotEnvPath, {
    PORT: String(port),
    PUBLIC_BASE_URL: publicBaseUrl,
    GOOGLE_CLIENT_ID: clientId,
    GOOGLE_CLIENT_SECRET: clientSecret,
  });

  process.env.GOOGLE_CLIENT_ID = clientId;
  process.env.GOOGLE_CLIENT_SECRET = clientSecret;
  process.env.PUBLIC_BASE_URL = publicBaseUrl;
  process.env.PORT = String(port);

  html(res, 200, setupSavedPage());
}

async function handleAuthStart(provider, url, res) {
  const deviceId = url.searchParams.get("device_id");
  if (!deviceId || !/^[a-zA-Z0-9_-]{8,128}$/.test(deviceId)) {
    json(res, 400, { status: "failed", error: "invalid device_id" });
    return;
  }

  let config;
  try {
    config = providerConfig(provider);
  } catch (error) {
    deviceAuth.set(deviceId, { status: "failed", provider, error: error.message });
    if (provider === "google") {
      html(res, 500, googleSetupPage(error.message));
      return;
    }

    html(res, 500, setupPage(provider, error.message));
    return;
  }

  const state = createLoginState(provider, deviceId);
  pendingStates.set(state, { provider, deviceId, createdAt: Date.now() });
  deviceAuth.set(deviceId, { status: "pending", provider });

  const authUrl = new URL(config.authorizeUrl);
  authUrl.searchParams.set("client_id", config.clientId);
  authUrl.searchParams.set("redirect_uri", config.redirectUri);
  authUrl.searchParams.set("response_type", "code");
  authUrl.searchParams.set("scope", config.scope);
  authUrl.searchParams.set("state", state);
  authUrl.searchParams.set("prompt", "select_account");
  redirect(res, authUrl.toString());
}

async function handleAuthCallback(provider, url, res) {
  const state = url.searchParams.get("state");
  const code = url.searchParams.get("code");
  const pending = state ? (pendingStates.get(state) || readLoginState(state)) : null;
  if (!state || !code || !pending || pending.provider !== provider) {
    html(res, 400, "<h1>Login failed</h1><p>Invalid or expired login state.</p>");
    return;
  }

  pendingStates.delete(state);

  try {
    const existingAuth = deviceAuth.get(pending.deviceId);
    if (existingAuth?.status === "authorized") {
      html(res, 200, successPage(existingAuth.player?.display_name));
      return;
    }

    const providerProfile = await exchangeCode(provider, code);
    const playerId = playerIdFor(provider, providerProfile.provider_subject);
    const existing = players[playerId] || {};
    const player = {
      player_id: playerId,
      provider,
      provider_subject: providerProfile.provider_subject,
      email: providerProfile.email,
      display_name: providerProfile.display_name,
      avatar_url: providerProfile.avatar_url,
      progress: existing.progress || {},
      updated_at: new Date().toISOString(),
    };

    players[playerId] = player;
    await savePlayers();

    const sessionToken = newToken();
    sessions.set(sessionToken, playerId);
    deviceAuth.set(pending.deviceId, {
      status: "authorized",
      session_token: sessionToken,
      player,
    });

    html(res, 200, successPage(player.display_name));
  } catch (error) {
    deviceAuth.set(pending.deviceId, { status: "failed", error: error.message });
    html(res, 500, `<h1>Login failed</h1><p>${String(error.message)}</p>`);
  }
}

async function handleFeedback(req, res) {
  if (rateLimited(req, "feedback", feedbackRateLimit, rateLimitWindowMs)) {
    json(res, 429, { ok: false, error: "too many submissions, try again in a few minutes" });
    return;
  }

  let body;
  try {
    body = JSON.parse((await readBody(req)) || "{}");
  } catch {
    json(res, 400, { ok: false, error: "invalid JSON body" });
    return;
  }

  const kind = feedbackKinds.has(body.kind) ? body.kind : "other";
  const message = cleanText(body.message, maxFeedbackChars);
  const contact = cleanText(body.contact, maxContactChars);
  const rating = clampInt(body.rating, 0, 5, 0);

  // A survey can carry its content in the structured `survey` block, so it may have no free-text
  // message; every other kind must say something.
  if (!message && kind !== "survey") {
    json(res, 400, { ok: false, error: "message is required" });
    return;
  }

  const record = {
    id: randomBytes(8).toString("hex"),
    type: "feedback",
    kind,
    message,
    rating,
    contact,
    context: sanitizeContext(body.context),
    received_at: new Date().toISOString(),
    ip: clientIp(req),
  };

  if (kind === "survey" && body.survey && typeof body.survey === "object") {
    record.survey = {
      overall: clampInt(body.survey.overall, 0, 5, 0),
      difficulty: cleanText(body.survey.difficulty, 32),
      would_recommend: Boolean(body.survey.would_recommend),
      round_role: cleanText(body.survey.round_role, 32),
      round_result: cleanText(body.survey.round_result, 32),
      survived: Boolean(body.survey.survived),
    };
  }

  const diagnostics = sanitizeDiagnostics(body.diagnostics);
  if (diagnostics) {
    record.diagnostics = diagnostics;
  }

  await appendJsonLine(feedbackPath, record);
  json(res, 200, { ok: true, id: record.id });
  // Direct email backup (fire-and-forget): a durable copy of each submission reaches your
  // inbox even if the dashboard host later goes down. No-op unless SMTP is configured.
  queueFeedbackEmail(record);
}

async function handleTelemetrySession(req, res) {
  if (rateLimited(req, "telemetry", telemetryRateLimit, rateLimitWindowMs)) {
    json(res, 429, { ok: false, error: "too many telemetry posts" });
    return;
  }

  let body;
  try {
    body = JSON.parse((await readBody(req)) || "{}");
  } catch {
    json(res, 400, { ok: false, error: "invalid JSON body" });
    return;
  }

  const record = {
    id: randomBytes(8).toString("hex"),
    type: "telemetry",
    reason: cleanText(body.reason, 32) || "session_end",
    context: sanitizeContext({
      app_version: body.app_version,
      engine_version: body.engine_version,
      platform: body.platform,
      session_id: body.session_id,
      player_name: body.player_name,
      player_id: body.player_id,
    }),
    received_at: new Date().toISOString(),
    ip: clientIp(req),
  };

  const diagnostics = sanitizeDiagnostics({
    device: body.device,
    perf: body.perf,
    recent_logs: body.recent_logs,
  });
  if (diagnostics) {
    record.diagnostics = diagnostics;
  }

  if (Array.isArray(body.round_results)) {
    record.round_results = body.round_results.slice(0, 100).map(r => ({
      role: cleanText(r && r.role, 32),
      result: cleanText(r && r.result, 32),
      survived: Boolean(r && r.survived),
      level: cleanText(r && r.level, 64),
      at: cleanText(r && r.at, 40),
    }));
  }

  await appendJsonLine(telemetryPath, record);
  json(res, 200, { ok: true, id: record.id });
}

function formatContextLine(ctx = {}) {
  const bits = [];
  if (ctx.player_name) bits.push(ctx.player_name);
  if (ctx.role) bits.push(ctx.role);
  if (ctx.level) bits.push(ctx.level);
  if (ctx.round_phase) bits.push(ctx.round_phase);
  if (ctx.app_version) bits.push(`v${ctx.app_version}`);
  if (ctx.platform) bits.push(ctx.platform);
  if (ctx.play_seconds) bits.push(`${ctx.play_seconds}s played`);
  return bits.map(escapeHtml).join(" &middot; ");
}

function renderDiagnostics(d) {
  if (!d) {
    return "";
  }
  const perf = d.perf
    ? `FPS avg ${d.perf.avg_fps} / min ${d.perf.min_fps} / 1%low ${d.perf.p1_low_fps} &middot; ${d.perf.hitches} hitches &middot; ${d.perf.session_seconds}s`
    : "";
  const dev = d.device
    ? `${escapeHtml(d.device.cpu || "?")} &middot; ${escapeHtml(d.device.gpu || "?")} &middot; ${d.device.ram_gb}GB &middot; ${escapeHtml(d.device.os || "?")}`
    : "";
  const logs = Array.isArray(d.recent_logs) && d.recent_logs.length
    ? `<details><summary>${d.recent_logs.length} recent log lines</summary><pre>${d.recent_logs.map(escapeHtml).join("\n")}</pre></details>`
    : "";
  return `<div class="diag">${perf}${dev ? `<br>${dev}` : ""}${logs}</div>`;
}

function renderFeedbackCard(e) {
  const stars = e.rating ? ` <span class="stars">${"&#9733;".repeat(e.rating)}${"&#9734;".repeat(5 - e.rating)}</span>` : "";
  let surveyHtml = "";
  if (e.survey) {
    const s = e.survey;
    surveyHtml = `<div class="survey">Overall ${s.overall || "-"}/5 &middot; difficulty ${escapeHtml(s.difficulty || "-")} &middot; ${s.would_recommend ? "would recommend" : "would not recommend"}${s.round_role ? ` &middot; ${escapeHtml(s.round_role)}` : ""}${s.round_result ? ` ${escapeHtml(s.round_result)}` : ""}${s.survived ? " (survived)" : ""}</div>`;
  }
  return `<article class="card kind-${escapeHtml(e.kind)}">
    <header><span class="kind">${escapeHtml(e.kind)}</span>${stars}<time>${escapeHtml(e.received_at)}</time></header>
    <div class="ctx">${formatContextLine(e.context)}</div>
    ${e.message ? `<p class="msg">${escapeHtml(e.message)}</p>` : ""}
    ${surveyHtml}
    ${e.contact ? `<div class="contact">Contact: ${escapeHtml(e.contact)}</div>` : ""}
    ${renderDiagnostics(e.diagnostics)}
  </article>`;
}

function renderTelemetryCard(e) {
  const rounds = Array.isArray(e.round_results) && e.round_results.length
    ? `<div class="rounds">${e.round_results.map(r => escapeHtml(`${r.level || "?"}: ${r.role || "?"} → ${r.result || "?"}${r.survived ? " (survived)" : ""}`)).join("<br>")}</div>`
    : "";
  return `<article class="card telemetry">
    <header><span class="kind">telemetry</span><span class="reason">${escapeHtml(e.reason)}</span><time>${escapeHtml(e.received_at)}</time></header>
    <div class="ctx">${formatContextLine(e.context)}</div>
    ${rounds}
    ${renderDiagnostics(e.diagnostics)}
  </article>`;
}

function adminUnauthorizedPage() {
  return `<!doctype html>
<meta charset="utf-8">
<title>Blackout Hunt feedback</title>
<body style="font-family: system-ui, sans-serif; background:#0d1114; color:#e9f2ef; padding:32px; line-height:1.5">
  <h1>Feedback dashboard is locked</h1>
  <p>Open this page with the admin key, e.g. <code>/admin?key=YOUR_TOKEN</code>.</p>
  <p>The key is printed in the backend console at startup and stored in <code>Tools/AccountBackend/data/admin-token.txt</code>. You can also set <code>ADMIN_TOKEN</code> in <code>.env</code>.</p>
</body>`;
}

function adminDashboardPage(key, feedback, telemetry) {
  const safeKey = encodeURIComponent(key);
  const surveyCount = feedback.filter(e => e.kind === "survey").length;
  const bugCount = feedback.filter(e => e.kind === "bug").length;
  const ideaCount = feedback.filter(e => e.kind === "idea").length;
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="30">
<title>Blackout Hunt feedback</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: system-ui, sans-serif; background:#0d1114; color:#e9f2ef; margin:0; padding:24px; }
  h1 { margin:0 0 4px; font-size:22px; }
  .summary { color:#8aa; margin-bottom:20px; font-size:14px; }
  .summary a { color:#7fd1ff; }
  .cols { display:grid; grid-template-columns: 1fr; gap:24px; max-width:1100px; }
  @media (min-width: 900px) { .cols { grid-template-columns: 3fr 2fr; } }
  h2 { font-size:15px; text-transform:uppercase; letter-spacing:0.08em; color:#9fb; border-bottom:1px solid #243; padding-bottom:6px; }
  .card { background:#141a1f; border:1px solid #223; border-left:4px solid #2a6; border-radius:8px; padding:12px 14px; margin-bottom:12px; }
  .card.kind-bug { border-left-color:#e0584a; }
  .card.kind-idea { border-left-color:#e9c46a; }
  .card.kind-praise { border-left-color:#2a9d8f; }
  .card.kind-survey { border-left-color:#7fb3ff; }
  .card.telemetry { border-left-color:#6a7; }
  header { display:flex; gap:10px; align-items:baseline; flex-wrap:wrap; }
  .kind { font-weight:700; text-transform:uppercase; font-size:12px; letter-spacing:0.05em; }
  .reason { color:#9aa; font-size:12px; }
  .stars { color:#e9c46a; }
  time { margin-left:auto; color:#788; font-size:12px; }
  .ctx { color:#8ba; font-size:12px; margin:4px 0; }
  .msg { white-space:pre-wrap; margin:8px 0; }
  .survey { font-size:13px; color:#bcd; }
  .contact { font-size:12px; color:#9ab; margin-top:6px; }
  .diag { font-size:12px; color:#789; margin-top:8px; }
  .rounds { font-size:12px; color:#9ba; margin:6px 0; }
  pre { white-space:pre-wrap; background:#0a0e10; padding:8px; border-radius:6px; max-height:280px; overflow:auto; font-size:11px; }
  .empty { color:#677; font-style:italic; }
</style>
</head>
<body>
  <h1>Blackout Hunt &mdash; player feedback</h1>
  <div class="summary">
    ${feedback.length} feedback (${bugCount} bug, ${ideaCount} idea, ${surveyCount} survey) &middot; ${telemetry.length} telemetry sessions &middot; auto-refresh 30s &middot;
    <a href="/admin/feedback.json?key=${safeKey}">feedback.json</a> &middot;
    <a href="/admin/telemetry.json?key=${safeKey}">telemetry.json</a>
  </div>
  <div class="cols">
    <section>
      <h2>Feedback &amp; surveys</h2>
      ${feedback.length ? feedback.map(renderFeedbackCard).join("\n") : '<p class="empty">No feedback yet.</p>'}
    </section>
    <section>
      <h2>Performance &amp; logs</h2>
      ${telemetry.length ? telemetry.map(renderTelemetryCard).join("\n") : '<p class="empty">No telemetry yet.</p>'}
    </section>
  </div>
</body>
</html>`;
}

async function route(req, res) {
  const url = new URL(req.url, publicBaseUrl);

  if (req.method === "GET" && url.pathname === "/health") {
    json(res, 200, {
      ok: true,
      features: {
        env_file: true,
        signed_login_state: true,
        feedback: true,
        telemetry: true,
      },
      providers: {
        google: isProviderConfigured("google"),
        microsoft: isProviderConfigured("microsoft"),
      },
    });
    return;
  }

  if (req.method === "GET" && url.pathname === "/setup/google") {
    html(res, 200, googleSetupPage());
    return;
  }

  if (req.method === "POST" && url.pathname === "/setup/google") {
    await handleGoogleSetup(req, res);
    return;
  }

  const startMatch = /^\/auth\/(google|microsoft)\/start$/.exec(url.pathname);
  if (req.method === "GET" && startMatch) {
    await handleAuthStart(startMatch[1], url, res);
    return;
  }

  const callbackMatch = /^\/auth\/(google|microsoft)\/callback$/.exec(url.pathname);
  if (req.method === "GET" && callbackMatch) {
    await handleAuthCallback(callbackMatch[1], url, res);
    return;
  }

  const deviceMatch = /^\/auth\/device\/([a-zA-Z0-9_-]+)$/.exec(url.pathname);
  if (req.method === "GET" && deviceMatch) {
    json(res, 200, deviceAuth.get(deviceMatch[1]) || { status: "pending" });
    return;
  }

  if (req.method === "GET" && url.pathname === "/player/me") {
    const player = authPlayer(req);
    if (!player) {
      json(res, 401, { error: "unauthorized" });
      return;
    }
    json(res, 200, { player });
    return;
  }

  if (req.method === "PUT" && url.pathname === "/player/save") {
    const player = authPlayer(req);
    if (!player) {
      json(res, 401, { error: "unauthorized" });
      return;
    }

    let body;
    try {
      body = JSON.parse((await readBody(req)) || "{}");
    } catch {
      json(res, 400, { error: "invalid JSON body" });
      return;
    }
    player.progress = body.progress || {};
    player.selected_avatar_url = body.progress?.selected_avatar_url || player.avatar_url || "";
    player.updated_at = new Date().toISOString();
    await savePlayers();
    json(res, 200, { ok: true, player });
    return;
  }

  // Player feedback, bug reports, feature ideas, and end-of-round surveys. No account required
  // so guest clients can submit. The client posts to BackendBaseUrl + "/feedback".
  if (req.method === "POST" && url.pathname === "/feedback") {
    await handleFeedback(req, res);
    return;
  }

  // Performance + recent-log session diagnostics. Client posts to "/telemetry/session".
  if (req.method === "POST" && url.pathname === "/telemetry/session") {
    await handleTelemetrySession(req, res);
    return;
  }

  // Owner-only views, gated by the admin token.
  if (req.method === "GET" && url.pathname === "/admin") {
    if (!adminAuthorized(req, url)) {
      html(res, 401, adminUnauthorizedPage());
      return;
    }
    const feedback = (await readJsonLines(feedbackPath, maxStoredEntries)).reverse();
    const telemetry = (await readJsonLines(telemetryPath, maxStoredEntries)).reverse();
    html(res, 200, adminDashboardPage(url.searchParams.get("key") || "", feedback.slice(0, 300), telemetry.slice(0, 300)));
    return;
  }

  if (req.method === "GET" && (url.pathname === "/admin/feedback.json" || url.pathname === "/admin/telemetry.json")) {
    if (!adminAuthorized(req, url)) {
      json(res, 401, { error: "unauthorized" });
      return;
    }
    const path = url.pathname.includes("telemetry") ? telemetryPath : feedbackPath;
    json(res, 200, { entries: (await readJsonLines(path, maxStoredEntries)).reverse() });
    return;
  }

  json(res, 404, { error: "not found" });
}

await mkdir(dataDir, { recursive: true });
await loadPlayers();
await loadStateSecret();
await loadAdminToken();

createServer((req, res) => {
  route(req, res).catch(error => {
    console.error(error);
    // Log the detail server-side; do not leak parser/internal messages to clients.
    json(res, 500, { error: "internal error" });
  });
}).listen(port, bindHost, () => {
  console.log(`Blackout Hunt account backend listening on http://${bindHost}:${port}`);
  console.log(`Public base URL: ${publicBaseUrl}`);
  console.log(`Feedback dashboard: ${publicBaseUrl}/admin?key=${adminToken}`);
  console.log(`Feedback email backup: ${emailConfigured() ? `enabled -> ${process.env.FEEDBACK_EMAIL_TO}` : "disabled (set SMTP_PASS + FEEDBACK_EMAIL_TO in .env)"}`);
});
