# Woodcock Dance v0.3.4 test report

This report records release-gate checks for the Windows 10/11 x64 package on
2026-10-07. It intentionally excludes personal machine identifiers.

The clean Release build used Visual Studio 2022 Build Tools 17.14.35,
MSVC 19.44.35228.0, CMake, and the Windows 10.0.26100.0 SDK.

Release executable SHA-256:
`A0034033287682F5E84195F582390AA120B47B44D68C7388D0A345E604F75C38`

## Release gates

- [x] Clean Release build succeeds from this source tree.
- [x] The ZIP is rebuilt from the same source; its SHA-256 is recorded in the GitHub Release entry.
- [x] Embedded-only startup succeeds without an `assets` directory.
- [x] Valid external PNG assets load successfully.
- [x] Oversized, malformed, and reparse-point assets fall back safely. Dimension and pixel limits are enforced by the decoder path.
- [x] The first-run auto-start question contains the expected text and the `はい(&Y)` button has initial focus.
- [x] The saved `Startup=1` path creates the quoted Run value; `Startup=0` leaves the Run value absent.
- [x] The saved auto-start choice is read on the next launch.
- [ ] The tray menu can change auto-start, size, visibility, and exit.
- [ ] Hide, resize, and show preserves the flock position.
- [ ] A display/work-area/DPI change recomputes movement geometry.
- [x] The hotkey is registered; an independent registration probe receives `ERROR_HOTKEY_ALREADY_REGISTERED`.
- [x] A second launch does not create a second instance in the same session.
- [x] No TCP connection is observed during the smoke tests; the source contains no network, download, shell, or telemetry path.

## Runtime evidence

- The release package, an embedded-only copy, a malformed-asset copy, an
  oversized-asset copy, and a reparse-point-asset copy all stayed responsive
  after launch.
- Each smoke test observed zero TCP connections for the process.
- The tray/DPI items remain manual follow-ups because this environment's UI
  bridge does not expose this tray-only layered window for input. The source
  review covered their message paths and geometry-preservation logic.
