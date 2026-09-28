# AGENTS.md

This repo is the Flag compiler written in Flag. It compiles itself, emits QBE and links
with cc.

## Before you write

Read `src/lex.flg` first, then the whole file you are going to change. New code should
read as if the same person wrote it and lex is the base.

## What we want

- The code explains itself. Needing a comment means a name is wrong: rename it. Only a trick
  no name can say gets a comment, of a few words.
- The fewest concepts, names and variables that can do the job.
- A compiler that is stable and simple, with no edge cases.
- Never write a function that is used only once; it goes inline in its caller.
- Diagnostics follow the `compiler-errors` skill.

You can redesign anything when the result is simpler: the data, the passes, the library,
the language itself. Removing a feature is a valid fix. Keep what is useful.

## How to work

Start from the data. Decide what each pass reads and writes, and pick the shape that makes
the passes simplest. The code follows from it.

Make it work first. Then do several passes of cleaning and minimization of concepts and
names: reread the whole file with fresh eyes and rewrite it. Stop when a pass finds
nothing. A workaround lives only as long as its reason; when the reason goes away, delete
it.

Every pass ends green:

    ./build.sh              stage 2 and stage 3 must emit the same QBE
    ./test.sh build/flagc   every test in tests/ passes

Every bug you find gets a test in `tests/`: `name.flg` with `name.out`, or with `name.err`
holding a piece of the expected diagnostic. When `src/` changes, `./build.sh seed`
refreshes the seeds in `boot/`, one per system.

At the end verify that the documentation is updated