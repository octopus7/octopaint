# OctoPaint 편집기 아키텍처

## 1. 제품 식별자와 범위

- 제품명, 솔루션명, 실행 파일명, 기본 창 제목은 모두 `OctoPaint`로 통일한다.
- 실행 파일은 `OctoPaint.exe`, 고유 문서 확장자는 `.ocp`를 사용한다.
- 여러 문서를 동시에 열고 편집하며, 각 문서는 독립적인 히스토리·선택 영역·보기 상태를 가진다.
- 1차 지원 파일은 `.ocp`, PNG, JPEG이며 PSD 읽기/쓰기는 호환성 계층으로 제공한다.
- UI는 WinUI 3/C++23으로 시작하되, 편집기 코어와 렌더러는 특정 UI 프레임워크에 의존하지 않는다.

## 2. 의존성 원칙

```text
OctoPaint (교체 가능한 WinUI 3 실행 프런트엔드)
        |
        v
OctoPaint.Application (유스케이스, 명령, 스냅샷)
        |
        v
OctoPaint.Core (문서 모델, 편집 규칙, 순수 C++23)
        ^                    ^
        |                    |
OctoPaint.Render.D3D11   OctoPaint.Codecs.*
(렌더 포트 구현)         (파일 포트 구현)
```

의존성 규칙:

- `Core`는 C++23 표준 라이브러리만 사용하며 WinUI, WinRT, Win32, Direct3D 타입을 포함하지 않는다.
- `Application`은 위젯 이벤트 대신 `BeginStroke`, `ResizeCanvas` 같은 사용자 의도 명령을 받는다.
- 프런트엔드는 `Application`의 명령 포트와 불변 스냅샷만 사용한다. 문서 객체를 직접 변경하지 않는다.
- D3D11 구현은 렌더 포트를 구현하지만 `Core`가 D3D11을 참조하지 않는다.
- 코덱은 `DocumentReader`/`DocumentWriter` 포트를 구현한다. PSD SDK나 외부 라이브러리 타입을 공용 헤더에 노출하지 않는다.
- 경계를 넘는 식별자는 포인터가 아니라 세대 번호를 포함한 강타입 ID를 사용한다.

## 3. 모듈 책임

| 모듈 | 책임 |
|---|---|
| `OctoPaint.Core` | 문서·레이어·채널·마스크·선택 모델, 합성 규칙, 명령 의미론 |
| `OctoPaint.Application` | 다중 문서 세션, 명령 직렬화, Undo/Redo, 작업 예약, 상태 스냅샷 |
| `OctoPaint.Render.Abstractions` | 렌더 그래프, 타일 업로드, 뷰포트 및 GPU 작업 계약 |
| `OctoPaint.Render.D3D11` | D3D11 텍스처 캐시, HLSL 합성·필터·표시, 디바이스 복구 |
| `OctoPaint.Codecs.Ocp` | `.ocp` 스냅샷 저장·복구·썸네일 |
| `OctoPaint.Codecs.Bitmap` | WIC 기반 PNG/JPEG 입출력 |
| `OctoPaint.Codecs.Psd` | PSD 계층·채널·블렌딩 변환과 호환성 진단 |
| `OctoPaint` | 교체 가능한 WinUI 3 실행 어댑터: 창, 탭, 메뉴, 패널, 입력/IME, 파일 선택기, 접근성 |

새 프런트엔드는 `Application`과 렌더 호스트 계약만 구현하며 기존 문서·도구 코드를 재사용한다.

## 4. 핵심 도메인 모델

```cpp
using DocumentId = StrongId<struct DocumentTag>;
using LayerId    = StrongId<struct LayerTag>;
using ChannelId  = StrongId<struct ChannelTag>;
using MaskId     = StrongId<struct MaskTag>;

struct SizeI { int32_t width; int32_t height; };
struct RectI { int32_t x; int32_t y; int32_t width; int32_t height; };

enum class PixelFormat { Rgba8, Rgba16, Rgba16Float, Gray8, Gray16Float };
enum class ColorSpace { Srgb, LinearSrgb /* 이후 ICC 프로필 ID로 확장 */ };
```

### Workspace

`Workspace`는 열린 `DocumentSession` 목록, 활성 문서 ID, 문서별 저장 상태를 소유한다. 문서 탭의 순서와 활성 뷰는 세션 상태이며 이미지 파일 내용과 분리한다. 같은 문서에 복수 뷰를 열 수 있도록 `ViewId`는 `DocumentId`와 별도로 둔다.

### Document

```text
Document
├─ canvas: SizeI
├─ pixelAspectRatio
├─ colorProfile / workingPixelFormat
├─ root: LayerGroup
├─ channels: ChannelSet
├─ selection: SelectionMask
├─ metadata
└─ revision: uint64
```

- 내부 합성 기준은 선형 색공간의 premultiplied RGBA다.
- 저장 원본의 비트 심도와 ICC 프로필은 메타데이터에 보존한다. 변환이 필요한 내보내기는 사용자에게 손실 진단을 제공한다.
- 모든 변경은 `revision`을 증가시킨다. 비동기 결과는 시작 revision과 대상 ID 세대를 검사한 뒤에만 반영한다.

### Layer tree

```cpp
using LayerNode = std::variant<RasterLayer, GroupLayer, AdjustmentLayer>;

struct LayerCommon {
    LayerId id;
    std::string nameUtf8;
    bool visible;
    bool locked;
    float opacity;              // 0..1
    BlendMode blendMode;
    std::optional<MaskId> mask;
};
```

