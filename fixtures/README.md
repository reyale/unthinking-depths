# fixtures

Shared test helpers included by unit tests across the `tests/` directory.

Not a test suite itself — no `TEST()` macros here. Just concrete implementations
of engine interfaces that tests need to instantiate without caring about bot logic.

## Contents

- `idle_bot.hpp` — `IdleBot`: a `Bot` that always returns empty commands. Used by
  determinism and replay tests that need a valid bot but want no game activity.
