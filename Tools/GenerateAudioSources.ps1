$ErrorActionPreference = "Stop"

$sourceDir = Join-Path $PSScriptRoot "..\Content\BlackoutHunt\Audio\Sources"
New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null

$code = @"
using System;
using System.IO;

public static class BlackoutHuntAudioSources
{
    private const int SampleRate = 44100;
    private const int ChannelCount = 2;
    private const double TwoPi = Math.PI * 2.0;

    public static void WriteLobbyLoop(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        const int seconds = 64;
        int frames = SampleRate * seconds;

        using (FileStream stream = File.Create(path))
        using (BinaryWriter writer = new BinaryWriter(stream))
        {
            WriteHeader(writer, frames, ChannelCount);

            uint seed = 0x8D2A4F1Bu;
            double smoothedNoise = 0.0;

            for (int i = 0; i < frames; ++i)
            {
                double t = i / (double)SampleRate;
                seed = seed * 1664525u + 1013904223u;
                double rawNoise = (((seed >> 8) & 0xffffu) / 32767.5) - 1.0;
                smoothedNoise += (rawNoise - smoothedNoise) * 0.0016;

                double pulse = 0.70 + 0.30 * Math.Pow((Math.Sin(TwoPi * (4.0 / seconds) * t - 0.8) + 1.0) * 0.5, 3.0);
                double breath = 0.58 + 0.42 * ((Math.Sin(TwoPi * (2.0 / seconds) * t + 1.1) + 1.0) * 0.5);

                double lowLeft =
                    DroneVoice(t, 28.00, 0.00, 2.0 / seconds) * 0.36 +
                    DroneVoice(t, 30.50, 0.37, 3.0 / seconds) * 0.27 +
                    DroneVoice(t, 41.00, 0.18, 4.0 / seconds) * 0.17;
                double lowRight =
                    DroneVoice(t, 28.00, 0.31, 2.0 / seconds) * 0.35 +
                    DroneVoice(t, 30.50, 0.09, 3.0 / seconds) * 0.28 +
                    DroneVoice(t, 41.00, 0.63, 4.0 / seconds) * 0.17;

                double dreadLeft =
                    RoughPadVoice(t, 72.00, 0.91, 5.0 / seconds) * 0.105 +
                    RoughPadVoice(t, 88.50, 1.73, 7.0 / seconds) * 0.068 +
                    DroneVoice(t, 124.00, 2.21, 6.0 / seconds) * 0.030;
                double dreadRight =
                    RoughPadVoice(t, 72.00, 1.31, 5.0 / seconds) * 0.102 +
                    RoughPadVoice(t, 88.50, 1.02, 7.0 / seconds) * 0.071 +
                    DroneVoice(t, 124.00, 2.79, 6.0 / seconds) * 0.030;

                double scrapeLeft = 0.0;
                double scrapeRight = 0.0;

                double impactLeft =
                    DistantImpact(t, 14.0, 0.52) +
                    DistantImpact(t, 29.5, 0.38) +
                    DistantImpact(t, 45.2, 0.46) +
                    DistantImpact(t, 58.5, 0.31);
                double impactRight =
                    DistantImpact(t, 14.1, 0.42) +
                    DistantImpact(t, 29.6, 0.50) +
                    DistantImpact(t, 45.3, 0.36) +
                    DistantImpact(t, 58.6, 0.44);

                double clangLeft = 0.0;
                double clangRight = 0.0;

                double air = smoothedNoise * 0.040 + rawNoise * 0.0018;

                double edgeFade = Math.Min(1.0, Math.Min(t / 0.35, (seconds - t) / 0.35));
                double gain = 0.78 * Math.Max(0.0, edgeFade);

                double left = SoftClip(((lowLeft * pulse) + (dreadLeft * breath) + scrapeLeft + impactLeft + clangLeft + air) * gain);
                double right = SoftClip(((lowRight * pulse) + (dreadRight * breath) + scrapeRight + impactRight + clangRight + air * 0.86) * gain);
                WriteSample(writer, left);
                WriteSample(writer, right);
            }
        }
    }