- `RasterLayer`: 희소 타일 이미지와 문서 좌표상의 오프셋을 가진다.
- `GroupLayer`: 순서가 있는 자식 레이어와 pass-through/isolated 합성 모드를 가진다.
- `AdjustmentLayer`: 픽셀을 소유하지 않고 하위 합성 결과에 조정 연산을 적용한다.
- 레이어 순서는 아래에서 위로 정의한다. ID는 재정렬 후에도 유지한다.
- 최소 블렌드 모드는 Normal, Multiply, Screen, Overlay, Darken, Lighten, Color Dodge, Color Burn, Soft Light, Hard Light, Difference, Exclusion, Hue, Saturation, Color, Luminosity다.
- 각 블렌드 모드는 CPU 참조 구현과 HLSL 구현에 동일한 골든 테스트 벡터를 적용한다.

### 확정된 고급 레이어 계약

고급 레이어 기능도 프런트엔드가 아닌 Core/Application 계약에 속한다. WinUI는 불변 스냅샷을 표시하고 명령을 제출할 뿐, 선택 집합·병합 의미론·텍스트 배치·효과 계산·외부 참조 갱신 규칙을 소유하지 않는다.

```cpp
using ExtendedLayerNode = std::variant<
    RasterLayer,
    GroupLayer,
    AdjustmentLayer,
    TextLayer,
    FillLayer,
    SmartObjectLayer>;

struct LayerSelectionState {
    std::vector<LayerId> orderedIds;
    std::optional<LayerId> activeAnchor;
};

struct TextLayer {
    LayerCommon common;
    Utf8TextDocument text;
    TextLayoutParameters layout;
    AffineTransform transform;
};

struct FillLayer {
    LayerCommon common;
    VersionedFillParameters fill; // Solid, Linear/Radial Gradient, Pattern
};

struct SmartObjectLayer {
    LayerCommon common;
    SmartObjectSource source;     // EmbeddedAsset 또는 ExternalReference
    AffineTransform transform;
    CachedRepresentation fallback;
};
```

- `LayerSelectionState`는 문서별 Application 상태다. ID는 중복 없이 레이어 트리 순서로 정규화하며 활성 anchor는 선택 집합에 포함한다. 선택 변경과 검색 조건 변경은 문서 내용을 바꾸지 않으므로 history 또는 dirty revision을 만들지 않는다.
- 복수 레이어 이동과 속성 변경은 대상 ID를 명시한 composite command 하나로 실행한다. 실행 전에 모든 대상, 부모, 잠금 및 속성 값을 검증하고 하나라도 유효하지 않으면 아무것도 변경하지 않는다.
- `TextLayer`는 UTF-8 본문, 문자 run, 문단/배치 속성, 폰트 descriptor와 transform을 보존한다. DirectWrite shaping은 `ITextLayoutService` 포트 뒤에 두며 DirectWrite/Win32 타입을 Core/Application 공개 헤더에 노출하지 않는다. 요청 폰트와 실제 대체 폰트를 구분해 진단한다.
- `FillLayer`는 단색, 선형/방사형 gradient, pattern을 버전 있는 파라미터로 표현한다. 결과 픽셀은 파생 캐시이며 원본은 파라미터와 참조 자산이다.
- `LayerEffectStack`은 각 레이어의 순서 있는 비파괴 속성이며 최소한 shadow, stroke, glow 계열을 수용한다. 각 effect는 안정된 ID, schema version, 활성 상태, 파라미터, 영향 반경(halo)을 가진다.
- 색상 태그는 안정된 `LayerColorTag` 값으로 `LayerCommon`에 저장한다. 이름·종류·태그·가시성·잠금 상태 검색은 불변 `LayerTreeSnapshot`에 대한 `LayerQuery`로 수행하며 결과는 파생 상태다.
- `SmartObjectSource`는 `.ocp` 자산 테이블의 embedded content 또는 정규화한 외부 파일 참조를 가리킨다. 외부 파일 identity, 마지막 확인 시각/해시 및 마지막 유효 렌더 캐시를 보존한다. 누락·변경은 자동으로 문서를 변형하지 않고 진단과 명시적 Refresh/Relink/Embed/Replace 명령을 생성한다.
- `VectorShapeLayer`는 의도적으로 타입 집합에서 제외한다. Fill/Text 기능을 벡터 도형 편집, SVG 교환 또는 Vector Shape Layer 저장 계약으로 확장 해석하지 않는다.

명령 계약은 다음을 추가한다.

```text
MergeDown, MergeVisible, FlattenDocument
SetLayerSelection
SetPropertiesForLayers, MoveLayers
AddTextLayer, EditTextLayer, SetTextLayout
AddFillLayer, SetFillParameters
AddLayerEffect, UpdateLayerEffect, RemoveLayerEffect, ReorderLayerEffect
SetLayerColorTags
AddSmartObject, RefreshSmartObject, RelinkSmartObject, EmbedSmartObject, ReplaceSmartObject
```

- Merge Down은 같은 부모의 바로 아래 합성 가능한 형제와 선택 레이어를 합성한다. Merge Visible은 숨김 레이어를 보존하면서 보이는 기여분을 하나의 `RasterLayer`로 치환한다. Flatten은 보이는 문서 결과를 하나의 `RasterLayer`로 만들고 나머지 트리를 제거하며 숨김 데이터나 alpha 제거가 필요한 경우 interaction port로 사전 확인한다.
- 세 병합 명령은 시작 revision의 불변 합성 스냅샷을 `ICompositeEvaluator`에 전달한다. 생성 타일과 제거 트리는 Copy-on-Write payload로 history에 보관하여 한 번의 Undo가 기존 ID, 순서, 속성, 선택 집합과 픽셀을 정확히 복원한다.
- 비동기 텍스트 shaping, effect 계산, pattern decode 및 Smart Object refresh 결과는 문서 revision, 대상 `LayerId` 세대와 자산 version이 모두 일치할 때만 게시한다.

