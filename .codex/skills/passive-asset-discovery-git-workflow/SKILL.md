---
name: passive-asset-discovery-git-workflow
description: Manage source control for the Passive Network Asset Discovery System with small, clear Conventional Commits. Use for staging, commit preparation, commit messages, history review, and change isolation.
---

# Git workflow

Inspect `git status`, staged changes, and recent history before staging. Preserve unrelated changes and use path-specific staging.

Make each commit one coherent, buildable change. Use Conventional Commits in the form `type(scope): imperative summary`; prefer `feat`, `fix`, `test`, `docs`, `refactor`, `build`, `ci`, or `chore`. Examples: `feat(arp): record sender observations`, `test(cli): cover invalid durations`.

Run the focused validation before committing, inspect `git diff --cached --check`, and report the hash and checks. Do not combine unrelated cleanup, generated artifacts, or another contributor's staged work.
