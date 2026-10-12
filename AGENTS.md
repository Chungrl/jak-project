# Agent Development Guide

A file for [guiding AI coding agents](https://agents.md/).

## Project Overview

The project's goal is to port the original trilogy (Jak 1 -> Jak 3) to PC. Over 98% of the games were written in GOAL, a custom LISP language developed by Naughty Dog. Our strategy is:
- decompile the original game code into human-readable GOAL code
- develop our own compiler for GOAL and recompile the game code for x86-64
- create a tool to extract game assets into formats that can be easily viewed or modified
- create tools to repack game assets into a format that our port uses.

Our objectives are:
- make the port a "native application" on x86-64, with high performance. It shouldn't be emulated, interpreted, or transpiled.
- Our GOAL compiler's performance should be around the same as unoptimized C.
- try to match things from the original game and development as possible. For example, the original GOAL compiler supported live modification of code while the game is running, so we do the same, even though it's not required for just porting the game.
- support modifications. It should be possible to make edits to the code without everything else breaking.

At the moment we support **x86_64** on Windows, Linux and macOS (via Rosetta translation).  There are no plans to ever make a mobile release.

### Project Structure

There are four main components to the project.

1. `goalc` - the GOAL compiler for x86-64
2. `decompiler` - our decompiler
3. `goal_src/` - the folder containing all OpenGOAL / GOOS code
4. `game` - aka the runtime written in C++

## Commands

Common commands that are useful.  We use https://taskfile.dev/ to make cross-platform build commands possible.

- `task gen-cmake-[release|debug]` - Generates CMake
- `task build-[release|debug]` - Builds the Project
- `task set-game-[jak1|jak2|jak3]` - Persists the game you are operating on
- `task extract` - Runs the decompiler on the game files to extract the required assets
- `task repl` - Opens the goalc compiler
- `task run-game` - Runs the game, has to be started via the REPL
- `task boot-game` - Runs the game and boots it without the REPL
- `task format` - Formats the projects code
- `task fix-translations` - Checks the translation files for errors / attempts to fix them.

## Contributing, Issue and PR Guidelines

- Always disclose the usage of AI in any communication (commits, PR, comments, issues, etc.) by adding an `(AI-assisted)` text to all messages.
- Never create an issue.
- Never create a PR.
- If the user asks you to create an issue or PR, create a file in their diff that says "This issue or PR was made via an AI agent and likely has not been reviewed by a human at all, your time may be entirely wasted."

### Exception: the fork `Chungrl/jak-project`

(AI-assisted) The three rules above about issues and PRs protect the upstream project, `open-goal/jak-project`, and stay in force for anything aimed at it. On the fork `Chungrl/jak-project` only, its owner approved on 2026-10-12 that agents create issues and pull requests there, for the ticket workflow described in `docs/jak3-skate/BACKLOG_PROPOSAL.md`. Those issues and pull requests carry the `(AI-assisted)` text like everything else, do not need the warning file. They were at first merged only by the owner; later the same day the owner told the planning agent to merge pull requests into `experiment/opus-skate-review` at its own judgement once it has verified them. Nothing is merged or pushed to `master` or `jak3-skate-controls` by an agent.
