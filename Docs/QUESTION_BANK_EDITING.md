# Editing the Physics Question Bank

BlackoutHunt ships with a built-in bank of 368 IGCSE physics questions compiled into the game.
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

   This writes all 368 built-in questions to `Saved/ClassroomPresets/QuestionBank.json` in the
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

Note the strict shipped-bank distribution (368 total, 92 per topic, fixed difficulty and type
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
    "xAxis": "", "yAxis": "", "angleOrShape": 0.0, "imageSoftPath": ""
}
```

How the fields are used depends on the diagram type — examples (see the worked example file for
full questions):

- **VelocityGraph / IVGraph** — `xAxis` / `yAxis` caption the axes; `labelA` adds a note. IVGraph
  also marks a reading point using `valueA`/`valueB` against axis maxima `valueC`/`valueD`.
- **ForceArrows / Circuit / MomentBeam / Sankey / EnergyChain** — `labelA`–`labelD` label the
  arrows / components / boxes with their values (e.g. `"4 N"`, `"2 Ω"`, `"useful 350 J"`).
- **Wave** — `valueA` sets amplitude fraction (0.05–0.45), `valueB` sets the number of cycles
  (1–6); `labelA`/`labelB` caption the wave.
- **RayDiagram** — `angleOrShape` is the incident angle from the normal (5–80 degrees); `labelA`
  captions it.

`imageSoftPath` is optional: set it to a texture object path to draw an illustrated diagram
instead of the procedural schematic (the procedural one is used if the texture cannot be loaded).
All diagram fields are optional — leaving them blank/zero just draws the generic schematic.

Diagram type values: `None`, `MotionGraph`, `VelocityGraph`, `ForceArrows`, `SpringGraph`,
`MomentBeam`, `Circuit`, `IVGraph`, `StaticCharge`, `Wave`, `EMSpectrum`, `RayDiagram`, `Sankey`,
`EnergyChain`.

## Tips

- Keep `id`s stable when you edit a question's text so a student's review history still lines up.
- True/False questions still use four `choices` by convention (e.g. the two extra options can be
  near-misses); set `correctChoiceIndex` to the right one.
- After editing, watch the log on the next load for `Loaded N question(s) from override` to
  confirm your file was accepted, or a validation warning if it fell back to the built-in bank.