렌더러는 확장 노드를 타일 기반 render graph로 컴파일한다. Text/Fill/Smart Object는 raster source pass를 만들고 effect stack은 명시적인 halo pass를 만든 뒤 기존 mask, clipping, opacity 및 blend pass로 들어간다. CPU 참조 경로와 D3D/HLSL 경로는 같은 파라미터 계약과 색공간 정책을 사용한다. 캐시 키에는 노드 ID, 노드 version, 원본 자산 version, effect version, 출력 tile 및 color context를 포함한다.

`.ocp`는 각 확장 노드를 안정된 type ID와 schema version으로 기록하고 text run, fill/effect 파라미터, 색상 태그, embedded asset, 외부 참조 및 fallback cache를 보존한다. 큰 embedded content는 content hash 기반 자산 테이블로 중복 제거한다. 알 수 없는 미래 노드는 opaque payload와 대체 렌더를 함께 보존해 왕복 손실을 막는다.

PSD codec은 Text Layer, Fill Layer, Layer Effects 및 Smart Object별 capability를 `CompatibilityReport`에 기록한다. 의미가 일치하는 구조만 편집 가능한 PSD 구조로 매핑하고, 안전하게 해석하지 못한 원본 블록은 opaque payload로 보존한다. 구조 보존이 불가능한 경우 저장 전에 rasterize/flatten 범위와 손실 항목을 사용자에게 명시해 승인을 받아야 한다. 색상 태그와 복수 선택 상태처럼 PSD 표현이 없거나 편집 세션에만 속한 정보는 PSD 출력 결과를 바꾸지 않으며 `.ocp`에 보존한다.

### Mask, channels, selection

- 레이어 마스크와 선택 영역은 모두 문서 좌표계의 희소 단일 채널 타일로 저장하되 서로 다른 도메인 타입으로 구분한다.
- 마스크 값은 `0`이 완전 차단, `1`이 완전 통과다. 레이어 불투명도 전에 적용한다.
- `ChannelSet`은 합성 RGBA 채널 뷰와 사용자 알파 채널을 제공한다. 채널 편집 대상은 `EditTarget { LayerColor, LayerMask, UserChannel }`로 명시한다.
- 선택이 없으면 `SelectionMask::mode == All`; 빈 선택은 `None`이다. 이를 같은 상태로 취급하지 않는다.
- 선택 결합은 Replace, Add, Subtract, Intersect를 지원한다.
- 선택 표시(개미 행진)는 프런트엔드/렌더러의 오버레이이며 문서 픽셀에 포함하지 않는다.

## 5. 타일 이미지 엔진

### 저장 단위

- 기본 타일 크기는 `256 x 256`이며 문서 좌표에서 정수 `TileKey { x, y, level }`로 찾는다.
- 완전히 투명하거나 기본값뿐인 타일은 할당하지 않는 희소 저장소를 사용한다.
- 타일 payload는 불변 공유 객체다. 쓰기 시 복사(Copy-on-Write)하여 Undo와 렌더 스냅샷이 같은 데이터를 안전하게 공유한다.
- 가장자리 타일도 물리 크기는 동일하게 유지하고 유효 영역만 별도 기록해 셰이더 분기를 줄인다.
- 화면 축소용 mip 타일은 파생 캐시이며 `.ocp`의 원본 데이터나 Undo 대상이 아니다.

```cpp
class ITileStore {
public:
    virtual TileReadView read(TileKey) const = 0;
    virtual MutableTile mapForWrite(TileKey, PixelFormat) = 0;
    virtual void publish(TileKey, TilePayload, uint64_t revision) = 0;
    virtual void evictDerived(uint64_t memoryTargetBytes) = 0;
};
```

### 변경 및 무효화

명령 결과는 전체 문서가 아니라 아래 변경 집합을 발행한다.

```cpp
struct ChangeSet {
    DocumentId document;
    uint64_t beforeRevision;
    uint64_t afterRevision;
    std::vector<DirtyTileRange> content;
    std::vector<LayerId> structure;
    bool selectionChanged;
    bool metadataChanged;
};
```

`Application`은 `ChangeSet`을 렌더러와 UI 구독자에게 전달한다. 렌더러는 해당 합성 타일과 상위 mip만 무효화한다. 대형 문서에서도 전체 레이어 재합성과 전체 UI 갱신을 금지한다.

### 메모리 정책

- 원본 타일, Undo 타일, GPU 타일, 파생 mip에 각각 예산을 둔다.
- 축출 우선순위는 화면 밖 파생 mip → 재생성 가능한 GPU 타일 → 오래된 Undo 체크포인트 순서다.
- 저장되지 않은 원본 타일은 절대 단순 축출하지 않으며 압축 스왑 또는 `.ocp` 저널로 내린다.
- 메모리 압박 이벤트는 코어 정책에 전달하되 UI 스레드에서 압축이나 디스크 I/O를 수행하지 않는다.

## 6. 명령, 트랜잭션, Undo/Redo

모든 변경은 단일 `CommandDispatcher`를 통과한다.

```cpp
struct CommandContext {
    DocumentId document;
    uint64_t expectedRevision;
    CancellationToken cancel;
};

class IEditorCommand {
public:
    virtual ValidationResult validate(const WorkspaceSnapshot&) const = 0;
    virtual CommandResult execute(CommandContext&, Workspace&) = 0;
};

struct CommandResult {
    ChangeSet changes;
    std::unique_ptr<IUndoRecord> undo;
    std::vector<Diagnostic> diagnostics;
};
```

- `expectedRevision` 불일치는 묵시적으로 덮어쓰지 않고 재검증 또는 취소한다.
- Undo는 문서별 스택이다. 활성 문서를 바꿔도 각 문서의 히스토리는 유지된다.
- 픽셀 명령의 Undo 레코드는 변경 전/후 타일 payload 참조와 구조 델타만 보관한다. 문서 전체 복사는 금지한다.
- 브러시 입력은 `BeginTransaction` → 여러 `AppendStrokeSample` → `CommitTransaction`으로 묶어 Undo 한 단계가 된다. 취소 시 게시 전 타일을 복원한다.
- 슬라이더 미리보기는 임시 트랜잭션을 갱신하고 확정할 때 한 번만 커밋한다.
- 파일 열기/저장, 썸네일 생성, 캐시 축출은 히스토리에 들어가지 않는다.
- 히스토리 예산 초과 시 오래된 레코드를 체크포인트로 압축하며 저장 이후의 변경 여부(dirty)는 저장 revision과 현재 revision 비교로 계산한다.

