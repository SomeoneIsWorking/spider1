---
id: C058
kind: claim
status: holds
created: 2026-08-27
tags: widescreen,projection,spiderman1
depends: titles/spiderman1/spider1_widescreen.cpp#Spider1Widescreen::publishAndRender, titles/spiderman1/spider1_widescreen.cpp#spider1ProjectViewport
---

## Claim

Spider-Man 1's sole world-render viewport boundary can publish 16:9 before retail culling and projection without changing focal length

## Evidence

Saved-project decompile of FUN_80075D0C shows descriptor bounds [0..3], lens divisor [6], computed H [7], OFX/OFY [8..9], followed by frustum construction and all object draws; retail 0x80098B00 seeds 512x240/divisor2365. Spider1Widescreen latches that native geometry and scales width/divisor together (512/2365 -> 684/3159). spider1_widescreen_test proves native identity, 16:9 result, and opposite bound orientation.

## What would falsify it

A real standard/wide pair changes H, stretches rather than reveals world content, culls valid edge geometry, moves title UI incorrectly, or shows that FUN_80075D0C consumes another descriptor layout.
