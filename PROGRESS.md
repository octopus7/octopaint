# OctoPaint Progress

Last updated: 2026-08-06 KST

This checklist is the repository-level implementation status. Completed items use `[x]`; active work and pending work remain unchecked and carry a status label.

## Project foundation

- [x] C++23 Visual Studio 2022 x64 solution
- [x] Replaceable WinUI 3 frontend boundary
- [x] UI-neutral Application API and platform-independent Core
- [x] Product, editor architecture, and file-format design documents
- [x] English, Korean, and Japanese README documents
- [x] Repository ignore rules for generated native and Windows packaging output

## Documents and history

- [x] Multiple open documents with stable, non-reused document IDs
- [x] Active document switching and closing
- [x] Per-document undo and redo history
- [x] Per-document saved revision and dirty-state tracking
- [ ] 🚧 Layer commands integrated with document history and snapshots
- [ ] Autosave and crash-recovery journal

## Raster and layer core

- [x] Fixed 256×256 premultiplied RGBA8 tiles
- [x] Sparse tile allocation and transparent-tile removal
- [x] Immutable tile payload sharing and tile-level Copy-on-Write
- [x] Raster and group layers with stable IDs
- [x] Ordered nested layer tree with validated insert, remove, and move
- [x] Blend-mode domain contract
- [ ] 🚧 Layer opacity/alpha lock in domain, snapshots, commands, and tests
- [ ] Raster layer masks and clipping relationships
- [ ] Composite, RGB, alpha, and named channels

## Painting and selection tools

- [ ] 🚧 Non-antialiased connected-pixel Pencil engine and tests
- [ ] 🚧 Deterministic time/pressure Airbrush engine and tests
- [ ] 🚧 Rectangular and elliptical marquee mask generation
- [ ] 🚧 Freehand and polygonal lasso mask generation
- [ ] 🚧 Move Layer command integration
- [ ] Selection replace, add, subtract, and intersect modes
- [ ] Tool preview overlays and one-command gesture commit

## WinUI editor shell

- [x] Native `OctoPaint.exe` and `OctoPaint` window title
- [x] Basic document creation shell
- [ ] 🚧 Multi-document tab projection
- [ ] 🚧 Left vertical tool toolbar with mutually exclusive active tool
- [ ] 🚧 Overlapping foreground/background color swatches with swap/reset
- [ ] 🚧 HSV/alpha picker synchronized with RGB and hexadecimal values
- [ ] Layer panel projection and layer property controls
- [ ] Canvas viewport, zoom, pan, rulers, and guides

## Rendering and image operations

- [ ] Direct3D canvas renderer with CPU correctness fallback
- [ ] Blend-mode compositor
- [ ] Brightness/contrast adjustment
- [ ] Hue/saturation and color adjustment
- [ ] Curves adjustment
- [ ] Desaturation adjustment
- [ ] Gaussian blur
- [ ] Crop
- [ ] Nine-anchor canvas resize
- [ ] Percentage and pixel-based image resampling

## Persistence and interoperability

- [ ] Native layered `.ocp` read/write with validation and atomic replacement
- [ ] PNG read/write
- [ ] JPEG read/write with explicit transparency flattening
- [ ] PSD layer-aware read/write
- [ ] PSD compatibility and lossy-conversion report

## Build, packaging, and verification

- [x] Headless Core tests
- [x] Application multi-document/history tests
- [x] Sparse tile and layer-domain tests
- [x] Version source in `VERSION`
- [x] Versioned `OctoPaint-<version>-win-x64.zip` build flow
- [x] Versioned `OctoPaint-<version>-win-x64.msi` WiX build definition
- [x] ZIP content verification
- [ ] MSI compilation verification on a machine with WiX Toolset 5 or newer
- [ ] Continuous integration build and test workflow
- [ ] Performance and large-document stress tests

## Current delivery slice

- [ ] 🚧 Finish and integrate the tool engine, WinUI toolbar/color picker, and document-layer commands
- [ ] Run a complete Debug x64 solution build and all headless test executables
- [ ] Commit and immediately push each independently completed work unit
