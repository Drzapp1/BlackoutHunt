import { createServer } from "node:http";
import { createHash, createHmac, randomBytes, timingSafeEqual } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, URL, URLSearchParams } from "node:url";

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
const dataDir = join(backendDir, "data");
const playersPath = join(dataDir, "players.json");
const stateSecretPath = join(dataDir, "state-secret.txt");
const loginStateTtlMs = 15 * 60 * 1000;

const pendingStates = new Map();
const deviceAuth = new Map();
const sessions = new Map();
let players = {};
let stateSecret = "";

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
    req.on("data", chunk => {
      body += chunk;
      if (body.length > 1024 * 1024) {
        reject(new Error("request body too large"));
        req.destroy();
      }
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

async function route(req, res) {
  const url = new URL(req.url, publicBaseUrl);

  if (req.method === "GET" && url.pathname === "/health") {
    json(res, 200, {
      ok: true,
      features: {
        env_file: true,
        signed_login_state: true,
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

    const body = JSON.parse(await readBody(req) || "{}");
    player.progress = body.progress || {};
    player.selected_avatar_url = body.progress?.selected_avatar_url || player.avatar_url || "";
    player.updated_at = new Date().toISOString();
    await savePlayers();
    json(res, 200, { ok: true, player });
    return;
  }

  json(res, 404, { error: "not found" });
}

await mkdir(dataDir, { recursive: true });
await loadPlayers();
await loadStateSecret();

createServer((req, res) => {
  route(req, res).catch(error => {
    console.error(error);
    json(res, 500, { error: error.message });
  });
}).listen(port, "127.0.0.1", () => {
  console.log(`Blackout Hunt account backend listening on http://127.0.0.1:${port}`);
  console.log(`Public base URL: ${publicBaseUrl}`);
});
