# Contributing Guide

This document defines the conventions for this repository. The project is
currently solo-developed, but following these rules maintains discipline and
prepares the codebase for future collaborators.

## Branch Strategy (GitFlow lite)

```
main ──────────────●────────────────●────────────────●────  (tags: v0.1, v0.2, ...)
                    │                │                │
dev ──────●─────────●────────●───────●────────────────●────  (integration)
          │                  │
feature/m0-setup  feature/m1-display  (one branch per milestone sub-task)
```

### Rules

- `main` is **always buildable**. Never commit directly to `main`.
- `dev` is the integration branch. PRs from `feature/*` target `dev`.
- `feature/<milestone>-<topic>` branches are short-lived (max 1 week).
- At the end of each milestone, `dev` is merged into `main` and tagged.

### Force push policy

- ❌ **NEVER** `git push --force` or `git push -f` on `main` or `dev`.
- ❌ **NEVER** `git push --force-with-lease` on `main` or `dev` (except in the rare case of recovering from a corrupted state, with team coordination).
- ✅ Force push is **only** allowed on `feature/<your-name>/*` branches that you own exclusively.
- ✅ To "undo" a commit on a shared branch, use `git revert <hash>` (creates a new commit that inverses the changes).

## Multi-Developer Workflow

### Direct commits to dev - not allowed

All changes to `dev` must come via Pull Request from a `feature/<author>/<topic>`
or `fix/<author>/<topic>` branch. Direct commits to `dev` (bypassing PR) are
not allowed because:

- Other collaborators don't get a chance to review before integration.
- The PR diff view (with inline comments) is lost.
- The audit trail for "why this change was made" is weaker.

If you accidentally commit directly to `dev`:
1. Don't push yet - create a feature branch from before your commit, then
  cherry-pick your commit onto it.
2. If you already pushed to `dev`, open an after-the-fact PR (empty diff
  but with description of what was committed) for documentation purposes.
3. Notify collaborators in the team channel.

The branch protection on `main` (PR required) prevents this on `main`,
but `dev` is currently unprotected to keep the workflow agile. Discipline
is required.

### Creating a feature branch

```bash
git checkout dev
git pull origin dev
git checkout -b feature/m0-smoke-test
# ... work, commit ...
git push -u origin feature/m0-smoke-test
# Open PR on GitHub: feature/m0-smoke-test -> dev
```

## Commit Message Format (Conventional Commits)

```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

### Allowed types

| Type       | Use for                                                 |
|------------|---------------------------------------------------------|
| `feat`     | New feature (bumps minor version)                       |
| `fix`      | Bug fix (bumps patch version)                           |
| `docs`     | Documentation only (README, /docs, comments)            |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `test`     | Adding or correcting tests                              |
| `chore`    | Maintenance tasks (deps, configs)                       |
| `build`    | Build system or external dependencies                   |
| `ci`       | CI/CD configuration                                     |
| `perf`     | Performance improvement                                 |

### Allowed scopes

`m0`, `m1`, `m2`, `m3`, `m4`, `m5`, `m6`, `obd`, `display`, `rules`,
`led`, `logger`, `bt`, `docs`, `adr`, `build`, `ci`, `test`, `setup`.

### Rules

- Subject line: lowercase, imperative mood, max 72 chars, no trailing period.
- Body: wrap at 100 chars, explain **why** (not what — the diff shows what).
- Footer: reference issues with `Closes #N`, `Refs #N`, or `Fixes #N`.
- One logical change per commit. Don't mix refactoring with feature addition.

### Examples

```
feat(m2): implement ELM327 AT command initialization sequence

Adds the ATZ → ATE0 → ATL0 → ATS0 → ATH0 → ATSP0 handshake with retry
logic and 1-second timeout per command. Required before sending any
PID request.

Closes #5
```

```
fix(obd): handle 'NO DATA' response without crashing parser

The previous parser assumed every response had at least 6 hex bytes.
When ELM327 returned 'NO DATA' (e.g., engine off), the parser would
read past the buffer. Now we validate response length first.

Refs #8
```

## Code Style

- **Language**: identifiers in English, comments in Brazilian Portuguese.
- **C++ standard**: C++17 (enforced via `-std=gnu++17` in platformio.ini).
- **Formatting**: clang-format, Google base style, 4-space indent, 100-col limit.
- **Naming**: `snake_case` for variables and functions, `PascalCase` for
  classes and structs, `UPPER_SNAKE_CASE` for macros and constants.
- **Headers**: every file has LGPL/MIT-compatible header (see existing files).

## Pull Request Checklist

- [ ] Branch name follows `feature/m<N>-<topic>` convention
- [ ] Commits follow Conventional Commits
- [ ] Code compiles with `-Wall -Wextra` (no new warnings)
- [ ] `pio run` succeeds
- [ ] Documentation in `/docs` updated if behavior changed
- [ ] ADR created if architectural decision was made
- [ ] Issue(s) referenced in PR description (`Closes #N`)
```
