# Blackout Hunt 0.8.1 — making the revision nodes behave

0.8.0 was about a full class surviving a round together. 0.8.1 is about the part that happens at the question nodes themselves — answering, contributing, getting the door open — where a few things could quietly trap a player or tell them the wrong thing. It's a live-play hardening pass: small fixes, but they're the ones that decide whether a node feels fair or feels broken. A couple of "the round shouldn't end like that" fixes came along too.

## Answering a node is about who's standing there

The biggest one. A question node used to be answered by a class-wide "answer team" the game picked for you by mastery and contribution count. That could lock you out of your own node: a lone student — or a Hall Monitor working a node by themselves — might never make up the majority, and a teammate who answered once and then wandered off could leave you stuck at "Vote recorded (1/5)" with no way to finish.

Now the answer team is simply whoever is physically standing at the node, rebuilt every single vote. One person at a node can clear it themselves. A cluster still votes together. Someone walking away shrinks the team to who's actually there instead of blocking it, and their stale vote is dropped rather than left hanging in the count. It's the same "who's actually here" model the multi-repairer breakers already used.

## The contribution counter tells the truth now

The exit gate and the Hall Monitor tools enforce 5–6 contributions before the class can leave. The on-screen counter, though, was capped at 4 — so it could show you a satisfied "4/4" while the door stayed firmly locked and nothing explained why. The displayed requirement now matches what the server actually enforces, everywhere it's shown, so "done" means done.

## Hall Monitors get told what they can actually do

Monitors can answer questions but never do the survivor hold-E task. The game used to hand them survivor instructions anyway — "hold E", "CONTRIBUTE IN PHYSICS CLASSROOM" — on nodes that were already solved, telling them to do something their role can't. That copy is role-aware now: a Monitor is told to answer another node to keep contributing, and that survivors finish the node from here.

While I was in the prompt code: the centred interaction prompt no longer draws behind the question panel. It was rendering straight through the opaque panel (and getting worse at higher HUD scale); now the panel carries its own "press 1-4 / Tab" hint cleanly and the duplicate underneath is gone.

## A dropped student doesn't hand away the round

Two round-resolution fixes that only bite under real classroom conditions:

- If the last alive survivor (or survivors) drop together on a Wi-Fi blip, the round used to resolve as an instant Teacher win. Now it holds for the reconnect grace before resolving — you'll see "holding the round for a reconnect" — so a transient drop gives people the 120 seconds they already had to come back. It can only ever delay a legitimate Teacher win, never prevent one: if they don't return, the round timer resolves it anyway.
- If the evacuation train leaves with some students already aboard, the ones who boarded now count as escaping — a survivor win — instead of the whole thing being scored as a clean Teacher win over the stragglers who didn't make it.

## Your progress is keyed to you, not your name

On the account-free classroom path (direct-IP, LAN, Playit), there's no Epic ID to lean on, so cross-level progress used to fall back to matching on your typed name. That meant a brand-new student who happened to reuse an earlier student's name could inherit their points and powerups and clobber the freshly reset revision stats. Restoring banked progress now requires the server-issued reconnect token or a real online ID — a name on its own can't claim someone else's run. A genuine cross-level traveller always carries its token, so this never gets in their way.

## Hosting: LAN works out of the box now

The Live Classroom host binds all network interfaces by default instead of loopback-only. Students can now join by LAN IP if they're in the same room or on the same switch, *or* over the tunnel / BH1 code from off-LAN — you're no longer forced through the Playit tunnel for everyone. The one tradeoff is a one-time Windows Firewall "Allow" the first time you host. If you specifically want a tunnel-only setup with no LAN fallback, the old loopback-only host is still a single config flag (`bClassroomLoopbackOnlyHost=True`) away.

## Getting in (Windows)

Unzip it, run `BlackoutHunt.exe`, and make sure everyone in a session is on 0.8.1 — the online build id moved with the version, so 0.8.1 and 0.8.0 clients won't see each other's sessions. For a hosted class over the tunnel, the Custom UDP mapping from the 0.8.0 notes still applies. This build went through the automated suite and the cooked host-plus-clients playability gate before I tagged it.

## Known limits

- Still a beta. It's been through the automation tests and the cooked smoke run (a real host plus two clients booting the package and playing through to a started round), but the live-play hardening above is exactly the kind of thing that wants a proper in-engine playtest with real students — so treat it as one, and tell me what trips up :)
- The hosted-over-tunnel path still needs the Custom UDP mapping set in your playit dashboard by hand; the game detects it and walks you through it, but I can't do that part for you.
