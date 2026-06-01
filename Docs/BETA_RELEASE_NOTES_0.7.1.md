# Blackout Hunt 0.7.1 Release Notes — Maps actually ship now

## Headline

`0.7.1` is a hotfix for a showstopper in `0.7.0`: **clicking any mode just bounced
you back to the menu and never let you play.** The cause was packaging, not gameplay —
the authored maps were never being cooked into the build, so the moment the game tried
to travel to one it failed and fell back to the standalone menu map. `0.7.1` cooks the
maps in, so Host / Bots / Test / Live Classroom / Tutorial all load and play.

## What was wrong

`0.7.0` turned the authored maps on by default (`bUseAuthoredLevels=True`), and the
tutorial is always authored. But every packaging script cooked only the engine entry
map (`-map=/Engine/Maps/Entry`). None of `Facility`, `Substation`, `Foggrounds`, or
`Tutorial` made it into the package. At runtime the menu routes every mode to one of
those maps; with the map missing, `OpenLevel` fails and Unreal drops the player back on
the standalone entry map — which is where the menu lives. So every button looked like it
"returned to the menu."

It worked in-editor (the uncooked maps load straight from the project), which is why it
slipped through — the cooked build was the only place it broke.

## What changed

- **Cook the authored maps.** All five packaging/validation scripts now pass the four
  maps in their `-map=` list, so `Facility`, `Substation`, `Foggrounds`, and `Tutorial`
  are in the package.
- **Guard against a repeat.** `Verify-EOSPackage` now lists the cooked container and
  fails the build if any authored map is missing, instead of letting it ship.
- **Build version** bumped to `0.7.1` (online build id included, so 0.7.1 and 0.7.0
  clients won't try to join each other).

No gameplay, content, or balance changes — everything from `0.7.0` (interactive visual
questions, the three-role tutorial, the jumpscare overhaul, authored maps, EOS online)
is unchanged. If you were on `0.7.0`, this is a drop-in replacement.

## Upgrading

Replace your `0.7.0` folder with this one. Everyone in a session should be on the same
`0.7.1` build.