    private static double DroneVoice(double t, double frequency, double phase, double wobbleFrequency)
    {
        double wobble =
            Math.Sin(TwoPi * wobbleFrequency * t + phase) * 0.045 +
            Math.Sin(TwoPi * wobbleFrequency * 0.37 * t + phase * 0.71) * 0.025;
        double core = Math.Sin(TwoPi * frequency * t + wobble + phase);
        double overtone = Math.Sin(TwoPi * frequency * 1.997 * t + wobble * 0.35 + phase * 0.47) * 0.18;
        double grit = Math.Sin(TwoPi * frequency * 3.07 * t + phase * 0.19) * 0.035;
        return core + overtone + grit;
    }

    private static double RoughPadVoice(double t, double frequency, double phase, double wobbleFrequency)
    {
        double beat = 0.62 + 0.38 * ((Math.Sin(TwoPi * wobbleFrequency * t + phase) + 1.0) * 0.5);
        double a = Math.Sin(TwoPi * frequency * t + phase);
        double b = Math.Sin(TwoPi * (frequency + 1.35) * t + phase * 0.37) * 0.72;
        double c = Math.Sin(TwoPi * (frequency * 0.505 + 0.55) * t + phase * 1.41) * 0.23;
        return (a + b + c) * beat;
    }

    private static double CreakVoice(double t, double start, double duration, double frequency, double level)
    {
        double x = t - start;
        if (x < 0.0 || x > duration)
        {
            return 0.0;
        }

        double attack = Math.Min(1.0, x / 1.1);
        double release = Math.Min(1.0, (duration - x) / 1.7);
        double pressure = 0.36 + 0.64 * Math.Pow((Math.Sin(TwoPi * 0.37 * x + 0.8) + 1.0) * 0.5, 2.0);
        double scrape =
            Math.Sin(TwoPi * frequency * x + Math.Sin(TwoPi * 0.12 * x) * 0.65) * 0.48 +
            Math.Sin(TwoPi * frequency * 1.37 * x + 0.9) * 0.28 +
            Math.Sin(TwoPi * frequency * 0.61 * x + 1.7) * 0.22;
        return scrape * attack * release * pressure * level * 0.12;
    }

    private static double HeavyMovingScrape(double t, double start, double duration, double baseFrequency, double level, double seed)
    {
        double x = t - start;
        if (x < 0.0 || x > duration)
        {
            return 0.0;
        }

        double attack = Math.Min(1.0, x / 0.9);
        double release = Math.Min(1.0, (duration - x) / 1.35);
        double motion = x / duration;
        double drag = 0.55 + 0.45 * Math.Pow((Math.Sin(TwoPi * (0.42 + motion * 0.22) * x + seed * 9.0) + 1.0) * 0.5, 2.0);
        double gritNoise =
            SmoothNoise(x, 55.0, seed) * 0.30 +
            SmoothNoise(x, 160.0, seed + 0.37) * 0.13 +
            SmoothNoise(x, 480.0, seed + 0.71) * 0.045;
        double grindFrequency = baseFrequency + Math.Sin(TwoPi * 0.075 * x + seed * 6.0) * 12.0 + motion * 8.0;
        double grind =
            Math.Sin(TwoPi * grindFrequency * x + SmoothNoise(x, 10.0, seed) * 0.9) * 0.30 +
            Math.Sin(TwoPi * grindFrequency * 1.87 * x + 1.4) * 0.18 +
            Math.Sin(TwoPi * grindFrequency * 0.49 * x + 0.8) * 0.24;
        double chatter = Math.Sin(TwoPi * (7.0 + motion * 2.4) * x + seed * 3.0) > 0.72 ? 0.16 : 0.0;
        return (grind + gritNoise + chatter) * attack * release * drag * level * 0.25;
    }

