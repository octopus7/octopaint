# OctoPaint product requirements

## Product identity

- Product name: `OctoPaint`
- Executable name: `OctoPaint.exe`
- Visual Studio solution name: `OctoPaint.sln`
- Startup project and user-visible application title: `OctoPaint`
- Internal libraries may use qualified names such as `OctoPaint.Core` and `OctoPaint.Application`, but the shipped application must not expose an adapter name such as `WinUI`.

## Architecture constraint

The initial frontend uses WinUI 3 and C++23. The frontend is replaceable and may not own editor behavior, document state, persistence rules, image algorithms, or renderer policy.

```text
Replaceable frontend adapter
          |
          v
Frontend-neutral application API
          |
          v
Document domain + command system + rendering/persistence ports
```

No `winrt::*`, XAML, HWND, or Windows App SDK type may appear in the public API of the application or domain layers.

## Document workspace

- Multiple documents can be open concurrently.
- Each document owns its canvas, layer tree, channels, current selection, color profile metadata, history, and dirty state.
- The workspace owns the open-document collection, active document, recent files, and frontend-independent commands for opening, closing, switching, and saving documents.
- Closing a dirty document requires an explicit save, discard, or cancel decision supplied through a frontend-neutral interaction port.

## Layer model

The layer tree supports:

- Pixel layers
- Layer groups
- Adjustment layers
- Visibility, opacity, naming, locking, ordering, and nested grouping
- Per-layer raster masks
- Clipping relationships
- Blend mode selection

The initial blend-mode contract includes:

- Normal
- Darken, Multiply, Color Burn
- Lighten, Screen, Color Dodge
- Overlay, Soft Light, Hard Light
- Difference, Exclusion
- Hue, Saturation, Color, Luminosity

Blend calculations must have a documented linear-light or encoded-color policy and produce deterministic CPU and GPU results within a defined tolerance.

## Channels and masks

- Composite color view and individual red, green, blue, and alpha channels
- Additional named alpha channels
- Channel visibility and per-channel editing
- Saved selections stored as named alpha channels
- Layer masks represented as grayscale image data independent of frontend controls

## Selections

- A document has one active selection represented as a grayscale coverage mask.
- Empty selection means the entire canvas is eligible for editing.
- Selection operations include replace, add, subtract, intersect, invert, clear, feather, expand, and contract.
- Selection bounds are cached but the mask remains the source of truth.
- A selection can be saved to or restored from an alpha channel.

## Color adjustments

The engine supports destructive commands and, where applicable, non-destructive adjustment layers for:

- Brightness and contrast
- Hue and saturation
- Curves with per-composite and per-channel curves
- Desaturate

Adjustments operate through a frontend-neutral parameter schema so another UI can construct and preview the same operation.

## Filters

- Filters use a registry and parameter-description contract independent of the frontend.
- Gaussian blur is mandatory in the first filter implementation.
- Filters support selection and layer-mask constraints.
- Large filters operate on tiles with a declared halo radius so neighboring pixels are available without flattening the full document.
- CPU SIMD is the correctness fallback; Direct3D compute is the preferred accelerated backend.

## Geometry operations

### Crop

- Crop accepts an explicit rectangle in document pixel coordinates.
- The operation can remove pixels outside the crop or preserve hidden layer pixels when a non-destructive mode is added later.
- Layers, masks, channels, selections, guides, and document origin are transformed consistently.

### Canvas resize

- Canvas width and height are entered in pixels.
- Placement uses a 3 by 3 anchor: top-left, top-center, top-right, center-left, center, center-right, bottom-left, bottom-center, or bottom-right.
- The operation changes canvas bounds without resampling layer pixels.
- Newly exposed pixels use an explicit transparent or background-fill policy.

### Image resampling

- Width and height can be specified as pixels or proportional scale.
- Aspect ratio can be locked or unlocked.
- The resampling algorithm is explicit and stored in the command.
- Initial algorithms are nearest-neighbor, bilinear, bicubic, and Lanczos.
- All layers, masks, channels, selections, and resolution metadata are resized consistently.

## Persistence and interchange

- `.ocp` is the native editable format and preserves the complete OctoPaint document model.
- PNG import/export is required, including alpha where supported.
- JPEG import/export is required with an explicit background-flattening decision for transparency.
- PSD import and export are required.
- PSD compatibility is capability-based: supported constructs round-trip; unsupported constructs must be preserved as opaque data when safe or reported as a clear lossy conversion before saving.
- Saving must be crash-safe and must not replace the last valid file until the new file is complete and validated.

## Command and history requirements

- Every document mutation is represented by a command.
- Commands provide undo and redo or store sufficient before/after state in the history system.
- Interactive previews are transient transactions and create one history entry when committed.
- Commands are independent of UI events and can be invoked by WinUI, another frontend, scripts, or tests.

## Initial delivery slices

1. Multi-document workspace, pixel layers, native command/history boundary, and WinUI tab projection.
2. Direct3D canvas, tile storage, brush foundation, selections, and raster masks.
3. Blend modes, channels, adjustments, Gaussian blur, and geometry operations.
4. `.ocp` persistence plus PNG and JPEG interchange.
5. PSD import/export with an explicit compatibility test matrix.

