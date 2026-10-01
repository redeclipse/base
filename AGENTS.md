# CSGOpen

Multiplayer FPS prototype based on Red Eclipse. The first milestone is a native
macOS baseline and a separate TDM preset; current settings do not yet reproduce
CS:GO movement or weapons.

- Work only in this repository; preserve the original gameplay.
- The CSGOpen TDM preset enables friendly fire for humans and bots, with a
  team damage multiplier of 1, as requested after the initial milestone.
- Always write and update README files and `AGENTS.md` in English. This includes
  `README.md` and `doc/csgopen/README.md`. Open
  README files with CSGOpen's overall purpose before current milestone details.
- Read `doc/csgopen/README.md`, `doc/csgopen/gameplay.md`, and
  `doc/csgopen/validation.md` before changing the build, launchers, or rules.
  Distinguish executed tests, code inspection, and pending manual checks.
- Use the upstream Makefile and C++ style (four spaces, braces on separate
  lines, existing variable macros and synchronization). Prefer CubeScript.
- Verified macOS commands: `scripts/csgopen/dev.sh check`, `build`, `original`,
  `tdm`, and `server`. Run the explicit smoke test in `config/csgopen/smoke.cfg`
  with the local dedicated server running; check for `SMOKE_DONE FAILURES 0`,
  not just exit code 0.
- Do not hardcode Homebrew prefixes. Do not use `brew upgrade`, Rosetta, or
  XQuartz as default solutions. Preserve Linux and Windows build paths.
- Do not modify assets or maps in submodules. Use recorded commits:
  `git submodule update --init --recursive`, never `--remote`.
- Keep logs, runtime profiles, and local results in `.csgopen/` (Git-ignored).
- Bind test servers to loopback, without public master registration or cloud
  deployment.
- Do not perform destructive Git resets, commits, pushes, or publication unless
  requested.
