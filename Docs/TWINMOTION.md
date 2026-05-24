# Twinmotion Visual Pass

Twinmotion is installed at:

`D:\Twinmotion2026.1`

The Unreal project has editor-side Datasmith import support enabled. Use Twinmotion as the fast art/blockout tool, then import the finished environment into Unreal as static scene content.

## Recommended Workflow

1. Open Twinmotion.
2. Build or kitbash a compact abandoned facility scene.
3. Keep gameplay-critical routes close to the current runtime layout: survivor spawn side, hunter spawn side, five breaker rooms, central shutter area, lockers, and east exit.
4. Export from Twinmotion with `File > Export to Datasmith file`.
5. Prefer Optimized export mode for repeated props, corridors, and copied set dressing.
6. In Unreal Editor, import the `.udatasmith` file into `Content/Twinmotion/Facility`.
7. Save the imported level as `Content/Maps/L_Facility.umap`.
8. Move or recreate gameplay actors in the level: breakers, lockers, doors, batteries, shutters, terminals, exit gate, and spawn points.
9. Once the authored level is working, update host travel from `/Engine/Maps/Entry` to `/Game/Maps/L_Facility`.

## Useful Scene Targets

- A reception/security threshold near the hunter side.
- Maintenance tunnels with pipes, exposed conduit, and low ceiling clearance.
- A ward wing with curtains, gurneys, and false hiding corners.
- A wet lab with glass dividers, emergency lights, and noisy equipment.
- A storage wing with shelves and forklift-style lane breaks.
- A central checkpoint with shutters that can be opened from two terminals.
- One exit route that becomes obvious only after the breakers are repaired.

## Notes

- Do not make the Twinmotion import the source of gameplay logic. Use it for meshes, materials, lighting reference, and set dressing.
- Keep collision simple in Unreal. Imported visual meshes can be expensive or awkward for multiplayer collision.
- Imported lights are useful as a starting point, but gameplay flicker circuits should remain controlled by Blackout Hunt actors.
- Official Epic workflow reference: https://dev.epicgames.com/documentation/en-us/twinmotion/twinmotion-to-unreal-engine-workflow
