# Editing the Physics Question Bank

BlackoutHunt ships with a built-in bank of 376 IGCSE physics questions compiled into the game.
Teachers can **replace** that bank with their own questions by dropping a JSON file into the
project's `Saved` folder. No rebuild is required — the game reads the file at runtime.

This is optional. If no override file is present (or it fails validation), the game uses the
built-in bank, so you can always remove the file to go back to the shipped questions.

## Where the file goes

```text
<Project>/Saved/ClassroomPresets/QuestionBank.json
```

This is the same folder as `LessonPresets.json`. The `Saved` folder is created the first time
you run the game (and is not part of the source tree).

## Two ways to start

1. **Start from the full shipped bank (recommended).** With the game running, open the console
   and run:

   ```text
   bh.ExportQuestionBank
   ```

   This writes all 376 built-in questions to `Saved/ClassroomPresets/QuestionBank.json` in the
   exact format below. Edit that file, then restart the round (or the game) to load your changes.

2. **Start from the worked example.** Copy [QuestionBank.example.json](QuestionBank.example.json)
   (14 questions covering all four topics, every question type, and most diagram types) to the
   override path above and expand it.

> **Important:** the override file *replaces the entire bank* — the game does not merge it with
> the built-in questions. Make sure your file covers every topic you intend to test. If you only
> want to tweak a handful of questions, use option 1 so you keep all 368 as a starting point.

## How the override is loaded

On load the game parses the file and runs **structural** validation. The file is used only if it
passes; otherwise the game logs a warning, backs up the unreadable file to
`QuestionBank.json.invalid-<timestamp>`, and falls back to the built-in bank.

A file passes validation when:

- it has at least one question, and
- **every** question has a non-empty `id` (unique across the file), `prompt`, `hint`, and
  `explanation`, **exactly four** `choices`, and a `correctChoiceIndex` in the range `0`–`3`, and
- there is **at least one question in each of the four topics** (`ForcesAndMotion`, `Electricity`,
  `Waves`, `Energy`).

Note the strict shipped-bank distribution (376 total, 94 per topic, fixed difficulty and type
counts) is **not** applied to teacher content — banks of any size are accepted as long as they are
structurally well-formed.

## File format

The root object has a `version` (currently `1`) and a `questions` array. Each question:

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Unique within the file. Used for the spaced-review queue. |
| `topic` | enum string | One of `ForcesAndMotion`, `Electricity`, `Waves`, `Energy`. |
| `topicName` | string | Display name; if blank the game derives it from `topic`. |
| `subtopic` | string | Short label shown on the question/diagram. |
| `difficulty` | enum string | `Easy`, `Medium`, `Hard`. |
| `type` | enum string | `MultipleChoice`, `TrueFalse`, `Calculation`, `FormulaFill`, `GraphReading`, `DragDropMatching`, `Ordering`. |
| `diagramType` | enum string | `None` or one of the diagram types listed below. |
| `prompt` | string | The question text. |
| `answer.choices` | string[4] | Exactly four answer options. |
| `answer.correctChoiceIndex` | int | `0`–`3`, index of the correct choice. |
| `answer.formula` | string | Optional "key idea" shown with the diagram. |
| `answer.numericAnswer` | number | Optional, for calculation questions. |
| `answer.numericTolerance` | number | Optional accepted +/- range for `numericAnswer`. |
| `diagram` | object | Data-driven visual givens (see below). Omit or leave blank for none. |
| `hint` | string | Shown when a student requests a hint. |
| `correctionPrompt` | string | Shown after a wrong answer. |
| `explanation` | string | The teaching explanation shown with the correction. |
| `masteryWeight` | number | Optional. If absent, derived from difficulty (Easy 1.0 / Medium 1.2 / Hard 1.5). |

Enum values are the names shown above (e.g. `"ForcesAndMotion"`, `"GraphReading"`), not the
display names. Unknown enum strings fall back to a safe default rather than rejecting the file.

### The `diagram` object (data-driven visuals)

When `diagramType` is not `None`, the matching schematic is drawn next to the question and is
labelled from the `diagram` object, so the picture shows *this* question's numbers:

