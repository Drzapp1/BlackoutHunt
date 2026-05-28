# Facility Vertical Slice

The current Facility is still a runtime blockout. Treat this checklist as the acceptance gate before it becomes the production reference map.

## Goals

- Every quadrant has a distinct landmark read from the central hub within 3 seconds.
- Objective silhouettes are readable from doorway distance, not only at interaction range.
- Lockers sit near loop exits, not only in dead ends.
- Both subway exits are findable from the center without opening the HUD map.
- Lighting uses color and shadow to label route identity: storage amber, lab cyan, ward violet, utility white, exits green.
- Chases have at least two break-line choices before a capture range commitment.
- Physics Classroom stations keep team clusters visible without blocking the main chase route.

## Current Runtime Pass

- Central Facility route stripes now point toward both exits and major destination clusters.
- Landmark floor pads identify storage, lab, ward, and utility/classroom lanes.
- A C++ vertical-slice pass adds hub-readable route chevrons, glyph overhead signs, route gateways, quadrant beacons, objective silhouettes, objective-type glyph plaques, and color-coded route lights.
- Exit routes now have extended green chevrons, runway dots, platform gateway frames, door leaf silhouettes, and card-reader props so both subway exits read from the hub and spawn-side lane.
- Hub route screens and small card/drawer dressing give the central hub and nearby room thresholds a more authored, less blockout-only read while staying nonblocking.
- Additional loop-side lockers now sit on the main north/south chase routes instead of only at room ends, with floor split cues marking nearby break-line choices.
- Imported props from the cooked SmartBasicInterfaces pack dress storage, lab, ward, utility, and exit lanes with cabinets, panels, generator sections, light fixtures, door frames, card readers, alarm panels, power buttons, and gas cans.
- Existing locker, battery, station, and exit placement remains unchanged for balance stability.

## Runtime Asset Paths

These imported assets are used by `ABHGameMode::AddFacilityVerticalSlicePass()` through `ABHRuntimeMeshPropActor`:

- `/Game/SmartBasicInterfaces/Meshes/SM_cabinet.SM_cabinet`
- `/Game/SmartBasicInterfaces/Materials/MI_Cabinet.MI_Cabinet`
- `/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel`
- `/Game/SmartBasicInterfaces/Materials/MI_Panel.MI_Panel`
- `/Game/SmartBasicInterfaces/Meshes/SM_generatorFront.SM_generatorFront`
- `/Game/SmartBasicInterfaces/Materials/MI_GeneratorFront.MI_GeneratorFront`
- `/Game/SmartBasicInterfaces/Meshes/SM_generatorSection.SM_generatorSection`
- `/Game/SmartBasicInterfaces/Materials/MI_GeneratorSection.MI_GeneratorSection`
- `/Game/SmartBasicInterfaces/Meshes/SM_light.SM_light`
- `/Game/SmartBasicInterfaces/Materials/MI_EmissiveLight.MI_EmissiveLight`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoorframe.SM_doubledoorframe`
- `/Game/SmartBasicInterfaces/Materials/MI_DoubleDoorFrame.MI_DoubleDoorFrame`
- `/Game/SmartBasicInterfaces/Meshes/SM_cardreader.SM_cardreader`
- `/Game/SmartBasicInterfaces/Materials/MI_CardReader.MI_CardReader`
- `/Game/SmartBasicInterfaces/Meshes/SM_alarm.SM_alarm`
- `/Game/SmartBasicInterfaces/Materials/MI_EmissiveRed.MI_EmissiveRed`
- `/Game/SmartBasicInterfaces/Meshes/SM_powerbtn.SM_powerbtn`
- `/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN`
- `/Game/SmartBasicInterfaces/Meshes/SM_gascan.SM_gascan`
- `/Game/SmartBasicInterfaces/Materials/MI_GasCan.MI_GasCan`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoor01.SM_doubledoor01`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoor02.SM_doubledoor02`
- `/Game/SmartBasicInterfaces/Materials/MI_DoubleDoor.MI_DoubleDoor`
- `/Game/SmartBasicInterfaces/Meshes/SM_cabinetdrawer.SM_cabinetdrawer`
- `/Game/SmartBasicInterfaces/Materials/MI_CabinetDrawer.MI_CabinetDrawer`
- `/Game/SmartBasicInterfaces/Meshes/SM_card.SM_card`
- `/Game/SmartBasicInterfaces/Materials/MI_Card01.MI_Card01`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel2.MI_ScreenPanel2`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel3.MI_ScreenPanel3`

If an imported mesh is missing, the spawner uses a tinted block fallback with the same collision setting. If a material is missing, the imported mesh keeps its default material or the fallback block uses the supplied route tint.

## Validation Viewpoints

Use these runtime positions as the manual review baseline after launching Facility:

- Spawn-side lane: around `(4400, 0, 160)`, looking west toward the hub and exit language.
- Central hub: around `(0, 0, 160)`, checking that `S`, `L`, `W`, `U`, and green `E` route signs are readable within 3 seconds.
- Storage/lab thresholds: around `(-2150, -1185, 160)` and `(2450, -1185, 160)`, checking amber/cyan gateways, nearby route screens, and station glyph plaques from doorway distance.
- Ward/utility thresholds: around `(-2550, 1185, 160)` and `(250, 1185, 160)`, checking violet/white gateways, objective glyph plaques, and route callouts.
- Subway platforms: around `(-4680, 0, 160)` and `(4680, 0, 160)`, checking green runway dots, platform gateway frames, door leaf silhouettes, and card-reader props.
- Chase loop checks: around `(-2225, -1800, 160)` and `(2225, 1800, 160)`, checking that loop lockers have visible split cues without blocking paths.

## Next Authored Pass

- Move stable room shapes into authored geometry or reusable room Blueprints.
- Replace runtime block chevrons/signs with embedded floor decals, authored signage, and room Blueprint lighting props once the route language is approved in playtests.
- Add room-local sound occlusion volumes before expanding objective count.
- Verify one survivor can kite from each objective to a locker without crossing another objective's interaction prompt.
- Capture screenshots from spawn, center hub, each objective cluster, each exit, and two chase loops.
