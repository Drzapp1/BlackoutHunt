<#
.SYNOPSIS
  Start the Blackout Hunt feedback stack: Node backend + Playit agent + Caddy (TLS).

.DESCRIPTION
  Brings up the three processes that let in-game feedback reach you over HTTPS from
  anywhere:
    1. Node backend  (Tools/AccountBackend/server.mjs)  -> 127.0.0.1:8787 (loopback)
    2. Playit agent  (ThirdParty/Playit/playit.exe)     -> tunnels your domain to local Caddy
    3. Caddy         (ThirdParty/Caddy/caddy.exe)        -> terminates TLS, proxies to the backend

  Each runs in its own window so you can watch logs / close individually. Re-running
  skips anything already listening. The feedback dashboard URL is printed at the end.

  For always-on hosting, set this script to run at logon (Task Scheduler) or wrap the
  three processes as Windows services (e.g. with NSSM).
#>
[CmdletBinding()]
param(
  [int]$Port = 8787
)

$ErrorActionPreference = "Stop"
$root      = Split-Path -Parent $MyInvocation.MyCommand.Path        # Tools/AccountBackend
$repoRoot  = (Resolve-Path (Join-Path $root "..\..")).Path
$caddyExe  = Join-Path $repoRoot "ThirdParty\Caddy\caddy.exe"
$playitExe = Join-Path $repoRoot "ThirdParty\Playit\playit.exe"
$caddyfile = Join-Path $root "Caddyfile"
$playitSecret = Join-Path $env:LOCALAPPDATA "playit_gg\playit.toml"

function Test-Listening([int]$p) {
  try { return [bool](Get-NetTCPConnection -State Listen -LocalPort $p -ErrorAction Stop) }
  catch { return [bool]((netstat -ano) -match (":{0}\s.*LISTENING" -f $p)) }
}

function Test-ProcessRunning([string]$name) {
  return [bool](Get-Process -Name $name -ErrorAction SilentlyContinue)
}

# 1. Node backend
if (Test-Listening $Port) {
  Write-Host "[backend] already listening on $Port — skipping." -ForegroundColor Yellow
} else {
  Write-Host "[backend] starting Node backend on 127.0.0.1:$Port ..." -ForegroundColor Cyan
  Start-Process -FilePath "node" -ArgumentList ".\server.mjs" -WorkingDirectory $root -WindowStyle Normal
}

# 2. Playit agent (uses the already-claimed secret in %LOCALAPPDATA%\playit_gg\playit.toml)
if (Test-ProcessRunning "playit") {
  Write-Host "[playit] agent already running — skipping." -ForegroundColor Yellow
} elseif (Test-Path $playitExe) {
  if (Test-Path $playitSecret) {
    Write-Host "[playit] starting tunnel agent ..." -ForegroundColor Cyan
    Start-Process -FilePath $playitExe -ArgumentList @("--secret-path", "`"$playitSecret`"") -WindowStyle Normal
  } else {
    Write-Warning "[playit] no claimed secret at $playitSecret. Run playit.exe once and claim it to your account first."
  }
} else {
  Write-Warning "[playit] $playitExe not found."
}

# 3. Caddy (TLS termination + reverse proxy)
if (Test-ProcessRunning "caddy") {
  Write-Host "[caddy] already running — skipping." -ForegroundColor Yellow
} elseif (Test-Path $caddyExe) {
  Write-Host "[caddy] starting (TLS + reverse_proxy to 127.0.0.1:$Port) ..." -ForegroundColor Cyan
  Start-Process -FilePath $caddyExe -ArgumentList @("run", "--config", "`"$caddyfile`"", "--adapter", "caddyfile") -WorkingDirectory $root -WindowStyle Normal
} else {
  Write-Warning "[caddy] $caddyExe not found."
}

# Report the dashboard URL using the admin token from .env (if present).
$envPath = Join-Path $root ".env"
$token = $null
if (Test-Path $envPath) {
  $m = Select-String -Path $envPath -Pattern '^\s*ADMIN_TOKEN\s*=\s*(.+)$' | Select-Object -First 1
  if ($m) { $token = $m.Matches[0].Groups[1].Value.Trim() }
}
Write-Host ""
Write-Host "Feedback stack starting." -ForegroundColor Green
if ($token) {
  Write-Host ("Dashboard (local):  http://127.0.0.1:{0}/admin?key={1}" -f $Port, $token)
}
Write-Host  "Dashboard (public): https://blackouthunt-feedback.playit.plus/admin?key=<your ADMIN_TOKEN>"
Write-Host  "First Caddy start fetches a Let's Encrypt cert once the Playit HTTPS tunnel is live (can take ~30s)."