대표 명령:

```text
CreateDocument, CloseDocument, ActivateDocument
AddLayer, RemoveLayer, MoveLayer, SetLayerProperties
AttachMask, RemoveMask, SetEditTarget
BeginStroke, AppendStrokeSample, EndStroke
SetSelection, CombineSelection, ClearSelection
ApplyAdjustment, AddAdjustmentLayer
ApplyFilter
CropDocument, ResizeCanvas, ResampleImage
ImportDocument, SaveDocument, ExportFlattened
```

## 7. 조정과 필터 확장 계약

조정과 필터는 동일한 `ImageOperator` 레지스트리를 사용한다. UI는 연산자별 패널을 하드코딩하지 않고 매개변수 스키마로 기본 편집기를 만들 수 있다.

```cpp
struct OperatorDescriptor {
    OperatorId id;               // 예: "octopaint.adjust.hsl"
    uint32_t version;
    std::string displayNameKey;
    ParameterSchema parameters;
    OperatorCapabilities caps;   // GPU, CPU, tiled, deterministic, halo radius
};

class IImageOperator {
public:
    virtual OperatorDescriptor descriptor() const = 0;
    virtual void processCpu(const TileInput&, TileOutput&, const ParameterBag&) = 0;
    virtual GpuKernelHandle gpuKernel(IRenderDevice&) = 0;
};
```

- 필수 조정: Brightness/Contrast, Saturation, Hue, Curves, Desaturate.
- Curves는 채널별 정규화 제어점과 보간법을 직렬화하며 실행 시 LUT로 컴파일한다.
- Desaturate는 단순 평균이 아니라 명시된 방식(Luma, Average, Lightness)을 매개변수로 보존한다.
- 필수 필터: Gaussian Blur. 수평/수직 분리 커널을 사용하고 반경만큼 인접 타일 halo를 요청한다. 큰 반경은 다중 패스 또는 다운샘플 경로를 사용한다.
- 모든 연산자는 CPU 참조 경로를 가져야 한다. GPU 미지원, 디바이스 손실, 테스트 환경에서 동일 계약으로 폴백한다.
- 연산자 ID와 버전을 `.ocp`에 기록한다. 알 수 없는 비파괴 연산자는 payload를 보존하고 비활성 표시하여 왕복 손실을 막는다.

## 8. 크롭, 캔버스 크기, 이미지 리샘플

서로 의미가 다른 세 명령을 분리한다.

```cpp
struct CropSpec {
    RectI boundsInDocument;
    bool deleteOutsidePixels;
};

enum class Anchor9 { TopLeft, Top, TopRight, Left, Center, Right,
                     BottomLeft, Bottom, BottomRight };

struct CanvasResizeSpec {
    SizeI newSize;
    Anchor9 anchor;
    FillSpec fill;
};

struct ResampleSpec {
    SizeI targetPixels;
    bool preserveAspectRatio;
    ResampleKernel kernel;
};
```

- Crop은 문서 원점을 선택 사각형의 좌상단으로 이동하고 레이어·마스크·선택 좌표를 함께 변환한다.
- Canvas resize는 픽셀을 스케일하지 않는다. 9방향 anchor로 기존 캔버스의 이동량을 결정하고 확장 영역을 투명 또는 지정 색으로 채운다.
- Resample은 모든 레이어 픽셀, 마스크, 사용자 채널, 선택 영역을 새 픽셀 격자로 변환한다. 벡터성 메타데이터가 생기면 별도 규칙을 둔다.
- UI의 비율 입력은 최종적으로 정수 `targetPixels`로 정규화한다. 종횡비 잠금 시 한 축 변경으로 다른 축을 계산하고 반올림 결과를 명시한다.
- 리샘플 커널은 Nearest, Bilinear, Bicubic, Lanczos3를 제공한다. 마스크/선택에는 색상 감마 변환을 적용하지 않는다.
- 세 명령 모두 타일 단위로 실행하고 취소 가능해야 하며, 완료 전에는 문서 스냅샷을 부분 상태로 노출하지 않는다.

## 9. 렌더러 경계와 D3D11 구현

프런트엔드는 네이티브 표면의 수명만 제공하고, 합성 방법을 알지 못한다.

```cpp
class IViewportRenderer {
public:
    virtual ViewportId attachSurface(const NativeSurfaceDescriptor&) = 0;
    virtual void resize(ViewportId, SizeI, float dpiScale) = 0;
    virtual void submit(RenderSnapshot) = 0;
    virtual void present(ViewportId) = 0;
    virtual void detachSurface(ViewportId) = 0;
};
```

`NativeSurfaceDescriptor`는 `Application`이나 `Core`가 아니라 플랫폼 어댑터와 렌더 구현 사이의 전용 ABI에 둔다. 향후 SDL, Win32, 다른 XAML 프런트엔드가 각자의 surface 어댑터를 제공할 수 있다.

D3D11 렌더 순서:

1. 보이는 문서 타일과 적절한 mip를 계산한다.
2. 변경된 타일만 GPU 텍스처 캐시에 업로드한다.
3. 레이어 트리를 타일별 렌더 그래프로 컴파일한다.
4. 마스크, 불투명도, 블렌딩, 조정 레이어를 HLSL 패스로 합성한다.
5. 뷰 변환, 색 관리, 선택/도구 오버레이를 적용한다.
6. 프런트엔드가 제공한 swap chain 표면에 표시한다.