    private static double DistantImpact(double t, double start, double level)
    {
        double x = t - start;
        if (x < 0.0 || x > 4.0)
        {
            return 0.0;
        }

        double attack = Math.Min(1.0, x / 0.05);
        double decay = Math.Exp(-x * 1.35);
        double body =
            Math.Sin(TwoPi * 42.0 * x) * 0.78 +
            Math.Sin(TwoPi * 67.0 * x + 0.4) * 0.25 +
            Math.Sin(TwoPi * 93.0 * x + 1.1) * 0.10;
        return body * attack * decay * level * 0.34;
    }

    private static double PipeHit(double t, double start, double level, double seed)
    {
        double x = t - start;
        if (x < 0.0 || x > 1.35)
        {
            return 0.0;
        }

        double first = PipeStrike(x, level, seed);
        double second = PipeStrike(x - 0.032 - seed * 0.010, level * 0.74, seed + 0.43);
        double afterScrape = SmoothNoise(x, 6100.0, seed + 0.91) * Math.Exp(-x * 16.0) * level * 0.12;
        return first + second + afterScrape;
    }

    private static double PipeStrike(double x, double level, double seed)
    {
        if (x < 0.0 || x > 1.05)
        {
            return 0.0;
        }

        double attack = Math.Min(1.0, x / 0.0018);
        double transient =
            SmoothNoise(x, 9200.0, seed) * Math.Exp(-x * 135.0) * 0.72 +
            SmoothNoise(x, 4100.0, seed + 0.17) * Math.Exp(-x * 85.0) * 0.34;
        double bite =
            Math.Sin(TwoPi * (760.0 + seed * 43.0) * x + 0.3) * Math.Exp(-x * 12.0) * 0.24 +
            Math.Sin(TwoPi * (1180.0 + seed * 79.0) * x + 1.1) * Math.Exp(-x * 16.0) * 0.22 +
            Math.Sin(TwoPi * (2050.0 + seed * 131.0) * x + 1.9) * Math.Exp(-x * 21.0) * 0.17 +
            Math.Sin(TwoPi * (3360.0 + seed * 173.0) * x + 0.7) * Math.Exp(-x * 28.0) * 0.11 +
            Math.Sin(TwoPi * (4820.0 + seed * 211.0) * x + 2.3) * Math.Exp(-x * 36.0) * 0.07;
        double dryClack =
            Math.Sin(TwoPi * (520.0 + seed * 31.0) * x + 0.2) * Math.Exp(-x * 18.0) * 0.16 +
            Math.Sin(TwoPi * (910.0 + seed * 57.0) * x + 1.6) * Math.Exp(-x * 22.0) * 0.13;
        return (transient + bite + dryClack) * attack * level * 0.74;
    }

    private static double SmoothNoise(double t, double rate, double seed)
    {
        double p = t * rate + seed * 997.0;
        int i = (int)Math.Floor(p);
        double f = p - i;
        double s = f * f * (3.0 - 2.0 * f);
        return Lerp(HashNoise(i), HashNoise(i + 1), s);
    }

    private static double HashNoise(int n)
    {
        unchecked
        {
            uint x = (uint)n;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            return (x / 2147483647.5) - 1.0;
        }
    }

    private static double Lerp(double a, double b, double t)
    {
        return a + (b - a) * t;
    }

    private static double SoftClip(double value)
    {
        return Math.Tanh(value * 1.1) / Math.Tanh(1.1);
    }

