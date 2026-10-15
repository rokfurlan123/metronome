# Pulse project

- Follow `/home/rokdbest/AGENTS.md`.
- This is a native C++/Win32 Windows application. Build in Linux with MinGW-w64; do not introduce browser or .NET dependencies without a product decision.
- Keep timing and preset logic platform-independent in `src/rhythm.h`; use the audio sample clock for timing.
- Run `scripts/test.sh` for rhythm/preset changes and `scripts/build.sh` for native changes. GUI tests are development-aware smoke tests, not independent behavioral testing.
- Update `docs/WORKLOG.md` with meaningful changes and validation. Keep runtime limitations explicit.
- Do not copy, launch, or modify anything on a Windows drive without the user's required authorization. Linux-hosted Wine tests use a disposable Linux prefix.
