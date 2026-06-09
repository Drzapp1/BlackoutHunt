# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# Splice the verified, code-genned Substation blocks (substation_codegen.txt) into
# BHGameMode::BuildSubstationLevel(). Anchored on declarations + scoped to the function body
# so it cannot touch the other maps' Breakers/ObjectiveStationSpecs arrays.

import sys

TARGET = r"D:\BlackoutHunt\Source\BlackoutHunt\BHGameMode.cpp"
CODEGEN = r"D:\BlackoutHunt\Tools\MapAnalysis\substation_codegen.txt"


def block(text, decl):
    """Return the full 'decl ... \\n\\t};' block (declaration line through its closing brace)."""
    i = text.index(decl)
    ls = text.rfind("\n", 0, i) + 1
    je = text.index("\n\t};", i)
    return text[ls:je + len("\n\t};")]


def replace_block(body, decl, new_block):
    i = body.index(decl)
    ls = body.rfind("\n", 0, i) + 1
    je = body.index("\n\t};", i)
    return body[:ls] + new_block + body[je + len("\n\t};"):]


def main():
    cg = open(CODEGEN, encoding="utf-8").read()
    new_walls = block(cg, "TArray<TPair<FVector, FVector>> Walls = {")
    new_doors = block(cg, "TArray<TPair<FVector, FRotator>> Doors = {")
    new_breakers = block(cg, "TArray<TPair<FVector, FRotator>> Breakers = {")
    new_stations = block(cg, "ObjectiveStationSpecs = {")
    # 5 crawl lines (drop the codegen's header comment; keep the per-line ones)
    c0 = cg.index("\tSpawnCrawlGate(-2100.0f, 2500.0f")
    c1 = cg.index("\n", cg.index("SpawnCrawlGate(2300.0f, -2500.0f"))
    new_crawls = cg[c0:c1]

    src = open(TARGET, encoding="utf-8").read()
    fn = "void ABHGameMode::BuildSubstationLevel()"
    fs = src.index(fn)
    fe = src.index("\nvoid ABHGameMode::", fs + len(fn))
    head, body, tail = src[:fs], src[fs:fe], src[fe:]
    before = body

    body = replace_block(body, "TArray<TPair<FVector, FVector>> Walls = {", new_walls)
    body = replace_block(body, "TArray<TPair<FVector, FRotator>> Doors = {", new_doors)
    body = replace_block(body, "TArray<TPair<FVector, FRotator>> Breakers = {", new_breakers)
    body = replace_block(body, "ObjectiveStationSpecs = {", new_stations)

    # crawl gates: replace the 4 old SpawnCrawlGate lines with the 5 new ones
    oc0 = body.index("\tSpawnCrawlGate(-2500.0f, -2200.0f")
    oc1 = body.index("\n", body.index("SpawnCrawlGate(1675.0f, -2500.0f"))
    body = body[:oc0] + new_crawls + body[oc1:]

    # refresh the stale design comment that referenced the deleted analyzer
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
    if old_comment in body:
        body = body.replace(old_comment, new_comment)
        cstat = "comment refreshed"
    else:
        cstat = "comment NOT found (left as-is)"

    if body == before:
        print("ERROR: no changes applied")
        sys.exit(1)

    open(TARGET + ".bak", "w", encoding="utf-8", newline="").write(src)
    open(TARGET, "w", encoding="utf-8", newline="").write(head + body + tail)

    # report
    print("applied. checks:")
    print("  first breaker now (2850,1900):", "{FVector(2850.0f, 1900.0f, 80.0f)" in body)
    print("  last breaker still (-5650,-3400):", "{FVector(-5650.0f, -3400.0f, 80.0f)" in body)
    print("  band door (4800,2500) present:", "{FVector(4800.0f, 2500.0f, 120.0f)" in body)
    print("  5 new crawl gates:", body.count("SpawnCrawlGate(") - body.count("auto SpawnCrawlGate"))
    print("  old crawl (-2500,-2200) gone:", "SpawnCrawlGate(-2500.0f, -2200.0f" not in body)
    print("  rib spine gap (y=865 seg) present:", "{FVector(-2500.0f, 865.0f, 175.0f)" in body)
    print("  old (-4500,-2400,20.0) wall gone:", "{FVector(-4500.0f, -2400.0f, 175.0f), FVector(0.30f, 20.0f" not in body)
    print(" ", cstat)


if __name__ == "__main__":
    main()