```json
"diagram": {
    "valueA": 0.0, "valueB": 0.0, "valueC": 0.0, "valueD": 0.0,
    "labelA": "", "labelB": "", "labelC": "", "labelD": "",
    "xAxis": "", "yAxis": "", "angleOrShape": 0.0, "shapeVariant": 0, "imageSoftPath": ""
}
```

> **Answer-safety rule:** a diagram shows the question's **givens**, never the answer. Do not put a
> derived/answer value in any label. (The shipped bank is checked for this automatically.)

How the fields are used depends on the diagram type — examples (see the worked example file for
full questions):

- **MotionGraph / VelocityGraph** — `xAxis` / `yAxis` caption the axes. `shapeVariant` picks the
  line shape: `1` constant, `2` accelerating, `3` decelerating, `4` accelerate-then-constant (`0`
  draws a generic increasing line). `labelA`/`labelB` may show given values.
- **IVGraph** — `shapeVariant` `0` ohmic (straight), `1` filament lamp (curve), `2` diode
  (threshold). `xAxis`/`yAxis` caption the axes; `valueA`/`valueB` mark a reading point against
  axis maxima `valueC`/`valueD`.
- **Circuit** — `shapeVariant` `0` single resistor, `1` series-2, `2` parallel-2, `3` series-3.
  `valueA`/`valueC` are resistances, `valueB` the supply; `labelA`/`labelC` label resistors,
  `labelB` the supply (e.g. `"2 ohm"`, `"6 V"`).
- **ForceArrows** — `valueA`/`valueB` are the two force magnitudes (the arrows scale to them);
  `labelA`/`labelB` label them. `shapeVariant` `1` adds a vertical weight/normal pair for free-body
  questions, captioned by `labelC` (down) / `labelD` (up).
- **MomentBeam / Sankey / EnergyChain / EnergyBars** — `labelA`–`labelD` label the distances,
  forces, flows or boxes with their values. EnergyBars also sizes its bars from `valueA`–`valueD`.
- **Wave** — `valueA` sets amplitude fraction (0.05–0.45), `valueB` the number of cycles (1–6);
  `labelA`/`labelB` caption the wave.
- **EMSpectrum** — `shapeVariant` `1`–`7` highlights the band in question (radio … gamma).
- **RayDiagram / InclinedPlane** — `angleOrShape` is the angle in degrees (incidence / ramp);
  `labelA` captions it.
- **Transformer** — `labelA`/`labelB` label the primary / secondary coils (e.g. `"Np 200"`).
- **SpringGraph / StaticCharge / MagneticField / ParticleModel / Lens / PressureColumn** — mostly
  schematic; caption with `labelA` (and `xAxis`/`yAxis` for the spring graph) as needed.

`imageSoftPath` is optional: set it to a texture object path (e.g. `"/Game/.../T_MyDiagram"`) to
draw an illustrated diagram instead of the procedural schematic (the procedural one is used if the
texture cannot be loaded). All diagram fields are optional — leaving them blank/zero just draws the
generic schematic.

Diagram type values: `None`, `MotionGraph`, `VelocityGraph`, `ForceArrows`, `SpringGraph`,
`MomentBeam`, `Circuit`, `IVGraph`, `StaticCharge`, `Wave`, `EMSpectrum`, `RayDiagram`, `Sankey`,
`EnergyChain`, `Lens`, `Transformer`, `MagneticField`, `InclinedPlane`, `PressureColumn`,
`EnergyBars`, `ParticleModel`.

## Previewing diagrams in-game

To eyeball any diagram type without building a quiz node, open the console and run:

```text
bh.Diagrams.PreviewType 7      ; overlay a sample of EBHDiagramType index 7 (0 = off)
bh.Diagrams.PreviewVariant 1   ; choose a shape variant
bh.DiagramCoverage             ; log per-type data coverage of the built-in bank
bh.Diagrams.Enhanced 0         ; temporarily fall back to plain schematics
```

## Tips

- Keep `id`s stable when you edit a question's text so a student's review history still lines up.
- True/False questions still use four `choices` by convention (e.g. the two extra options can be
  near-misses); set `correctChoiceIndex` to the right one.
- After editing, watch the log on the next load for `Loaded N question(s) from override` to
  confirm your file was accepted, or a validation warning if it fell back to the built-in bank.
