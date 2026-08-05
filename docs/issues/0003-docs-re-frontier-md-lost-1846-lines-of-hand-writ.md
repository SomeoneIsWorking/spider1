---
id: 3
title: docs/re-frontier.md lost 1846 lines of hand-written rationale when re_frontier.py first machine-edited it
status: resolved
symptom: git diff docs/re-frontier.md shows -1868/+107; the file is 218 lines where HEAD has 1979
tags: workflow,tooling,re-frontier,data-loss
created: 2026-08-04
updated: 2026-08-05
---

## Root cause

**The writer was a REGENERATOR, not an editor.** `save()` (old `tools/re_frontier.py:149`) discarded
the file and re-emitted it from the parsed model: a fixed `HEADER` constant, then one
`Entry.serialize()` per entry (heading + the six schema fields). Anything the parser had not put
into that model — every paragraph of rationale, every dated correction, every `expires if`, the
file's own header, and even a hand-added `- where-2:` field — existed only in the text that was
thrown away. The loss was therefore not an edge case: it was the writer's normal operation, and it
scaled with how much prose the file carried (1846 lines here).

Two smaller defects fell out of the same read: the in-repo copy still had the INST-14 bug
(`check` on a missing roadmap printed OK and exited 0 — the global skill had been fixed, this copy
never was), and `add` would happily create a roadmap from nothing at a mistyped path.

## What was tried / dead ends

Refuse-on-unparsed was considered and rejected: on the real 1979-line file EVERY entry carries
prose, so a refusing tool refuses always and is a tool nobody can run — which was already the
state the operator was in ("today I hand-edited the file rather than use the tool"). Preservation
had to be the default path, with refusal reserved for a genuine editor bug.

## Resolution

### FIXED 2026-08-05 — writes are in-place surgery, gated on a preservation check

`tools/re_frontier.py` no longer regenerates the file. `load()` keeps the raw lines and records
where each entry heading, field line and block boundary sits; `set` rewrites exactly the field
lines it was told to change (inserting one only when the field is absent), `add` splices a new
block into its area section, and an `area=` change MOVES the entry's whole block — prose included.
Every other byte is copied through.

Every write then passes `_check_preserved()`, which is deliberately independent of the editing
code: it compares multisets of non-blank lines old-vs-new, minus the exact lines the command
intended to replace. On any deficit it prints the lost lines with their old line numbers, writes
NOTHING, and exits 2. Proven to fire: with the editor deliberately sabotaged (an extra
`lines.pop(idx+1)`) it refused and named `old line 32: - evidence: PS-X EXE header…`, and the file
was byte-identical afterwards.

`selftest` (also wired as `tools/test_re_frontier.py`, so `pytest` runs it) drives a prose-bearing
roadmap through a real `set` and `add` as a SUBPROCESS and asserts both that nothing was lost and
that the edits LANDED — the positive control matters, because a tool that refused every write
would pass a loss check trivially.

**Red then green, on the real corpus** (`git show 74af0c6:docs/re-frontier.md`, 1979 lines):

    $ python3 tools/re_frontier.py selftest --tool <old> --corpus scratch/refrontier/prose-1979.md
    set DROPPED 1562 non-blank line(s)   ... exit 1
    $ python3 tools/re_frontier.py selftest --corpus scratch/refrontier/prose-1979.md
    checked 1641 non-blank source lines; 1 field line legitimately replaced; 0 lost ... exit 0

A `set`/`add` sequence on that file now produces a 12-line diff containing only the intended
changes. Read paths are unchanged: `stats`/`list`/`blocked`/`next` are byte-identical between the
old and new tool on both the current 218-line file and the 1979-line prose file.

**STILL OPEN, and it is the OPERATOR's call, not the tool's:** whether to restore the prose version
of `docs/re-frontier.md` (now that editing it is safe) or keep the machine-readable one. This fix
deliberately did not re-author the file — it only made either choice survivable. The recovery is
`git show 74af0c6:docs/re-frontier.md`.

**NOT fixed here:** the six sibling copies of this tool (`~/.claude/skills/re-frontier/`,
spyro, gears1, zelda3d, xmen2, openbl2) still regenerate and will still destroy prose. Only
spider1's copy is in this agent's tree.

### Note (2026-08-04)
NOT caused by the 2026-08-04 base-relative session: that session's first re_frontier.py call was a read-only `show HACK-02`, and HACK-02 already parsed in the machine `- field: value` format, which means the conversion had already happened. It was the PREVIOUS session, when HACK-02 was first registered.

WHAT WAS LOST: re_frontier.py re-serialises only what its parser understood — `### <id> — <title>` headings plus `- <field>: <value>` lines. The old file carried multi-paragraph prose under each step (why a value is what it is, corrections with dates, 'expires if' conditions) which the parser never saw and the writer therefore dropped. The evidence fields survive; the reasoning behind them does not.

RECOVERABLE: yes, entirely — `git show HEAD:docs/re-frontier.md`. Nothing is gone from history.

THE DEFECT IS THE TOOL, not the file: a machine editor that silently discards everything it cannot parse will do this again to any repo that mixes prose with entries. Until re_frontier.py either preserves unparsed text or refuses to write over it, treat `set`/`add` on a prose-bearing roadmap as destructive. Left for the OPERATOR to decide: keep the machine-readable file (which is what finally made `hacks` able to see HACK-02 at all — the prose version reported 'no hacks tracked' while a hack shipped) or merge the prose back.
