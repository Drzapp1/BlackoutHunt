# Caddy — TLS terminator for the feedback HTTPS tunnel

`Tools/AccountBackend/Start-FeedbackStack.ps1` runs this Caddy to terminate TLS for the
Playit **HTTPS** tunnel and reverse-proxy decrypted requests to the feedback backend
(`Tools/AccountBackend/server.mjs`) on `127.0.0.1:8787`. Caddy obtains and auto-renews a
Let's Encrypt certificate; the site config is `Tools/AccountBackend/Caddyfile`.

- Source: https://caddyserver.com/download (`os=windows`, `arch=amd64`)
- Version: **v2.11.3** (as downloaded 2026-05-29)
- File: `caddy.exe` — **git-ignored** (53 MB binary, not committed; see `.gitignore`)
- SHA-256 (this build): `2ae8cb4df7b4dfef0fb59b1ce41750b477a8cbe164b12272ab82f1681d89ad42`

Re-download (PowerShell), from the repo root:

```powershell
curl -L -o ThirdParty\Caddy\caddy.exe "https://caddyserver.com/api/download?os=windows&arch=amd64"
```

The download API serves the current stable build, so a fresh download may be newer than the
hash above — that's expected.

Note: the Caddyfile forces an **RSA** leaf (`key_type rsa2048`) so the certificate chain
terminates at **ISRG Root X1**, which is present in UE 5.7's bundled `cacert.pem`. Caddy's
default ECDSA chain currently runs through **ISRG Root X2**, which the engine bundle lacks —
so a packaged game build would reject it.
