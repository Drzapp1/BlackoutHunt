# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# Reconstruct a "base + ONLY my map edits" copy of BHGameMode.cpp, so the commit cannot drag in
# the heavy foreign uncommitted WIP that co-exists in this shared working tree. Applies the SAME
# transformations I made (Substation array swaps from substation_codegen.txt + the Facility/shutter
# edits) onto a clean `git show <base>:...` blob, then writes BHGameMode.reconstructed.cpp and
# prints a sanity diff (must be ONLY my changes).

import subprocess
import sys

REPO = r"D:\BlackoutHunt"
BASE = sys.argv[1] if len(sys.argv) > 1 else "HEAD"
REL = "Source/BlackoutHunt/BHGameMode.cpp"
CODEGEN = r"D:\BlackoutHunt\Tools\MapAnalysis\substation_codegen.txt"
OUT = r"D:\BlackoutHunt\Tools\MapAnalysis\BHGameMode.reconstructed.cpp"


def git_show(rev_path):
    r = subprocess.run(["git", "-C", REPO, "show", rev_path], capture_output=True)
    if r.returncode != 0:
        sys.exit("git show failed: " + r.stderr.decode("utf-8", "replace"))
    return r.stdout.decode("utf-8")  # blob is LF; keep \n


def block(text, decl):
    i = text.index(decl)
    ls = text.rfind("\n", 0, i) + 1
    je = text.index("\n\t};", i)
    return text[ls:je + len("\n\t};")]


def replace_block(body, decl, new_block):
    i = body.index(decl)
    ls = body.rfind("\n", 0, i) + 1
    je = body.index("\n\t};", i)
    return body[:ls] + new_block + body[je + len("\n\t};"):]


def must_replace(text, old, new, label):
    if old not in text:
        sys.exit(f"RECONSTRUCT FAILED: anchor not found for '{label}'")
    return text.replace(old, new, 1)


