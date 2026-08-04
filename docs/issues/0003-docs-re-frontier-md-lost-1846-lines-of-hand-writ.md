---
id: 3
title: docs/re-frontier.md lost 1846 lines of hand-written rationale when re_frontier.py first machine-edited it
status: open
symptom: git diff docs/re-frontier.md shows -1868/+107; the file is 218 lines where HEAD has 1979
tags: workflow,tooling,re-frontier,data-loss
created: 2026-08-04
updated: 2026-08-04
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-04)
NOT caused by the 2026-08-04 base-relative session: that session's first re_frontier.py call was a read-only `show HACK-02`, and HACK-02 already parsed in the machine `- field: value` format, which means the conversion had already happened. It was the PREVIOUS session, when HACK-02 was first registered.

WHAT WAS LOST: re_frontier.py re-serialises only what its parser understood — `### <id> — <title>` headings plus `- <field>: <value>` lines. The old file carried multi-paragraph prose under each step (why a value is what it is, corrections with dates, 'expires if' conditions) which the parser never saw and the writer therefore dropped. The evidence fields survive; the reasoning behind them does not.

RECOVERABLE: yes, entirely — `git show HEAD:docs/re-frontier.md`. Nothing is gone from history.

THE DEFECT IS THE TOOL, not the file: a machine editor that silently discards everything it cannot parse will do this again to any repo that mixes prose with entries. Until re_frontier.py either preserves unparsed text or refuses to write over it, treat `set`/`add` on a prose-bearing roadmap as destructive. Left for the OPERATOR to decide: keep the machine-readable file (which is what finally made `hacks` able to see HACK-02 at all — the prose version reported 'no hacks tracked' while a hack shipped) or merge the prose back.
