# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.24.0] - 2026-10-27

### Added

- `input_view::mouse()` reads the mouse: button levels and counted edges, the pointer position and whether it is over the window, and the wheel.
- The Win32 and Linux windows feed the mouse, keeping it captured while a button is held so a drag that leaves the window still ends.

### Changed

- `input::binding` can also hold a `mouse_button`, so actions bind to mouse buttons and save them as `"mouse:<name>"`.