캔버스 픽셀은 WinUI `WriteableBitmap`이나 UI 객체를 경유하지 않는다. GPU 리소스는 렌더러가 소유하고 코어에는 `TilePayload`와 불투명 핸들만 보인다. D3D 디바이스 손실 시 GPU 캐시만 재생성하며 문서는 유지한다.

## 10. 스레딩과 비동기 실행

```text
UI thread        입력 수집, WinUI 객체, 스냅샷 표시
State thread     명령 순서화, 문서 revision 게시 (문서별 단일 writer)
Render thread    D3D11 컨텍스트, 렌더 그래프, present
Worker pool      CPU 타일 연산, 압축, mip, 코덱 변환
I/O scheduler    파일 읽기/쓰기와 자동 복구 저널
```

- UI 스레드는 문서 잠금, GPU 완료, 파일 I/O를 기다리지 않는다.
- 상태 스레드만 공개 문서 revision을 교체한다. 작업자는 비공개 결과 타일을 만들고 상태 스레드가 원자적으로 게시한다.
- 렌더러는 불변 `RenderSnapshot`을 소비한다. 프레임 중간에 문서 트리를 잠그지 않는다.
- 긴 작업은 진행률과 취소 토큰을 제공한다. 취소된 작업의 부분 타일은 게시하지 않는다.
- 같은 문서의 변경 명령은 순서대로 실행한다. 서로 다른 문서의 타일 연산과 렌더 준비는 병렬 실행할 수 있다.
- PSD 저장처럼 스냅샷이 필요한 작업은 시작 revision의 불변 참조를 저장한다. 저장 중 편집은 허용하고, 완료 시 해당 revision만 저장됨으로 표시한다.

## 11. 파일 포트와 호환성

```cpp
class IDocumentReader {
public:
    virtual ProbeResult probe(ByteSource&) = 0;
    virtual ReadResult read(ByteSource&, const ReadOptions&, ProgressSink&) = 0;
};

class IDocumentWriter {
public:
    virtual CompatibilityReport analyze(const DocumentSnapshot&) = 0;
    virtual WriteResult write(const DocumentSnapshot&, ByteSink&,
                              const WriteOptions&, ProgressSink&) = 0;
};
```

- `.ocp`는 레이어 트리, 타일, 마스크, 사용자 채널, 조정 매개변수, 색상 정보와 앱 버전을 보존하는 기준 포맷이다.
- PNG/JPEG 저장은 명시적 평면화 내보내기다. JPEG는 알파가 없으므로 배경색 선택을 요구한다.
- PSD 입출력은 기능별 `CompatibilityReport`를 먼저 생성한다. 지원하지 않는 블렌드/조정은 경고 후 픽셀 병합 여부를 사용자가 결정한다.
- PSD에서 해석하지 못한 안전한 메타데이터 블록은 가능한 한 opaque payload로 보존하여 다시 PSD로 저장할 때 왕복시킨다.
- 저장은 임시 파일 작성, flush, 원자적 교체 순서로 수행한다. 실패하면 기존 파일을 유지한다.

## 12. 프런트엔드 계약

프런트엔드가 받는 상태는 수명이 독립적인 값 스냅샷이다.

```cpp
struct WorkspaceSnapshot {
    uint64_t sequence;
    std::vector<DocumentSummary> documents;
    std::optional<DocumentId> activeDocument;
    CommandAvailability commands;
};

struct DocumentSnapshot {
    DocumentId id;
    uint64_t revision;
    SizeI canvas;
    LayerTreeSnapshot layers;
    ChannelSnapshot channels;
    SelectionSummary selection;
};
```

- 메뉴 활성화 여부와 Undo 이름은 `CommandAvailability`가 제공한다. WinUI가 도메인 규칙을 재구현하지 않는다.
- 문자열은 UTF-8 값 또는 localization key로 경계를 넘긴다.
- 입력 좌표는 프런트엔드가 device pixel → viewport 좌표로 바꾸고, `Application`의 뷰 변환기가 문서 좌표로 정규화한다.
- 펜 압력·기울기·버튼은 플랫폼 중립 `PointerSample`로 전달한다.
- 이벤트 구독은 sequence 번호를 포함한다. UI는 오래된 스냅샷을 폐기할 수 있다.

## 13. 확장성 규칙

- 필터/조정, 코덱, 도구, 렌더 백엔드는 각각 별도 레지스트리와 안정된 C ABI 경계를 둔다.
- 1차 릴리스에서는 외부 바이너리 플러그인을 로드하지 않고 내부 모듈도 같은 계약으로 등록한다. ABI 버전 및 샌드박스 정책이 확정된 뒤 외부 로딩을 연다.
- 도구는 직접 문서를 수정하지 않고 preview overlay와 최종 command를 생성한다.
- 직렬화되는 모든 타입은 안정된 문자열 ID와 schema version을 갖는다. C++ RTTI 이름이나 enum 정수만 파일에 기록하지 않는다.
- GPU 셰이더는 연산자 ID, 버전, 픽셀 형식, 디바이스 기능으로 캐시 키를 구성한다.

## 14. 오류와 진단

- 사용자 오류, 파일 호환성 경고, 시스템 오류, 내부 불변식 위반을 구분한 `DiagnosticCode`를 사용한다.
- 명령 실패는 문서를 변경 전 revision에 유지한다.
- GPU 실패는 CPU 폴백 가능 여부와 함께 보고하고, 저장 실패는 대상 경로와 복구 가능한 임시 파일 상태를 제공한다.
- 성능 측정 지점은 명령 지연, 타일 처리량, GPU 업로드량, 합성 패스 수, 캐시 적중률, Undo 메모리, 저장 시간을 포함한다.

## 15. 단계별 구현 순서

### 단계 1 — 편집기 골격

