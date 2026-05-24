# Playit Tunnel Agent

This folder contains the optional Windows tunnel helper used by the in-game `START INTERNET TUNNEL` button.

- Source: https://github.com/playit-cloud/playit-agent
- Downloaded release: `v1.0.4`
- File: `playit-windows-x86_64-signed.exe`, stored locally as `playit.exe`
- SHA-256: `88000d40af7a8e5a0548d27d71c0cad7d5f4b91fd85f6e9297237ac8b57fbdc9`

The agent lets a listen-server host expose UDP `7777` through a public tunnel when router port forwarding, CGNAT, hotel Wi-Fi, campus Wi-Fi, or mobile networks block direct inbound traffic. The host still needs to claim the agent and create/select a Custom UDP tunnel in the browser; players only need the resulting allocation address and port.
