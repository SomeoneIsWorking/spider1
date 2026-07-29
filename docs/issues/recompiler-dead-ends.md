# Recompiler dead ends — ruled out, do not re-derive

Negative results about the MIPS→C translation. Each entry exists so a future session does not spend a
tick re-discovering that something is *not* a bug.

---

## DE-01 — "The recompiler ignores branch-likely (beql/bnel/blezl/bgtzl)" — **NOT A DEFECT**

*The worry:* branch-likely instructions NULLIFY their delay slot when the branch is not taken, so
emitting the delay slot unconditionally would be wrong. `decode.py` has no case for opcodes
`0x14`–`0x17`, and a raw scan of the executable's text range found **161** words whose top 6 bits
match them (beql 80, bnel 56, blezl 14, bgtzl 11). That looks damning.

*Why it is not:* **the PSX CPU is an R3000A, which is MIPS-I. Branch-likely is MIPS-II.** The
instructions do not exist on this hardware, so those words cannot be code.

*Confirmed, rather than argued:* none of the candidate addresses is emitted as code. Checking the
generated substrate for a `L_<addr>` label or a `func_<addr>` entry at each of the first six
(`0x80091AF4`, `0x80091B48`, `0x80091C24`, `0x80091C9C`, `0x80091CBC`, `0x80091CEC`) finds nothing,
and they cluster in `0x8009xxxx` — the string/table data region. They are data being read as opcodes
by a scan that swept the whole text range.

*The reusable lesson, which is already a project rule:* **a grep count is text, not code.** Counting
raw word patterns across a text range counts constants, strings and jump tables alongside
instructions. Before treating a count as a defect surface, confirm the addresses are REACHED — an
emitted label, a call site, or a fire counter on a real run. This one was one step from being
reported as a recompiler bug.

*What would reopen this:* a candidate address that IS emitted as code, or a port of a MIPS-II console.