- 다중 문서 Workspace, 불변 스냅샷, CommandDispatcher, 문서별 Undo/Redo
- RasterLayer/GroupLayer, 256 타일 저장소, Normal 합성
- WinUI 탭·레이어 패널과 D3D11 뷰포트 연결
- `.ocp` 최소 저장/열기와 PNG/JPEG 평면 입출력

완료 기준: 두 문서를 동시에 열어 독립 편집·Undo·저장하고 프런트엔드 없이 Core/Application 테스트가 통과한다.

### 단계 2 — 기본 편집 기능

- 레이어 마스크, 선택 영역, 사용자 알파 채널
- 필수 블렌드 모드와 레이어 트리 합성
- Crop, 9-anchor Canvas Resize, 비율/픽셀 Resample
- Pencil, Airbrush, Marquee/Lasso, Move Layer 도구와 브러시 트랜잭션
- 세로 툴바, 전경/배경색 스와치, HSV 피커 상태 투영
- dirty tile/mip 캐시, 메모리 예산

완료 기준: 16K 문서의 작은 영역 편집이 전체 문서 복사 없이 동작하고 CPU/HLSL 합성 골든 테스트가 일치한다.

### 단계 3 — 조정과 필터

- Brightness, Saturation, Hue, Curves, Desaturate
- Gaussian Blur CPU/HLSL 구현과 halo 스케줄링
- 비파괴 AdjustmentLayer, 미리보기 트랜잭션, 취소/진행률

완료 기준: 같은 매개변수의 CPU/GPU 결과가 정의된 허용 오차 안에서 일치하고 큰 반경 필터를 취소할 수 있다.

### 단계 4 — PSD 호환

- PSD 계층, 마스크, 채널, 주요 블렌드 모드 읽기/쓰기
- CompatibilityReport, 미지원 데이터 보존, round-trip fixture
- 대형 파일 스트리밍과 손상 파일 진단

완료 기준: 공개 테스트 fixture와 OctoPaint fixture의 읽기-쓰기-읽기 구조 및 기준 이미지 비교를 통과한다.

### 단계 5 — 제품 안정화

- 색 관리/ICC, 자동 복구, 디바이스 손실, 저메모리 대응
- 키보드·펜·고DPI·접근성, 성능 추적, 크래시 진단
- 프런트엔드 교체 검증용 최소 headless 또는 Win32 harness

완료 기준: UI 없이 주요 편집 시나리오를 자동 실행하며, 프런트엔드 교체가 Core 변경 없이 가능함을 검증한다.

## 16. 도구 시스템과 툴바 투영

도구는 프런트엔드 컨트롤이 아니라 Application에 등록되는 상태 기계다.

```cpp
enum class ToolKind {
    Pencil,
    Airbrush,
    RectangularMarquee,
    EllipticalMarquee,
    FreehandLasso,
    PolygonalLasso,
    MoveLayer,
};

struct PointerSample {
    PointF documentPosition;
    float pressure;       // 0..1
    float tiltX;
    float tiltY;
    uint64_t timestampUs;
    PointerButtons buttons;
};

struct EditorColors {
    ColorF foreground;
    ColorF background;
};
```

- 프런트엔드는 WinUI pointer 이벤트를 `PointerSample`로 변환하고 `BeginToolGesture`, `UpdateToolGesture`, `EndToolGesture`를 호출한다.
- 도구는 진행 중에는 불변 문서 위의 preview overlay만 게시하고, 종료 시 하나의 undo 가능한 명령을 생성한다.
- 도구 종류, 현재 설정, 전경색과 배경색은 `WorkspaceSnapshot`에 포함한다. WinUI를 다른 프런트엔드로 바꿔도 상태 의미가 유지된다.
- 도구 단축키와 툴바 그룹은 프런트엔드 표현이지만 ToolKind와 활성화 가능 여부는 Application이 제공한다.

### Pencil

- 입력 위치를 문서 정수 픽셀로 양자화하고 coverage는 항상 0 또는 1이다.
- 드래그 샘플 사이는 Bresenham 또는 동등한 supercover 정수 선 알고리즘으로 연결하여 빠른 입력에도 구멍이 생기지 않게 한다.
- 안티앨리어싱, subpixel coverage와 가장자리 feather를 사용하지 않는다.
- selection coverage, layer mask, alpha lock과 채널 편집 대상을 최종 쓰기 전에 적용한다.

### Airbrush

- 반경, hardness, flow, opacity, spacing, 시간당 spray rate와 pressure 매핑을 직렬화 가능한 설정으로 가진다.
- 포인터가 정지해도 timestamp 차이에 따라 도료가 누적되며, 샘플 빈도와 무관한 결과를 위해 고정 시간 step으로 적분한다.
- preview 타일과 commit 타일은 같은 dab 생성기를 사용한다.

### Selection and Move

- Marquee는 Rectangle과 Ellipse 모양을, Lasso는 Freehand와 Polygonal 경로를 제공한다.
- 모든 선택 도구는 Replace/Add/Subtract/Intersect 결합 모드를 사용한다.
- 선택 경로는 preview 동안 벡터 형태로 유지하고 commit 시 단일 채널 selection tile로 rasterize한다.
- MoveLayer는 레이어의 문서 좌표 offset을 preview하고 commit한다. 선택 픽셀 이동은 별도의 MoveSelectionContent 명령으로 분리해 의미를 혼합하지 않는다.

### Vertical toolbar and HSV picker

- WinUI 어댑터는 문서 영역 왼쪽에 세로 툴바를 투영하고 선택된 도구를 시각적으로 강조한다.
- 툴바 하단에는 전경색을 앞쪽 위, 배경색을 뒤쪽 아래에 겹친 대각선 스와치를 표시한다.
- SwapColors 명령은 두 색을 교환하고 ResetColors 명령은 검정 전경/흰색 배경으로 복원한다.
- 각 스와치를 누르면 해당 색을 편집하는 HSV picker flyout을 연다. picker는 Hue, Saturation, Value, Alpha와 RGB/hex 입력을 상호 동기화한다.
- picker preview는 임시 상태이며 확인 시 SetForegroundColor 또는 SetBackgroundColor 명령 하나로 기록한다.

