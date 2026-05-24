# Asset Sources

Blackout Hunt now includes a small ambientCG material set imported into Unreal `.uasset` files under:

- `Content/BlackoutHunt/Art/Textures`
- `Content/BlackoutHunt/Art/Materials`

The original downloaded 1K JPG source sets are kept under:

- `ThirdParty/ambientCG`

## License

ambientCG states that its downloadable assets are licensed under Creative Commons CC0 1.0 Universal, including use in games and commercial projects.

Project attribution text:

`Created using assets from ambientCG.com, licensed under the Creative Commons CC0 1.0 Universal License.`

## Imported Assets

- `Concrete048`: concrete floor and large concrete surfaces
- `Plaster001`: facility and substation walls/ceilings
- `Metal063`: rusty industrial metal
- `DiamondPlate009`: substation and utility floor plates
- `PaintedMetal004`: painted panels, props, and pipes
- `Tiles078`: tiled facility floor patches
- `Sign009`: warning placards near exits and hazard routes

## Foggrounds Outdoor Assets

Foggrounds can run entirely from C++ primitive fallback dressing, but it also supports optional imported CC0 outdoor meshes from Kenney Nature Kit:

- source page: `https://kenney.nl/assets/nature-kit`
- expected local package: `ThirdParty/Kenney/NatureKit.zip` or extracted `ThirdParty/Kenney/NatureKit`
- Unreal destination: `Content/BlackoutHunt/Art/Foggrounds/Nature`

The importer stops if it cannot confirm a CC0 license in the downloaded package.

## Interactable Prop Visuals

Runtime interactable nodes use these imported CC0 materials for their prop bodies and panels:

- batteries, breakers, lockers, terminals, switches, alarms, decoys, and objective stations reuse the painted metal, rusted metal, diamond plate, and warning sign material set
- recognizable prop silhouettes are assembled from Unreal static mesh primitives in C++ so they cook with the game without a separate DCC import step

## Audio Assets

Lobby music and menu click sounds are generated in-house from deterministic synthesis scripts, then imported as Unreal `SoundWave` assets. The lobby loop also includes a processed imported scream layer:

- source WAV files: `Content/BlackoutHunt/Audio/Sources`
- imported assets: `Content/BlackoutHunt/Audio`
- generation script: `Tools/GenerateAudioSources.ps1`
- import script: `Tools/ImportAudioAssets.py`

Imported scream sources:

- source page: `https://opengameart.org/content/female-high-pitched-scream-sfx`
- local source: `ThirdParty/OpenGameArt/FemaleHighPitchedScream/screams.ogg`
- license: CC0
- source page: `https://opengameart.org/content/horror-scream1`
- local source: `ThirdParty/OpenGameArt/HorrorScream1/scream_horror1.mp3`
- license: CC0
- processed asset: `Content/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.uasset`

## Reimport

After changing downloaded source maps, re-run:

```powershell
& 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\MainGame\BlackoutHunt.uproject' -ExecutePythonScript='D:\MainGame\Tools\ImportAmbientCGAssets.py' -unattended -nop4 -nosplash
```

The import script replaces the project-local material assets and saves `/Game/BlackoutHunt`.

After adding or replacing the Kenney Nature Kit package, run:

```powershell
& 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\MainGame\BlackoutHunt.uproject' -ExecutePythonScript='D:\MainGame\Tools\ImportFoggroundsAssets.py' -unattended -nop4 -nosplash
```

After changing generated audio source settings, run:

```powershell
.\Tools\GenerateAudioSources.ps1
& 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\MainGame\BlackoutHunt.uproject' -ExecutePythonScript='D:\MainGame\Tools\ImportAudioAssets.py' -unattended -nop4 -nosplash
```
