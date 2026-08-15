# Academic Annotation Integration on CrossInk v1.5.1-rc-2

## Base

- CrossInk tag: `v1.5.1-rc-2`
- Base commit: `09d0045e3eae1a519974b7b5c1b889243e5e6da0`
- Integration branch: `academic-tags-v1.5.1-rc2`

This tree rebases the academic annotation and clipping work from the earlier CrossInk 1.5.0 snapshot onto CrossInk v1.5.1-rc-2.

## Academic functionality retained

- User-defined annotation-tag catalog and management screen.
- Tags assignable to EPUB clippings and to the current EPUB page.
- Tagged-page indicator in the reader status bar.
- Tag information in clipping display and exports.
- Clipping text capacity increased from 512 to 4,096 bytes.
- Explicit rejection of oversized clipping selections instead of silent truncation.
- Content-derived EPUB identity in clipping files to prevent stale annotations from attaching to another book at the same path.
- Page-tag deletion and migration when a book is deleted, moved, or renamed.
- Forward line-end selection reliability fix.
- English, Brazilian Portuguese, and European Portuguese strings and format documentation.

## RC2 functionality preserved during conflict resolution

- X4 Pro and touch-capability paths.
- Quick Actions and Quick Lock settings.
- Touch long-press support in the clipping list.
- RC2 clipping text matcher for layout-inserted hyphens.
- Updated reader menu order and end-of-book actions.
- KOReader Sync and OPDS ESP32-S3 render-stack routing in `src/main.cpp`.
- RC2 memory, reader, network, and FreeInkUI changes outside the academic patch.

The academic patch does not modify `src/main.cpp`; the RC2 stack-overflow fixes remain unchanged.

## Manual merge resolutions

The synthetic three-way merge required five manual resolutions:

1. `CHANGELOG.md`: retained the RC2 release notes and added the academic work under `Unreleased`.
2. `EpubReaderActivity.cpp`: retained both RC2's `ClippingTextMatcher` and the academic `OptionSelectionActivity` dependency.
3. `EpubReaderClippingListActivity.cpp`: retained RC2 touch/long-press input and the academic value inset.
4. `EpubReaderMenuActivity.cpp`: combined RC2 menu sizing/order with the page-tag action.
5. `SettingsActivity.cpp`: retained both Quick Actions and Manage Annotation Tags actions.

## Validation performed

- `git diff --check`: passed.
- Translation generation for all 28 languages: passed; 745 used string keys generated.
- Syntax compilation of all 14 changed/new C++ translation units using the normal simulator capability profile: passed.
- Syntax compilation of the same 14 units using the X4 Pro simulator touch/USB capability profile: passed.
- `ClipWordStoreTest`: 7 of 7 tests passed, including clipping text matcher and clipping-layout cases.
- The complete CMake test build reaches an existing RC2 baseline linker failure in `DifferentialRoundingTest` because its target omits `EpdFontFamily` symbols. The unmodified RC2 tag reproduces the same failure.

A full PlatformIO simulator build could not be completed in the integration container because PlatformIO package-network access is restricted. Run the following on the normal macOS development environment:

```bash
cd CrossInk-Academic-v1.5.1-rc2
git submodule update --init --recursive

CI=1 "$HOME/.venvs/platformio-crossink/bin/pio" \
  run -e simulator
```

Also validate the X4 Pro simulator profile:

```bash
CI=1 "$HOME/.venvs/platformio-crossink/bin/pio" \
  run -e x4-pro-simulator
```

## Recommended hardware checks

- Boot and resume through Wi-Fi for OPDS and KOReader Sync on X4 Pro.
- Create, edit, filter, and export short and long clippings.
- Create, rename, and delete annotation tags.
- Assign and remove a current-page tag and verify the status-bar indicator.
- Move, rename, and delete tagged EPUBs.
- Open legacy clipping files and verify migration.
- Exercise clipping-list tap and long-press actions on touchscreen and button-only devices.