## 17. 핵심 결정 요약

- UI는 WinUI 3/C++23이지만 제품의 중심 API는 플랫폼 중립 `Application` 명령과 스냅샷이다.
- 이미지는 희소 256 타일과 Copy-on-Write로 관리해 대형 문서, Undo, 비동기 렌더를 함께 해결한다.
- 문서 변경은 명령 단위로 원자적 게시하며, 렌더러는 불변 revision만 읽는다.
- D3D11/HLSL은 기본 가속 경로이고 모든 필수 연산에 CPU 참조 경로를 둔다.
- 조정과 필터는 버전이 있는 공통 연산자 계약으로 확장한다.
- 도구는 플랫폼 중립 pointer sample을 받는 Application 상태 기계이며 WinUI 툴바는 이를 투영만 한다.
- Crop, Canvas Resize, Resample은 데이터 의미가 다르므로 별도 명령으로 유지한다.
- `.ocp`가 무손실 기준 포맷이고 PNG/JPEG는 평면 내보내기, PSD는 호환성 보고가 있는 변환 포맷이다.

## 18. 채택된 핵심 워크플로와 페인팅 확장 경계

이 절의 기능은 후보 검토에서 채택되었지만 특정 프런트엔드에 귀속되지 않는다. WinUI는 키 입력, 드롭, 운영체제 클립보드와 패널을 어댑터로 투영할 뿐이며 명령 검색, 설정 의미, 이력 이동, 페인트 계산을 소유하지 않는다.

### 명령 레지스트리, 설정 및 런타임 세션

```cpp
struct CommandDescriptor {
    CommandId id;                    // 안정된 문자열 ID
    LocalizationKey label;
    std::vector<LocalizationKey> searchTerms;
    ParameterSchema parameters;
    ShortcutBinding defaultShortcut;
};

class ICommandRegistry {
public:
    virtual std::span<const CommandDescriptor> descriptors() const = 0;
    virtual CommandAvailability availability(CommandId, const WorkspaceSnapshot&) const = 0;
    virtual CommandResult invoke(CommandId, const ParameterBag&, CommandContext&) = 0;
};

class IUserSettingsPort {
public:
    virtual SettingsReadResult read() = 0;
    virtual SettingsWriteResult write(const VersionedUserSettings&) = 0;
    virtual SettingsWriteResult backupCorruptAndReset() = 0;
};

struct EditorSessionSnapshot {
    uint64_t sequence;
    std::optional<DocumentId> activeDocument;
    StableToolId activeTool;
    EditorColors colors;
    ToolSettingsSnapshot toolSettings;
    CommandContextSnapshot commandContext;
};
```

- 메뉴, 사용자 단축키와 Command Palette는 모두 `ICommandRegistry`의 같은 ID, 활성 조건과 실행 경로를 사용한다. Palette 검색용 지역화 문자열은 표시/검색에만 사용하고 저장이나 실행 identity로 사용하지 않는다.
- 단축키는 command ID와 입력 scope의 매핑으로 저장한다. 새 매핑은 동일 scope의 exact/prefix 충돌을 먼저 계산하고 사용자가 해결하기 전에는 활성 설정을 바꾸지 않는다.
- 사용자 설정은 schema version을 가진 값 객체다. Application은 마이그레이션과 기본값을 정의하고, 파일 위치·원자 저장은 설정 포트가 담당한다. 손상 파일은 백업한 뒤 안전한 기본값으로 시작하고 진단을 게시한다.
- `EditorSessionSnapshot`은 현재 실행 중인 UI 중립 편집 상태를 투영한다. 프런트엔드는 이 값을 표시하고 입력 의도를 명령으로 돌려보낸다. 정상 종료 후 workspace 복원은 별도 Reliability 후보이므로 이 계약만으로 영구 세션 복원을 암시하지 않는다.
- 파일/이미지 Drag & Drop은 어댑터가 경로·스트림·제안 동작을 `ExternalItemRequest` 값으로 바꾼 뒤 기존 열기/가져오기 포트로 전달한다. Core에는 drag event, storage item 또는 HWND가 들어오지 않는다.

### 클립보드, 복제 및 이력 경계

```cpp
struct ClipboardPayload {
    ClipboardPayloadId id;
    RectI sourceBounds;
    PixelFormat format;
    ColorProfileDescriptor color;
    SharedTileSnapshot pixels;
    std::optional<SelectionSnapshot> selection;
    uint64_t estimatedBytes;
};

class IClipboardPort {
public:
    virtual ClipboardWriteResult write(const ClipboardPayload&, ClipboardWriteMode) = 0;
    virtual ClipboardReadResult read(const ClipboardFormatPreference&) = 0;
};

struct HistorySnapshot {
    DocumentId document;
    std::vector<HistoryEntrySummary> entries;
    HistoryEntryId current;
    std::optional<HistoryEntryId> saved;
};
```