    public static void WriteMenuClick(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        int frames = (int)(SampleRate * 0.13);

        using (FileStream stream = File.Create(path))
        using (BinaryWriter writer = new BinaryWriter(stream))
        {
            WriteHeader(writer, frames, ChannelCount);

            for (int i = 0; i < frames; ++i)
            {
                double t = i / (double)SampleRate;
                double attack = Math.Min(1.0, t / 0.004);
                double decay = Math.Exp(-t * 34.0);
                double tail = Math.Exp(-t * 15.0);
                double envelope = attack * decay;
                double lowEnvelope = attack * tail;

                double tick =
                    Math.Sin(TwoPi * 880.0 * t) * 0.48 +
                    Math.Sin(TwoPi * 1320.0 * t + 0.31) * 0.16 +
                    Math.Sin(TwoPi * 318.0 * t) * 0.16 * lowEnvelope;
                double clickNoise = (((i * 1103515245u + 12345u) >> 16) & 0x7ffu) / 1023.5 - 1.0;
                double sample = (tick * envelope + clickNoise * envelope * 0.035) * 0.58;

                WriteSample(writer, sample);
                WriteSample(writer, sample * 0.86);
            }
        }
    }

    private static void WriteHeader(BinaryWriter writer, int frames, int channels)
    {
        int bitsPerSample = 16;
        int byteRate = SampleRate * channels * bitsPerSample / 8;
        short blockAlign = (short)(channels * bitsPerSample / 8);
        int dataBytes = frames * channels * bitsPerSample / 8;

        writer.Write(new[] { 'R', 'I', 'F', 'F' });
        writer.Write(36 + dataBytes);
        writer.Write(new[] { 'W', 'A', 'V', 'E' });
        writer.Write(new[] { 'f', 'm', 't', ' ' });
        writer.Write(16);
        writer.Write((short)1);
        writer.Write((short)channels);
        writer.Write(SampleRate);
        writer.Write(byteRate);
        writer.Write(blockAlign);
        writer.Write((short)bitsPerSample);
        writer.Write(new[] { 'd', 'a', 't', 'a' });
        writer.Write(dataBytes);
    }