def main():
    cg = open(CODEGEN, encoding="utf-8").read()
    new_walls = block(cg, "TArray<TPair<FVector, FVector>> Walls = {")
    new_doors = block(cg, "TArray<TPair<FVector, FRotator>> Doors = {")
    new_breakers = block(cg, "TArray<TPair<FVector, FRotator>> Breakers = {")
    new_stations = block(cg, "ObjectiveStationSpecs = {")
    c0 = cg.index("\tSpawnCrawlGate(-2100.0f, 2500.0f")
    c1 = cg.index("\n", cg.index("SpawnCrawlGate(2300.0f, -2500.0f"))
    new_crawls = cg[c0:c1]

    src = git_show(f"{BASE}:{REL}")
    fn = "void ABHGameMode::BuildSubstationLevel()"
    fs = src.index(fn)
    fe = src.index("\nvoid ABHGameMode::", fs + len(fn))
    head, body, tail = src[:fs], src[fs:fe], src[fe:]

    # Substation array swaps (scoped to the function body)
    body = replace_block(body, "TArray<TPair<FVector, FVector>> Walls = {", new_walls)
    body = replace_block(body, "TArray<TPair<FVector, FRotator>> Doors = {", new_doors)
    body = replace_block(body, "TArray<TPair<FVector, FRotator>> Breakers = {", new_breakers)
    body = replace_block(body, "ObjectiveStationSpecs = {", new_stations)
    oc0 = body.index("\tSpawnCrawlGate(-2500.0f, -2200.0f")
    oc1 = body.index("\n", body.index("SpawnCrawlGate(1675.0f, -2500.0f"))
    body = body[:oc0] + new_crawls + body[oc1:]

    # design comment refresh
    old_comment = (
        "\t// ---- OPEN-UP redesign: short wall segments arranged so EVERY room has 2-3 entrances (loops, no dead-ends),\n"
        "\t// with a continuous west cable gallery (column at X<-4500 left open) and a big open central transformer hall\n"
        "\t// (X=-500 and Y=0 dividers removed across the middle for long sightlines). Generated + connectivity-verified\n"
        "\t// in Tools/MapAnalysis/substation_proposed.py (0 single-entrance rooms, down from 14). ----\n"
    )
    new_comment = (
        "\t// ---- FLOW REDESIGN (Tools/MapAnalysis/layout_substation.py, verified by bh_mapcheck -> substation_after.png):\n"
        "\t// 5 vertical ribs + 2 horizontal band walls, with an OPEN Y=0 spine running the whole map width so the 'Power'\n"
        "\t// compass bearing matches a real run. Every room has >=2 wide arches (loops, no dead-ends); the west cable\n"
        "\t// gallery (X<-4500) stays open top-to-bottom. Breakers are PRE-SORTED near->far from spawn so the first power\n"
        "\t// beat is a fair, close objective (NOT the far hunter-den corner). 0 unreachable objectives (was: beat #0 in\n"
        "\t// the hunter's spawn room across the map). Crawls are hall<->band flanks offset from the centre doors. ----\n"
    )
    body = must_replace(body, old_comment, new_comment, "design comment")

    # shutters + terminals onto open rib hall-arches
    body = must_replace(body,
        "\tfor (const FVector& Location : {FVector(-2500.0f, -1250.0f, 120.0f), FVector(1800.0f, 1250.0f, 120.0f), FVector(3900.0f, 350.0f, 120.0f)})\n",
        "\tfor (const FVector& Location : {FVector(-2500.0f, -1700.0f, 120.0f), FVector(1800.0f, 1700.0f, 120.0f), FVector(3900.0f, -1700.0f, 120.0f)})  // open rib hall-arches (Y=+/-1700 gaps)\n",
        "shutter locations")
    body = must_replace(body,
        "FVector(-2700.0f, -1250.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)",
        "FVector(-2300.0f, -1700.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)", "terminal 1")
    body = must_replace(body,
        "FVector(4000.0f, 350.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)",
        "FVector(4100.0f, -1700.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)", "terminal 2")

    src2 = head + body + tail

    # --- Facility edits (in BuildBackroomsFacility, elsewhere in the file) ---
    src2 = must_replace(src2,
        "\tconst float CrawlSpecs[16][4] = {\n"
        "\t\t{ 4.0f, 4.0f, 30.0f, 5.0f }, { 9.0f, 3.0f, 90.0f, 4.8f }, { 14.0f, 5.0f, 150.0f, 5.2f },\n"
        "\t\t{ 20.0f, 4.0f, 60.0f, 4.6f }, { 23.0f, 9.0f, 90.0f, 5.0f }, { 3.0f, 9.0f, 0.0f, 5.2f },\n"
        "\t\t{ 8.0f, 10.0f, 45.0f, 4.8f }, { 13.0f, 11.0f, 120.0f, 5.0f }, { 18.0f, 12.0f, 75.0f, 4.8f },\n"
        "\t\t{ 22.0f, 13.0f, 30.0f, 4.6f }, { 5.0f, 15.0f, 90.0f, 5.0f }, { 10.0f, 17.0f, 135.0f, 5.2f },\n"
        "\t\t{ 15.0f, 16.0f, 15.0f, 4.8f }, { 19.0f, 18.0f, 60.0f, 5.0f }, { 12.0f, 8.0f, 105.0f, 4.8f },\n"
        "\t\t{ 6.0f, 12.0f, 160.0f, 5.0f }\n"
        "\t};",
        "\t// DENSER crawl network (16 -> 26): crawl tunnels are ADDITIVE prone passages (they only ever add a\n"
        "\t// route, never seal the hall), so densifying is zero-risk for connectivity. The last 10 are LONGER\n"
        "\t// (lenScale ~6.0-6.5) so the standing hunter's detour around them is bigger -- a real shortcut/escape\n"
        "\t// advantage, not just a hidey-hole. A prone survivor inside is uncapturable (crawl-space immunity).\n"
        "\tconst float CrawlSpecs[26][4] = {\n"
        "\t\t{ 4.0f, 4.0f, 30.0f, 5.0f }, { 9.0f, 3.0f, 90.0f, 4.8f }, { 14.0f, 5.0f, 150.0f, 5.2f },\n"
        "\t\t{ 20.0f, 4.0f, 60.0f, 4.6f }, { 23.0f, 9.0f, 90.0f, 5.0f }, { 3.0f, 9.0f, 0.0f, 5.2f },\n"
        "\t\t{ 8.0f, 10.0f, 45.0f, 4.8f }, { 13.0f, 11.0f, 120.0f, 5.0f }, { 18.0f, 12.0f, 75.0f, 4.8f },\n"
        "\t\t{ 22.0f, 13.0f, 30.0f, 4.6f }, { 5.0f, 15.0f, 90.0f, 5.0f }, { 10.0f, 17.0f, 135.0f, 5.2f },\n"
        "\t\t{ 15.0f, 16.0f, 15.0f, 4.8f }, { 19.0f, 18.0f, 60.0f, 5.0f }, { 12.0f, 8.0f, 105.0f, 4.8f },\n"
        "\t\t{ 6.0f, 12.0f, 160.0f, 5.0f },\n"
        "\t\t// --- denser pass: 10 longer through-tunnels woven across the mid-hall ---\n"
        "\t\t{ 6.0f, 6.0f, 120.0f, 6.2f }, { 11.0f, 6.0f, 45.0f, 6.0f }, { 16.0f, 7.0f, 90.0f, 6.4f },\n"
        "\t\t{ 21.0f, 7.0f, 135.0f, 6.0f }, { 4.0f, 13.0f, 60.0f, 6.2f }, { 9.0f, 13.0f, 0.0f, 6.5f },\n"
        "\t\t{ 16.0f, 14.0f, 105.0f, 6.0f }, { 20.0f, 16.0f, 150.0f, 6.2f }, { 13.0f, 17.0f, 75.0f, 6.0f },\n"
        "\t\t{ 7.0f, 16.0f, 30.0f, 6.4f }\n"
        "\t};",
        "Facility CrawlSpecs")
    src2 = must_replace(src2,
        "\t\t\tCrawl->Configure(FVector(lenScale * 50.0f + 40.0f, 95.0f, 110.0f));\n\t\t}\n\t};\n\tint32 CrawlCount = 0;",
        "\t\t\t// Y half-extent 105 (>= the side walls at +/-100) so the whole between-walls passage is inside the\n"
        "\t\t\t// shelter volume -- a prone survivor anywhere in the duct is uncapturable, with no edge margin where\n"
        "\t\t\t// the Teacher could reach in. (Was 95, which left a 5u sliver outside the immunity volume.)\n"
        "\t\t\tCrawl->Configure(FVector(lenScale * 50.0f + 40.0f, 105.0f, 110.0f));\n\t\t}\n\t};\n\tint32 CrawlCount = 0;",
        "Facility edge margin")

    open(OUT, "w", encoding="utf-8", newline="").write(src2)
    print("wrote", OUT, f"({len(src2)} bytes)")

    # sanity diff vs clean base
    base_tmp = OUT + ".base"
    open(base_tmp, "w", encoding="utf-8", newline="").write(src)
    d = subprocess.run(["git", "-C", REPO, "diff", "--no-index", "--stat", base_tmp, OUT], capture_output=True, text=True)
    print(d.stdout or d.stderr)


if __name__ == "__main__":
    main()