- Copy는 선택된 픽셀과 색 정보를 불변 타일 스냅샷으로 캡처하는 읽기 작업이다. Cut은 같은 payload를 만든 뒤 선택 제약을 적용한 삭제 명령 하나를 실행한다. Paste는 내부 payload나 외부 평면 이미지를 새 편집 대상으로 가져오는 명령이며 부분 타일을 공개하지 않는다.
- 운영체제별 delayed rendering과 image format 변환은 `IClipboardPort` 어댑터 책임이다. 대용량 payload는 시작 document revision을 유지하고 만료·source loss를 명시적으로 보고한다.
- Duplicate Document/Layer는 모든 복제 노드에 새 ID를 할당하되 불변 tile payload는 COW로 공유한다. 중첩 순서, 속성, 마스크와 지원되는 메타데이터를 보존하고 선택/활성 anchor 정책은 명령 매개변수로 명시한다.
- 이력 패널은 `HistorySnapshot`만 읽는다. 특정 시점 이동은 안정된 `HistoryEntryId`를 받는 원자 명령이며 실패 시 현재 revision을 유지한다. 과거로 이동한 뒤 새 명령을 실행하면 기존 redo branch를 교체한다. 별도 분기 그래프는 이번 계약에 포함하지 않는다.
- 문서별 이력, 클립보드 캡처와 복제는 서로 다른 문서의 상태를 혼합하지 않는다. workspace 런타임 세션은 명령 완료 후 새 sequence의 스냅샷만 게시한다.

### 채택된 페인팅 도메인

채택된 도구는 안정된 tool ID로 등록하며 Paint Brush, Eraser, Eyedropper, Paint Bucket, Gradient, Brush Preset, Symmetry/Mirror, Clone Stamp와 Healing을 포함한다. 각 gesture는 플랫폼 중립 pointer/stylus sample과 아래 값 계약을 사용한다.

```cpp
struct PaintBrushSettings {
    float radius;
    float hardness;
    float spacingDiameterRatio;
    float flow;
    float opacity;
    PressureMapping pressure;
    StrokeStabilizerSettings stabilizer;
    BrushTipReference tip;
};

struct FloodFillSpec {
    PointI seed;
    ColorValue replacement;
    float tolerance;
    ColorDistancePolicy distance;
    EditTarget target;
};

struct GradientSpec {
    GradientKind kind;               // Linear or Radial
    PointF start;
    PointF end;
    std::vector<GradientStop> stops;
    ColorInterpolationPolicy interpolation;
    EditTarget target;
};

struct SampleColorRequest {
    PointI point;
    SampleSource source;             // ActiveLayer or VisibleComposite
    SampleFootprint footprint;
    ColorDestination destination;    // Foreground or Background
    uint64_t expectedRevision;
};

struct SymmetrySpec {
    std::vector<DocumentSpaceReflection> reflections;
};

struct CloneSource {
    DocumentId document;
    LayerId layer;
    PointI anchor;
    uint64_t revision;
};
```

- Paint Brush는 이미 확정된 dab, hardness falloff, 거리 spacing, flow/opacity, pressure sensitivity와 Stroke Stabilizer 코어를 재사용한다. tip과 preset은 versioned data이며 코드를 실행하지 않는다.
- Eraser는 같은 dab geometry를 사용하되 색 레이어에서는 alpha를, 마스크·선택·사용자 채널에서는 coverage를 감소시킨다. alpha-locked 색 레이어에서는 기존 alpha와 완전 투명 픽셀을 바꾸지 않으며 도구 availability 또는 진단으로 제한을 드러낸다.
- Eyedropper는 요청한 immutable revision에서 활성 레이어 또는 visible composite의 색을 샘플링한다. 합성 샘플은 렌더러 전용 GPU readback에 의존하지 않고 동일한 CPU reference compositor 경로를 제공하며 문서 Undo 항목을 만들지 않는다.
- Paint Bucket은 seed에서 시작하는 연결 영역 탐색으로 정의한다. tile frontier를 스트리밍하고 방문 budget·취소 토큰을 사용하며 tolerance는 versioned color-distance policy로 평가한다.
- Gradient는 Linear와 Radial을 지원한다. preview와 commit은 동일한 stop 정렬, premultiplied alpha 및 색 공간 보간 정책을 사용한다.
- Brush Preset은 브러시 설정, pressure/stabilizer 설정과 안전한 tip asset만 저장한다. 가져오기/내보내기는 크기 한도, digest, schema version과 required feature를 검증하고 현재 설정을 부분 변경하지 않는다.
- Symmetry/Mirror는 하나의 안정화된 입력 stream을 문서 좌표에서 fan-out한다. 원본과 반사 stroke 전체가 한 preview transaction과 한 Undo 항목을 공유한다.
- Clone Stamp는 고정된 `CloneSource` revision을 읽어 source/destination overlap에 영향받지 않는다. Healing은 같은 sample 계약과 halo tile을 사용하며 색/명도 적응 알고리즘에 결정적 CPU reference 구현을 둔다.

### 타일, 선택, Undo 및 렌더 공통 계약

- 모든 변경 도구는 active selection coverage, edit target, layer mask, visibility/lock 정책과 alpha lock을 적용한 뒤 dirty tile만 게시한다. selection 밖이나 잠긴 대상의 payload는 공유 상태를 유지한다.
- Paint Brush, Eraser, Bucket, Gradient, Symmetry, Clone과 Healing은 gesture/operation 하나당 하나의 `IEditorCommand`와 Undo record를 생성한다. 취소·검증 실패·resource limit 초과 시 결과 타일을 하나도 게시하지 않는다.
- preview overlay는 시작 revision과 설정 digest를 포함한다. commit은 같은 rasterizer/operator를 사용하고 revision 불일치 시 재검증 또는 취소하므로 preview와 최종 픽셀이 조용히 달라지지 않는다.
- 렌더러는 게시된 immutable tile revision과 dirty ranges만 소비한다. Eyedropper composite, Bucket tolerance와 Healing 기준 결과에는 CPU reference 경로가 있고 GPU 가속은 같은 golden vector 허용 오차를 만족해야 한다.
- Brush Preset과 사용자 설정은 `.ocp` 문서 데이터와 분리한다. 문서 결과에 필요한 Gradient, Clone/Healing commit 결과는 타일로 저장하고, Undo 재생에 필요한 versioned 매개변수와 source revision은 이력 record가 보존한다.
- Line 및 기본 Shape 도구는 보류 상태이며 이 절의 tool ID, 명령 집합 또는 도메인 타입에 포함되지 않는다.