    private static void WriteSample(BinaryWriter writer, double value)
    {
        value = Math.Max(-1.0, Math.Min(1.0, value));
        writer.Write((short)Math.Round(value * short.MaxValue));
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp

[string]$baseLoopPath = Join-Path $sourceDir "BH_EerieLobbyLoop_Base.wav"
[string]$finalLoopPath = Join-Path $sourceDir "BH_EerieLobbyLoop.wav"
[string]$screamSourcePath = Join-Path $PSScriptRoot "..\ThirdParty\OpenGameArt\HorrorScream1\scream_horror1.mp3"
[string]$fallbackScreamSourcePath = Join-Path $PSScriptRoot "..\ThirdParty\OpenGameArt\FemaleHighPitchedScream\screams.ogg"
[string]$screamProcessedPath = Join-Path $sourceDir "BH_TerrifiedScream_Faint.wav"
[string]$hollowPipeHitsPath = Join-Path $sourceDir "freesound_community-hollow-metal-pipe-hits-73613.mp3"
[string]$longMetalScrapePath = Join-Path $sourceDir "freesound_community-long-metal-scrape-03-103265.mp3"
[string]$hitMetalPipePath = Join-Path $sourceDir "soundreality-hit-metal-pipe-399088.mp3"
[string]$horrorNotePath = Join-Path $sourceDir "soundreality-horror-note-268362.mp3"
[string]$scorePointPath = Join-Path $sourceDir "universfield-horror-game-score-point-117202.mp3"
[string]$darkAtmospherePath = Join-Path $sourceDir "Sound Effects - Royalty-free download for video - Epidemic Sound_2.mp3"
[string]$eerieSwellPath = Join-Path $sourceDir "Sound Effects - Royalty-free download for video - Epidemic Sound_3.mp3"
[string]$eerieTexturePath = Join-Path $sourceDir "Sound Effects - Royalty-free download for video - Epidemic Sound.mp3"
[string]$ghostWhispersPath = Join-Path $sourceDir "Sound Effects - Royalty-free download for videok - Epidemic Sound_2.mp3"
[string]$jumpscarePipePath = Join-Path $sourceDir "u_8t57ikf46v-metal-pipe-230698.mp3"

[BlackoutHuntAudioSources]::WriteLobbyLoop($baseLoopPath)
[BlackoutHuntAudioSources]::WriteMenuClick((Join-Path $sourceDir "BH_MenuClick.wav"))

$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (-not (Test-Path $screamSourcePath) -and (Test-Path $fallbackScreamSourcePath)) {
    $screamSourcePath = $fallbackScreamSourcePath
}

function Format-FfmpegNumber {
    param([double]$Value)
    return $Value.ToString("0.###", [System.Globalization.CultureInfo]::InvariantCulture)
}

function Add-MixLayer {
    param(
        [System.Collections.ArrayList]$Inputs,
        [System.Collections.ArrayList]$Filters,
        [System.Collections.ArrayList]$Labels,
        [string]$Path,
        [string]$Label,
        [int]$DelayMs,
        [double]$Volume,
        [double]$TrimStart = 0.0,
        [double]$Duration = 0.0,
        [int]$HighPass = 20,
        [int]$LowPass = 16000,
        [double]$FadeIn = 0.02,
        [double]$FadeOut = 0.12,
        [double]$TailSeconds = 0.0,
        [double]$TailFade = 0.8,
        [string]$Echo = ""
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Warning "Ambient layer source missing: $Path"
        return
    }

    [int]$inputIndex = $Inputs.Count
    [void]$Inputs.Add($Path)

    [string]$trim = "atrim=start=$(Format-FfmpegNumber $TrimStart)"
    if ($Duration -gt 0.0) {
        $trim += ":duration=$(Format-FfmpegNumber $Duration)"
    }

    [string]$chain = "[{0}:a]{1},asetpts=PTS-STARTPTS,aresample=44100,aformat=channel_layouts=stereo" -f $inputIndex, $trim
    if ($HighPass -gt 0) {
        $chain += ",highpass=f=$HighPass"
    }
    if ($LowPass -gt 0) {
        $chain += ",lowpass=f=$LowPass"
    }
    $chain += ",volume=$(Format-FfmpegNumber $Volume)"
    if ($FadeIn -gt 0.0) {
        $chain += ",afade=t=in:st=0:d=$(Format-FfmpegNumber $FadeIn)"
    }
    if ($Duration -gt 0.0 -and $FadeOut -gt 0.0) {
        [double]$fadeStart = [Math]::Max(0.0, $Duration - $FadeOut)
        $chain += ",afade=t=out:st=$(Format-FfmpegNumber $fadeStart):d=$(Format-FfmpegNumber $FadeOut)"
    }
    if ($TailSeconds -gt 0.0) {
        $chain += ",apad=pad_dur=$(Format-FfmpegNumber $TailSeconds)"
    }
    if (-not [string]::IsNullOrWhiteSpace($Echo)) {
        $chain += ",aecho=$Echo"
    }
    if ($Duration -gt 0.0 -and $TailSeconds -gt 0.0) {
        [double]$totalDuration = $Duration + $TailSeconds
        [double]$tailFadeDuration = [Math]::Min($TailFade, $totalDuration)
        [double]$tailFadeStart = [Math]::Max(0.0, $totalDuration - $tailFadeDuration)
        $chain += ",atrim=0:$(Format-FfmpegNumber $totalDuration),afade=t=out:st=$(Format-FfmpegNumber $tailFadeStart):d=$(Format-FfmpegNumber $tailFadeDuration)"
    }
    $chain += ",adelay=$DelayMs|$DelayMs[$Label]"

    [void]$Filters.Add($chain)
    [void]$Labels.Add("[$Label]")
}

if ($ffmpeg) {
    $mixInputs = [System.Collections.ArrayList]::new()
    $mixFilters = [System.Collections.ArrayList]::new()
    $mixLabels = [System.Collections.ArrayList]::new()
    [void]$mixInputs.Add($baseLoopPath)
    [void]$mixLabels.Add("[0:a]")

    if (Test-Path $screamSourcePath) {
        & $ffmpeg.Source -y -i $screamSourcePath `
            -filter_complex "[0:a]atrim=start=0.20:duration=4.80,asetpts=PTS-STARTPTS,highpass=f=85,lowpass=f=7600,volume=0.95,asplit=3[base][low][rasp];[low]asetrate=38200,aresample=44100,volume=0.22,adelay=80|80[lowp];[rasp]highpass=f=850,lowpass=f=4400,volume=0.16,adelay=32|32[raspp];[base][lowp][raspp]amix=inputs=3:duration=longest:normalize=0,aecho=0.76:0.50:760|1390:0.26|0.17,apad=pad_dur=4.4,atrim=0:8.6,afade=t=in:st=0:d=0.04,afade=t=out:st=1.85:d=6.75,volume=0.32,alimiter=limit=0.92,aformat=sample_fmts=s16:channel_layouts=stereo[out]" -map "[out]" -map_metadata -1 -map_chapters -1 `
            $screamProcessedPath | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "ffmpeg failed while processing scream source."
        }

        Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $screamProcessedPath -Label "scream" -DelayMs 24800 -Volume 0.96 -TrimStart 0.0 -Duration 5.62 -HighPass 20 -LowPass 12000 -FadeIn 0.35 -FadeOut 4.4 -TailSeconds 1.8 -TailFade 1.6 -Echo "0.62:0.26:940|1820:0.14|0.08"
    } else {
        Write-Warning "Imported scream source was not found at $screamSourcePath; generated lobby loop without scream layer."
    }

    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $darkAtmospherePath -Label "darkatmo" -DelayMs 0 -Volume 0.22 -TrimStart 0.0 -Duration 64.0 -HighPass 45 -LowPass 9200 -FadeIn 7.0 -FadeOut 7.0
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $ghostWhispersPath -Label "whispersA" -DelayMs 0 -Volume 0.055 -TrimStart 7.0 -Duration 64.0 -HighPass 260 -LowPass 7200 -FadeIn 8.0 -FadeOut 8.0
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $ghostWhispersPath -Label "whispersB" -DelayMs 30500 -Volume 0.040 -TrimStart 83.0 -Duration 31.0 -HighPass 300 -LowPass 6800 -FadeIn 6.5 -FadeOut 6.5
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $eerieTexturePath -Label "textureA" -DelayMs 2600 -Volume 0.16 -TrimStart 0.0 -Duration 13.0 -HighPass 95 -LowPass 8200 -FadeIn 3.2 -FadeOut 4.0 -TailSeconds 2.6 -TailFade 2.2 -Echo "0.72:0.22:1280|2430:0.12|0.07"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $eerieTexturePath -Label "textureB" -DelayMs 46200 -Volume 0.12 -TrimStart 1.1 -Duration 12.0 -HighPass 100 -LowPass 7800 -FadeIn 4.0 -FadeOut 4.2 -TailSeconds 3.0 -TailFade 2.4 -Echo "0.72:0.20:1320|2540:0.11|0.06"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $eerieSwellPath -Label "swellA" -DelayMs 28600 -Volume 0.14 -TrimStart 0.0 -Duration 13.8 -HighPass 105 -LowPass 9000 -FadeIn 3.8 -FadeOut 4.6 -TailSeconds 3.2 -TailFade 2.6 -Echo "0.70:0.20:1180|2310:0.12|0.06"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $horrorNotePath -Label "horrorNote" -DelayMs 11200 -Volume 0.21 -TrimStart 0.0 -Duration 31.6 -HighPass 45 -LowPass 9800 -FadeIn 5.5 -FadeOut 7.5
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $scorePointPath -Label "scorePoint" -DelayMs 42000 -Volume 0.12 -TrimStart 0.0 -Duration 6.0 -HighPass 100 -LowPass 9500 -FadeIn 1.1 -FadeOut 3.2 -TailSeconds 2.8 -TailFade 2.3 -Echo "0.66:0.23:960|1870:0.13|0.07"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $longMetalScrapePath -Label "scrapeA" -DelayMs 10400 -Volume 0.34 -TrimStart 0.15 -Duration 9.7 -HighPass 120 -LowPass 7600 -FadeIn 2.1 -FadeOut 3.0 -TailSeconds 2.6 -TailFade 2.4 -Echo "0.64:0.24:720|1480|2360:0.16|0.09|0.05"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $longMetalScrapePath -Label "scrapeB" -DelayMs 35000 -Volume 0.28 -TrimStart 1.35 -Duration 8.6 -HighPass 130 -LowPass 7400 -FadeIn 2.4 -FadeOut 3.1 -TailSeconds 2.8 -TailFade 2.4 -Echo "0.64:0.23:760|1510|2390:0.15|0.08|0.05"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $hollowPipeHitsPath -Label "pipeA" -DelayMs 7200 -Volume 0.42 -TrimStart 0.25 -Duration 3.1 -HighPass 160 -LowPass 11800 -FadeIn 0.006 -FadeOut 1.5 -TailSeconds 2.8 -TailFade 2.2 -Echo "0.60:0.25:430|980|1690:0.20|0.12|0.07"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $hitMetalPipePath -Label "pipeB" -DelayMs 22000 -Volume 0.46 -TrimStart 0.0 -Duration 2.3 -HighPass 165 -LowPass 12000 -FadeIn 0.006 -FadeOut 1.4 -TailSeconds 3.0 -TailFade 2.3 -Echo "0.60:0.26:410|940|1640:0.21|0.12|0.07"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $hollowPipeHitsPath -Label "pipeC" -DelayMs 39100 -Volume 0.36 -TrimStart 4.2 -Duration 3.0 -HighPass 160 -LowPass 11600 -FadeIn 0.006 -FadeOut 1.6 -TailSeconds 2.8 -TailFade 2.2 -Echo "0.60:0.24:450|1030|1740:0.19|0.11|0.06"
    Add-MixLayer -Inputs $mixInputs -Filters $mixFilters -Labels $mixLabels -Path $hitMetalPipePath -Label "pipeD" -DelayMs 52500 -Volume 0.38 -TrimStart 2.0 -Duration 2.2 -HighPass 165 -LowPass 11800 -FadeIn 0.006 -FadeOut 1.5 -TailSeconds 3.0 -TailFade 2.2 -Echo "0.60:0.24:420|990|1710:0.19|0.11|0.06"

    if (Test-Path -LiteralPath $jumpscarePipePath) {
        Write-Host "Skipping jumpscare-only pipe asset: $jumpscarePipePath"
    }

    if ($mixLabels.Count -gt 1) {
        $ffmpegArgs = @("-y")
        foreach ($inputPath in $mixInputs) {
            $ffmpegArgs += @("-i", [string]$inputPath)
        }

        [string]$joinedLabels = ($mixLabels | ForEach-Object { [string]$_ }) -join ""
        [string]$finalMix = "${joinedLabels}amix=inputs=$($mixLabels.Count):duration=first:dropout_transition=0:normalize=0,highpass=f=20,acompressor=threshold=-17dB:ratio=1.7:attack=45:release=650,volume=1.05,alimiter=limit=0.94,afade=t=in:st=0:d=2.20,afade=t=out:st=61.20:d=2.80[out]"
        [string]$filterText = (($mixFilters | ForEach-Object { [string]$_ }) + $finalMix) -join ";"

        $ffmpegArgs += @("-filter_complex", $filterText, "-map", "[out]", "-map_metadata", "-1", "-map_chapters", "-1", $finalLoopPath)
        & $ffmpeg.Source @ffmpegArgs | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "ffmpeg failed while mixing imported ambience layers."
        }
    } else {
        Copy-Item -Path $baseLoopPath -Destination $finalLoopPath -Force
    }
} else {
    Copy-Item -Path $baseLoopPath -Destination $finalLoopPath -Force
    Write-Warning "ffmpeg was not found; generated lobby loop without imported ambience layers."
}

Write-Host "Generated Blackout Hunt audio sources in $sourceDir"
