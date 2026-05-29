# Live Classroom Feedback Capture - 2026-05-29

Use this sheet after the May 29 live classroom test. Keep it teacher-local until it has been reviewed for privacy.

## Package Anchor

- Build version: `0.2.0-beta.6`
- Package to distribute:

```text
Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip
```

- SHA-256:

```text
9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7
```

- Sidecar hash file:

```text
Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip.sha256
```

- Preferred join endpoint:

```text
blackouthunt.playit.plus:24761
```

## Pre-Test Package Verification Record

- SHA-256 confirmed on May 28, 2026: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- Archive listing was readable: 175 entries.
- Expected launcher/notices files were present: `BlackoutHunt.exe`, `Launch-BlackoutHunt-DX11.cmd`, `Launch-BlackoutHunt-DX11-Low.cmd`, `THIRD-PARTY-NOTICES.txt`, and `BlackoutHunt\Binaries\Win64\BlackoutHunt-Win64-Shipping.exe`.
- Archive listing did not match `Saved\Account`, `Saved\Logs`, `Saved\Crashes`, or obvious secret/credential path patterns.
- LT9 decision: distribute the 0.2.0-beta.6 archive and sidecar above, not the current local `Builds\Windows` folder. A May 28 recheck found the staged folder's shipping executable had drifted from the archived 0.2.0-beta.6 executable.
- Practical validation caveat: the current dirty `Tools\Verify-ClassroomPackage.ps1` has an in-progress syntax error near line 216, so it is not currently a reliable rerun target until that unrelated tool edit is resolved.

## Privacy Rules

Collect only the minimum evidence needed to reproduce problems.

Do collect:

- anonymous or local machine label, such as `teacher-pc`, `lab-07`, or `student-client-03`
- local lobby display names only if they are safe to keep in teacher-local notes
- map, mode, lesson preset, graphics preset, launch method, join path, and failure symptoms
- package/runtime logs from the teacher machine only after reviewing them for local setup details

Do not collect:

- saved account data
- passwords
- backend secrets
- private student account/profile data
- raw IP/account details unless they are necessary for a local IT-only diagnosis
- raw logs that have not been reviewed for sensitive local setup details
- student full names if a pseudonym or machine label is enough

## Session Summary

- Test date/time:
- Teacher/operator:
- Package used:
- SHA-256 checked before distribution: yes/no
- Teacher machine label:
- Map:
- Mode:
- Lesson preset or manual set:
- Scare intensity:
- Teacher count:
- Bot count:
- Host/client count at peak:
- Exact join path used:
- Classroom board/projector used: yes/no
- Manual 12-question backup generated: yes/no

## Launch And Machine Matrix

Use local labels instead of student names.

| Machine label | Role | Launch method | Result | Graphics preset | Window/resolution | CPU/RAM/GPU if failure happened | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| teacher-pc | host | `BlackoutHunt.exe` | | | | | |
| lab-01 | client | `BlackoutHunt.exe` | | | | | |
| lab-02 | client | `BlackoutHunt.exe` | | | | | |

Launcher results to record:

- normal launcher worked
- `.cmd` launcher worked
- `.cmd` launcher blocked
- `.bat` launcher worked
- `.bat` launcher blocked
- failed before menu
- reached menu but unstable
- D3D11 feature-level error

## Join And Lobby Matrix

| Machine label | Join path | Lobby name safe to keep? | Joined roster | Ready worked | Role assigned | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| lab-01 | saved endpoint / typed endpoint / invite code / LAN | yes/no | yes/no | yes/no | | |
| lab-02 | saved endpoint / typed endpoint / invite code / LAN | yes/no | yes/no | yes/no | | |

Record if students saw any host-only controls. This should be `no`.

- Student saw role assignment/admin/tunnel/preflight controls: yes/no
- If yes, machine label and exact screen:

## Student Friction

Record what students got stuck on, not who got stuck.

| Moment | What happened | How many affected | Recovery used | Follow-up needed |
| --- | --- | --- | --- | --- |
| Launch | | | | |
| Lobby name | | | | |
| Join endpoint | | | | |
| Ready gate | | | | |
| First objective | | | | |
| Question controls | | | | |
| Hiding/interactions | | | | |
| Teacher/Hall Monitor roles | | | | |
| Performance/settings | | | | |

## Technical Failures

Create one row per distinct failure. Prefer exact steps and machine labels over memory.

| ID | Machine label | Phase | Symptom | Steps to reproduce | Expected | Actual | Recovery | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LT8-F1 | | launch/join/lobby/round/exit | | | | | | |

Failure categories to consider:

- crash
- disconnect
- stuck lobby state
- Playit endpoint unavailable
- firewall prompt
- GPU/D3D11 error
- severe frame drops
- audio runaway
- missing UI prompt
- unable to ready
- unable to kick stuck client
- role/admin visibility issue

## Safe Log Handling

Teacher package logs are expected under the extracted package root:

```text
BlackoutHunt\Saved\Logs
```

In this repository's verified staged package, the path is:

```text
Builds\Windows\BlackoutHunt\Saved\Logs
```

If technical failures occurred on the teacher/dev machine, create a support bundle after the run:

```powershell
.\Tools\New-ClassroomSupportBundle.ps1
```

Then scan package logs when present:

```powershell
.\Tools\Test-RuntimeLogs.ps1 -Path .\Builds\Windows\BlackoutHunt\Saved\Logs
```

Before sharing any bundle or log outside the project team:

- review `PREFLIGHT.md`
- review copied logs under the bundle `logs` folder
- remove or redact local setup details that are not needed for diagnosis
- do not add `Saved\Account`, `Saved\Crashes`, backend data, or credential files

## Evidence Collected

- Support bundle created: yes/no
- Support bundle path:
- Runtime log gate run: yes/no
- Runtime log gate result:
- Classroom report exported: yes/no
- Classroom report kept teacher-local: yes/no
- Heatmap telemetry exported: yes/no
- Heatmap telemetry reviewed for anonymous-only rows: yes/no

## Follow-Up Task Queue

Turn failures into specific tasks. Keep each task tied to evidence.

| Priority | Task | Evidence ID | Owner | Notes |
| --- | --- | --- | --- | --- |
| high/medium/low | | LT8-F1 | | |

Suggested task wording:

- `Fix packaged launch failure on <machine label/spec> when double-clicking BlackoutHunt.exe. Evidence: LT8-F1.`
- `Investigate Playit join timeout using endpoint blackouthunt.playit.plus:24761. Evidence: LT8-F2.`
- `Improve first-round objective guidance where students stalled at <moment>. Evidence: LT8-F3.`

## Completion Checklist

- [ ] Package name and SHA-256 recorded.
- [ ] Map, mode, endpoint, preset, host/client count recorded.
- [ ] Launch behavior recorded for teacher and affected student machines.
- [ ] Student/admin visibility boundary checked.
- [ ] Failures have reproduction steps or clear observed symptoms.
- [ ] No private student/account/password/backend data included.
- [ ] Support bundle created if technical failures occurred.
- [ ] Logs reviewed before sharing.
- [ ] Follow-up tasks drafted from evidence.
