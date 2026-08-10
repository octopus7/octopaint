# OctoPaint 이미지 프로세싱 구현 계획

상태: `VALIDATED_COMPLETE`
최종 계약 감사: `PASS` — content SHA-256 `dd576c38856996391e26eaa78e2db03d649704783e6d39bba5dfbef3bc26cc17` (상태 metadata 적용 전)
실행 범위: 현재 대화에서 Group 1·2·3·4·5 구현·통합
후속 새 대화 범위: Application/renderer adapter, UI, GPU parity, LinearSrgbV2
비범위: `Workspace`, WinUI, D3D/HLSL, Undo/Redo, 파일 형식, 실제 제품 명령 및 UI 연결

## 1. 목표와 완료 의미

플랫폼 API에 의존하지 않는 C++23 CPU 이미지 프로세싱 라이브러리를 만들고 작은 고정 버퍼로 검증한다. Group 1은 기본 명도·톤, Group 2는 Histogram·Levels·Curves, Group 3은 Hue·Saturation·RGB Offset·Colorize, Group 4는 Convolution·Box/Gaussian Blur·Sharpen·Unsharp Mask, Group 5는 effect·edge·deterministic noise/dither를 구현한다.

이번 단계에서 “구현 완료”는 다음만 뜻한다.

- typed CPU 함수와 독립 headless test가 존재한다.
- portable GCC/Clang 계열 build와 Visual Studio project 구성이 존재한다.
- 현재 WSL에서 실행 가능한 테스트를 실제로 통과한다.
- Application/renderer 연결은 하지 않는다.
- Windows MSVC 실행 증거가 없으면 `NOT_RUN`으로 기록한다.

이 구현은 **EncodedSrgbV1 compatibility reference**다. 현재 저장 경계와 비교할 수 있는 CPU 기준이지만, 미래 renderer 전체의 무조건적인 색 정확도 기준이나 linear-light 대체물이라고 주장하지 않는다.

## 2. 현재 코드와의 확인된 경계

- Core tile/tool 저장은 premultiplied RGBA8다.
- Application renderer snapshot은 Core RGBA를 premultiplied BGRA8로 변환한다.
- 현재 CPU compositor는 encoded channel에서 premultiplied BGRA를 합성한다.
- 목표 editor architecture에는 linear-space processing이 별도로 기술돼 있다.
- 따라서 이번 library input은 `Rgba8Premultiplied + EncodedSrgbV1`로 이름과 버전을 고정한다.
- BGRA 변환, linear-sRGB/ICC 변환 및 profile adapter는 후속 integration 소유다.
- library는 `OctoPaint.Core`, `OctoPaint.Application`, WinUI, Windows SDK를 참조하지 않고 C++23 표준 라이브러리만 사용한다.
- 제품 요구사항의 `ImageOperator` registry와 versioned parameter schema는 후속 adapter 소유다. 이번 typed 함수들을 registry에 직접 연결하지 않는다.

## 3. 공통 point-operation 계약

### 3.1 프로젝트와 namespace

```text
src/OctoPaint.ImageProcessing/
  include/octopaint/image/
  src/
tests/OctoPaint.ImageProcessing.*.Tests/
```

namespace는 `octopaint::image`를 사용한다.

### 3.2 public buffer 타입

```cpp
enum class PixelFormat : std::uint8_t {
    Rgba8Premultiplied = 0,
};

enum class ColorEncoding : std::uint8_t {
    EncodedSrgbV1 = 0,
};

enum class BorderMode : std::uint8_t {
    Clamp = 0,
    Mirror = 1,
    Transparent = 2,
};

struct ImageView final {
    std::span<std::byte const> pixels;
    std::size_t stride_bytes;
    std::uint32_t width;
    std::uint32_t height;
    PixelFormat format;
    ColorEncoding encoding;
};

struct MutableImageView final {
    std::span<std::byte> pixels;
    std::size_t stride_bytes;
    std::uint32_t width;
    std::uint32_t height;
    PixelFormat format;
    ColorEncoding encoding;
};
```

Group 1·2·3와 G5-A/B는 source와 destination geometry가 같은 **point operation**만 사용한다. tile halo와 destination ROI는 이 타입에 억지로 넣지 않는다. Group 4와 G5-C는 별도 `NeighborhoodImageView` 계약을 사용한다.

### 3.3 stable result contract

```cpp
enum class ProcessResult : std::uint8_t {
    Succeeded = 0,
    InvalidParameter,
    UnsupportedPixelFormat,
    UnsupportedColorEncoding,
    DimensionMismatch,
    SizeOverflow,
    InvalidSourceStride,
    InvalidDestinationStride,
    SourceBufferTooSmall,
    DestinationBufferTooSmall,
    OverlappingBuffers,
    SourceNotPremultiplied,
    InvalidControlPoints,
    InvalidKernel,
    InvalidRegion,
    InsufficientHalo,
    ResourceLimitExceeded,
};

std::string_view ProcessResultMessage(ProcessResult result) noexcept;
ProcessResult ValidateImage(ImageView source) noexcept;
ProcessResult ValidateImagePair(ImageView source,
                                MutableImageView destination) noexcept;
```

`ProcessResultMessage`는 모든 enum 값과 unknown cast에 대해 안정된 비소유 문자열을 반환한다.

### 3.4 validation precedence와 byte 범위

operation wrapper는 parameter/option enum을 먼저 검증하고 다음 함수별 순서를 따른다.

```text
ValidateImage(source):
  source format -> source encoding -> row/required-byte overflow
  -> source stride -> source buffer -> source premultiplied invariant

ValidateImagePair(source, destination):
  source format -> destination format
  -> source encoding -> destination encoding
  -> dimension match -> row/required-byte overflow
  -> source stride -> destination stride
  -> source buffer -> destination buffer
  -> overlap -> source premultiplied invariant

operation wrapper:
  spec/option enum -> 위 source/pair validation -> destination mutation
```

format cast 오류는 `UnsupportedPixelFormat`, encoding cast 오류는 `UnsupportedColorEncoding`, operation option/method cast 오류는 `InvalidParameter`다.

정확한 규칙:

- width 또는 height가 0이어도 parameter, format, encoding, pair dimension은 검증한다. 그 뒤 stride/buffer/overlap/premultiplied 검증 없이 성공 no-op다.
- non-empty required bytes는 `(height - 1) * stride_bytes + width * 4`다. 마지막 row 뒤 padding은 span에 포함하지 않아도 된다.
- active pixel은 각 row의 첫 `width * 4` byte다. row padding은 읽거나 변경하지 않는다.
- exact alias는 pointer, span length, width, height, stride, format, encoding이 모두 같은 경우다.
- exact alias만 in-place로 허용한다. 그 외 supplied span 주소 범위가 겹치면 `OverlappingBuffers`다.
- source는 모든 호출에서 immutable이다. 성공 시 destination active pixel만 변경한다.
- 실패 시 destination span 전체가 byte-for-byte 유지된다.
- overwrite destination의 기존 RGB/A 값은 검증하지 않는다.
- source active pixel은 `R <= A`, `G <= A`, `B <= A`여야 한다. 위반하면 `SourceNotPremultiplied`다.
- alpha 0인 유효 source는 RGB도 0이다. 성공 output도 이 canonical form을 유지한다.

### 3.5 integer pixel math

Group 1·2·3와 Group 5의 straight-RGB byte path는 다음 정수식을 사용한다.

```text
unpremultiply(c, a) = a == 0 ? 0 : floor((c * 255 + floor(a / 2)) / a)
premultiply(c, a)   = floor((c * a + 127) / 255)
clamp_byte(x)       = min(255, max(0, x))
```

- 모두 넉넉한 unsigned/signed intermediate에서 계산한다.
- alpha는 Group 1·2·3·5에서 보존한다. Group 4는 §4.5의 four-channel convolution 및 final invariant 규칙을 따른다.
- neutral spec은 active row copy fast path를 사용해 모든 valid premultiplied byte를 byte-exact 보존한다.
- compile option에서 fast-math를 사용하지 않는다.

## 4. Group 1·2·3·4·5 public API freeze

모든 header와 parameter 의미는 leaf fan-out 전에 contract owner가 test와 함께 publish한다. leaf는 public header를 수정하지 않는다.

### 4.1 LUT와 deterministic transfer compiler

```cpp
using ChannelLut8 = std::array<std::uint8_t, 256>;
struct RgbLut8 final { ChannelLut8 red, green, blue; };

ProcessResult ApplyRgbLut(ImageView source,
                          MutableImageView destination,
                          RgbLut8 const& lut) noexcept;
```

Brightness·Contrast는 아래 exact integer formula로 LUT를 만든다.

```text
BrightnessSpec.offset_byte: integer [-255, 255]
brightness(x) = clamp_byte(x + offset_byte)

ContrastSpec.factor_q8_8: integer [0, 1024], identity = 256
n = (2*x - 255) * factor_q8_8 + 255*256
contrast(x) = 0 if n <= 0,
              255 if n >= 255*512,
              floor((n + 256) / 512) otherwise
```

Exposure와 Gamma는 parameter를 float로 받지 않는다.

```cpp
struct ExposureSpec final { std::int16_t stops_q8_8; }; // [-4096, 4096]
struct GammaSpec final { std::uint16_t gamma_milli; };   // [1, 8000], identity 1000
```

contract commit에는 다음이 함께 있어야 한다.

- `TransferLut.cpp`의 지정된 fixed-point log2/exp2 approximation, coefficient, evaluation order 및 intermediate width
- Exposure/Gamma 전체 256-entry LUT를 검사하는 exhaustive test와 stable checksum
- NaN/infinity가 public API에 진입할 수 없도록 integer parameter만 노출한 증거
- MSVC와 GCC full-table comparison 전에는 `cross-toolchain deterministic: UNVERIFIED` 표기

normative transfer algorithm:

```text
Q30 = 1,073,741,824
Q24 = 16,777,216

ByteLog2Q24(x), x in [1,255]:
  y = floor((x*Q30 + 127) / 255)
  e = 0
  while y < Q30: y = y*2; e = e-1
  frac = 0
  for i = 0..23:
    y = floor((y*y + 2^29) / 2^30)
    if y >= 2*Q30:
      y = floor((y + 1) / 2)
      frac |= 1 << (23-i)
  return e*Q24 + frac

Exp2Q30(z_q24):
  integer = mathematical_floor(z_q24 / Q24)
  fraction = z_q24 - integer*Q24
  y = Q30
  for i = 0..23, high fractional bit first:
    if fraction bit (23-i) is set:
      y = floor((y*K[i] + 2^29) / 2^30)
  apply integer by exact left shift or half-up right shift
  return y
```

`K[i] = round_half_up(2^(2^-(i+1)) * Q30)`이며 frozen decimal constants는 다음이다.

```text
1518500250,1276901417,1170923762,1121280436,1097253708,1085434106,
1079572136,1076653033,1075196443,1074468888,1074105294,1073923544,
1073832680,1073787251,1073764537,1073753181,1073747502,1073744663,
1073743244,1073742534,1073742179,1073742001,1073741913,1073741868
```

Exposure LUT:

```text
x=0 -> 0
stops_q8_8=0 -> x
z = ByteLog2Q24(x) + stops_q8_8*65536
z >= 0 -> 255
otherwise q = Exp2Q30(z), out = clamp_byte(floor((q*255 + 2^29)/2^30))
```

Gamma LUT는 normalized `x^(1000/gamma_milli)`, 즉 conventional `x^(1/gamma)`다.

```text
x=0 -> 0; x=255 -> 255; gamma_milli=1000 -> x
z = round_half_away_from_zero(ByteLog2Q24(x)*1000/gamma_milli)
q = Exp2Q30(z)
out = clamp_byte(floor((q*255 + 2^29)/2^30))
```

모든 곱은 명시된 범위를 수용하는 64-bit integer에서 수행한다. checksum은 LUT byte를 input index 0→255 순서로 FNV-1a 64(offset `14695981039346656037`, prime `1099511628211`) 처리한다.

```text
Gamma 1:    d7fa3758735e03f5
Gamma 100:  81c47ef63d123d30
Gamma 500:  6cf3750a928573d9
Gamma 1000: 4242dc5249c33625
Gamma 2000: 284e44cb63c070e5
Gamma 8000: c6450f07d43026d2
Exposure -4096: d80ac658736bb725
Exposure -256:  d11f3f6fa0e2ed19
Exposure 0:     4242dc5249c33625
Exposure 256:   eede21252a3d2ba5
Exposure 4096:  2f75c19c2b02dada
```

`ApplyRgbLut`는 source byte를 한 번 unpremultiply한 straight R/G/B로 LUT index에 사용하고, LUT output을 기존 alpha로 한 번만 premultiply하며 alpha와 padding을 보존한다.

### 4.2 Group 1 signatures

```cpp
struct BrightnessSpec final { std::int16_t offset_byte; };
struct ContrastSpec final { std::uint16_t factor_q8_8; };

enum class DesaturateMethod : std::uint8_t {
    Luma709Q8 = 0,
    Average = 1,
    Lightness = 2,
};

ProcessResult ProcessBrightness(ImageView, MutableImageView,
                                BrightnessSpec) noexcept;
ProcessResult ProcessContrast(ImageView, MutableImageView,
                              ContrastSpec) noexcept;
ProcessResult ProcessExposure(ImageView, MutableImageView,
                              ExposureSpec) noexcept;
ProcessResult ProcessGamma(ImageView, MutableImageView,
                           GammaSpec) noexcept;
ProcessResult ProcessInvert(ImageView, MutableImageView) noexcept;
ProcessResult ProcessDesaturate(ImageView, MutableImageView,
                                DesaturateMethod) noexcept;
```

정확한 straight-RGB 연산:

```text
invert(x) = 255 - x
Luma709Q8(R,G,B) = floor((54*R + 183*G + 19*B + 128) / 256)
Average(R,G,B)   = floor((R + G + B + 1) / 3)
Lightness(R,G,B) = floor((max(R,G,B) + min(R,G,B) + 1) / 2)
```

validation precedence에서 spec/method 범위 오류는 `InvalidParameter`다.

### 4.3 Group 2 signatures와 의미

```cpp
enum class TransparentRgbPolicy : std::uint8_t {
    ExcludeFullyTransparent = 0,
    IncludeCanonicalZero = 1,
};

struct HistogramOptions final {
    TransparentRgbPolicy transparent_rgb;
};

struct Histogram256 final {
    std::array<std::uint64_t, 256> red;
    std::array<std::uint64_t, 256> green;
    std::array<std::uint64_t, 256> blue;
    std::array<std::uint64_t, 256> alpha;
    std::array<std::uint64_t, 256> luma;
    std::uint64_t pixel_count;
    std::uint64_t color_sample_count;
};

ProcessResult AnalyzeHistogram(ImageView source,
                               HistogramOptions options,
                               Histogram256& output) noexcept;
```

- RGB와 Luma는 unpremultiplied straight bytes에서 집계한다.
- Luma는 Group 1의 `Luma709Q8` 식을 공유한다.
- alpha histogram과 `pixel_count`는 모든 pixel을 포함한다.
- `ExcludeFullyTransparent`는 alpha 0 pixel을 RGB/Luma와 `color_sample_count`에서 제외한다.
- 성공하면 기존 `output`을 완전히 교체한다. zero-size 성공은 모든 bin과 두 count가 0인 `Histogram256`으로 교체한다.
- 실패하면 기존 `output` 전체가 유지된다.
- `uint32_t width * uint32_t height`의 최댓값 `18,446,744,065,119,617,025`는 `uint64_t` 최대보다 `8,589,934,590` 작으므로 valid image의 counter overflow는 불가능하다. 별도 unreachable overflow branch를 만들지 않고 buffer size의 `SizeOverflow`와 output atomicity를 테스트한다.

```cpp
struct ChannelLevels final {
    std::uint8_t input_black;
    std::uint8_t input_white;
    std::uint16_t gamma_milli;
    std::uint8_t output_black;
    std::uint8_t output_white;
};

struct LevelsSpec final {
    ChannelLevels composite;
    ChannelLevels red;
    ChannelLevels green;
    ChannelLevels blue;
};

ProcessResult ProcessLevels(ImageView, MutableImageView,
                            LevelsSpec const&) noexcept;
```

- `input_black < input_white`, `output_black <= output_white`여야 하며 gamma 범위는 `[1,8000]`이다.
- 한 channel LUT는 input clamp → normalized Gamma transfer → output range 순서다. `x <= input_black`이면 `u=0`, `x >= input_white`이면 `u=255`, 그 사이에서만 `u=floor(((x-input_black)*255 + floor((input_white-input_black)/2))/(input_white-input_black))`다. `g=GammaLut[u]`, `y=output_black + floor((g*(output_white-output_black)+127)/255)`다.
- composite LUT를 먼저 적용하고 그 결과에 R/G/B LUT를 적용한다.
- identity channel은 `{0,255,1000,0,255}`다.
- Gamma transfer는 Group 1 fixed-point compiler만 사용한다.

```cpp
struct CurvePoint8 final { std::uint8_t x; std::uint8_t y; };
using CurvePoints8 = std::span<CurvePoint8 const>;

struct CurvesSpec final {
    CurvePoints8 composite;
    CurvePoints8 red;
    CurvePoints8 green;
    CurvePoints8 blue;
};

ProcessResult ProcessCurves(ImageView, MutableImageView,
                            CurvesSpec const&) noexcept;
```

- 각 curve는 2~256 points, 첫 x=0, 마지막 x=255, x strictly increasing이다.
- duplicate/out-of-order/missing endpoint는 `InvalidControlPoints`다.
- piecewise-linear interpolation만 이번 범위다.
- segment `[x0,y0]..[x1,y1]`에서 `dx=x1-x0`, `num=y0*(x1-x)+y1*(x-x0)`, `y=floor((num+floor(dx/2))/dx)`로 integer half-up quantize한다.
- composite LUT를 먼저 적용하고 R/G/B LUT를 적용한다.
- neutral curve는 `{0,0},{255,255}`다.
- 함수는 모든 span과 point를 검증하고 LUT를 완성한 뒤 destination을 변경한다.

### 4.4 Group 3 signatures와 integer HSL 계약

```cpp
struct HueShiftSpec final { std::int32_t degrees_q8_8; }; // [-92160, 92160]
struct SaturationSpec final { std::uint16_t scale_q8_8; }; // [0, 512], identity 256
struct HueSaturationSpec final {
    std::int32_t degrees_q8_8;
    std::uint16_t saturation_scale_q8_8;
};
struct RgbOffsetSpec final {
    std::int16_t red;
    std::int16_t green;
    std::int16_t blue;
}; // each [-255,255]
struct ColorizeSpec final {
    std::int32_t hue_degrees_q8_8; // [0, 92159]
    std::uint16_t saturation_q8_8; // [0, 256]
    std::uint16_t strength_q8_8;   // [0, 256]
};

ProcessResult ProcessHueShift(ImageView, MutableImageView, HueShiftSpec) noexcept;
ProcessResult ProcessSaturation(ImageView, MutableImageView, SaturationSpec) noexcept;
ProcessResult ProcessHueSaturation(ImageView, MutableImageView,
                                   HueSaturationSpec) noexcept;
ProcessResult ProcessRgbOffset(ImageView, MutableImageView, RgbOffsetSpec) noexcept;
ProcessResult ProcessColorize(ImageView, MutableImageView, ColorizeSpec) noexcept;
```

- 모든 연산은 unpremultiplied straight bytes에서 수행하고 기존 alpha를 보존한다.
- hue는 degree Q8.8이며 modulo `360*256`으로 wrap한다. `±360°`는 identity다.
- achromatic input은 hue shift로 변하지 않으며 canonical hue 0을 사용한다.
- HSL lightness numerator는 `max+min`, saturation은 `delta/(255-|max+min-255|)`의 exact rational이다.
- hue sector는 standard RGB max-channel 식을 degree Q8.8 rational로 계산한다. 모든 division은 integer half-up이며 tie와 negative modulo는 `ColorMath.cpp`의 명시된 helper를 따른다.
- HSL→RGB는 chroma/X/m sector formula를 fixed-point integer로 평가한다. coefficient, evaluation order, intermediate width는 contract source와 test가 규범이다.
- saturation scale은 HSL saturation에 곱해 `[0,1]`로 clamp한다. combined 함수는 한 번 HSL로 변환해 hue를 먼저 wrap하고 saturation을 scale한다.
- RGB Offset은 straight R/G/B에 각 signed byte offset을 더해 clamp한다.
- Colorize는 input HSL lightness를 보존하고 지정 hue/saturation으로 만든 RGB와 원래 straight RGB를 `strength_q8_8`로 integer half-up blend한다.
- contract commit은 primary/secondary/gray vectors, `±120°`, `±360°`, saturation 0/1x/2x, alpha fixtures와 deterministic RGB grid checksum을 publish한다.
- Vibrance, Temperature/Tint, Color Balance, Channel Mixer는 Group 3 후속 확장이며 이번 완료 범위가 아니다.

### 4.5 Group 4 neighborhood·convolution 계약

Group 4는 point view와 분리된 다음 contract를 foundation commit에서 publish한다.

```cpp
struct NeighborhoodSourceView final {
    ImageView image;
    std::uint32_t origin_x;
    std::uint32_t origin_y;
    std::uint32_t full_width;
    std::uint32_t full_height;
};

struct NeighborhoodDestinationView final {
    MutableImageView image;
    std::uint32_t origin_x;
    std::uint32_t origin_y;
};

struct SampledRgba8 final {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

inline constexpr std::uint64_t MaxNeighborhoodScratchBytes =
    512ULL * 1024ULL * 1024ULL;
struct ScratchBudget final {
    std::uint64_t max_bytes{ MaxNeighborhoodScratchBytes };
};

ProcessResult ValidateNeighborhoodOperation(NeighborhoodSourceView,
    NeighborhoodDestinationView, std::uint16_t radius,
    BorderMode) noexcept;
ProcessResult SampleNeighborhoodRgba8(NeighborhoodSourceView,
    std::int64_t full_x, std::int64_t full_y, BorderMode,
    SampledRgba8& output) noexcept;

using KernelCoefficientQ16 = std::int32_t;
struct Kernel1DView final {
    std::span<KernelCoefficientQ16 const> coefficients;
    std::uint16_t radius;
};
struct Kernel3x3Q16 final {
    std::array<KernelCoefficientQ16, 9> coefficients;
};

ProcessResult ProcessSeparableConvolution(NeighborhoodSourceView,
    NeighborhoodDestinationView, Kernel1DView horizontal,
    Kernel1DView vertical, BorderMode,
    ScratchBudget budget = {}) noexcept;
ProcessResult ProcessConvolution3x3(NeighborhoodSourceView,
    NeighborhoodDestinationView, Kernel3x3Q16 const&, BorderMode) noexcept;

struct BoxBlurSpec final { std::uint16_t radius; };       // [0,32]
struct GaussianBlurSpec final { std::uint8_t radius; };   // [0,8]
struct UnsharpMaskSpec final {
    std::uint8_t radius;                                  // [0,8]
    std::uint16_t amount_q8_8;                            // [0,1024]
};

ProcessResult ProcessBoxBlur(NeighborhoodSourceView,
    NeighborhoodDestinationView, BoxBlurSpec, BorderMode,
    ScratchBudget budget = {}) noexcept;
ProcessResult ProcessGaussianBlur(NeighborhoodSourceView,
    NeighborhoodDestinationView, GaussianBlurSpec, BorderMode,
    ScratchBudget budget = {}) noexcept;
ProcessResult ProcessSharpen3x3(NeighborhoodSourceView,
    NeighborhoodDestinationView, BorderMode) noexcept;
ProcessResult ProcessUnsharpMask(NeighborhoodSourceView,
    NeighborhoodDestinationView, UnsharpMaskSpec, BorderMode,
    ScratchBudget budget = {}) noexcept;
```

좌표·halo 규칙:

- source `image` rectangle은 full-image 좌표 `(origin_x,origin_y)`에서 시작하고 full bounds 안에 있어야 한다.
- destination rectangle도 full bounds 안에 있어야 한다.
- source와 destination은 모두 `Rgba8Premultiplied + EncodedSrgbV1`이어야 하며 각자의 stride/buffer를 독립 검증한다. source active pixel만 premultiplied invariant를 검증하고 overwrite destination 기존 pixel은 검증하지 않는다.
- `full_width==0 || full_height==0`이면 둘 다 0이어야 하고 source/destination image도 empty, 모든 origin도 0이어야 한다. 이 valid empty operation은 `ValidateNeighborhoodOperation`에서 overlap·halo 없이 `ProcessResult::Succeeded` no-op다. nonzero full bounds의 zero-area destination도 origin/bounds까지 검증한 뒤 overlap·halo 없이 `ProcessResult::Succeeded` no-op다.
- radius `r` 연산은 destination rectangle을 각 방향으로 `r` 확장하고 full bounds로 clip한 모든 pixel이 source rectangle 안에 있어야 한다. 아니면 `InsufficientHalo`다.
- full bounds 밖 sample만 BorderMode로 생성한다. tile/source rectangle의 경계에 BorderMode를 적용하지 않는다.
- Mirror는 마지막 pixel을 중복하지 않는 reflect-101이며 size 1 축은 항상 index 0이다.
- neighborhood source와 destination supplied spans의 overlap은 모두 거부한다. radius 0 identity도 detached output을 사용한다.
- Group 4 public operation은 scratch budget/radius/kernel option을 먼저 검증한 뒤 `ValidateNeighborhoodOperation`을 호출한다. helper는 border enum → source view → destination view → origin/full-bound overflow → destination bounds → overlap → required halo 순서로 검증한다. `SampleNeighborhoodRgba8`은 valid border와 source를 먼저 검증하고, `full_width==full_height==0`이면 모든 border mode/coordinate에서 `InvalidRegion`을 반환하며 `output`을 보존한다. nonempty full bounds 내부 sample이 supplied source rectangle에 없으면 `InsufficientHalo`를 반환하며 실패 시 `output`을 보존한다. foundation이 두 helper를 구현·검증하고 G4-A/B/C와 G5-C는 pushed contract의 frozen seam을 소비한다.

수치·alpha 규칙:

- generic kernel coefficient는 signed Q16이고 각 1D/3×3 kernel의 algebraic sum은 정확히 65536이어야 한다. coefficient count는 `2*radius+1`, radius는 `[0,32]`, `sum(abs(coefficient)) <= 2^26`이어야 한다. radius 범위 위반은 `InvalidParameter`, coefficient count/algebraic sum/absolute-sum 위반은 `InvalidKernel`이며 scratch cap만 `ResourceLimitExceeded`다. `INT32_MIN`의 absolute value도 signed 64-bit로 계산한다.
- separable horizontal은 byte sample×Q16을 signed 64-bit Q16 scratch에 누적하고 **reduction/clamp 없이** 저장한다. vertical은 Q16×Q16 scratch를 signed 64-bit Q32에 누적한 뒤 한 번만 `v>=0 ? (v+2^31)>>32 : -(((-v)+2^31)>>32)`로 half-away-from-zero reduce하고 final clamp 전 signed result로 유지한다. `abs_sum <= 2^26`이면 horizontal magnitude는 `255*2^26 < 2^35`, vertical magnitude는 `255*2^52 < 2^60`이라 signed 64-bit 안이다.
- 3×3은 signed 64-bit Q16 accumulator를 `v>=0 ? (v+32768)>>16 : -(((-v)+32768)>>16)`로 한 번 reduce한다. convolution 중간에는 alpha/RGB invariant clamp를 하지 않는다. 최종 alpha를 `[0,255]`로 clamp한 뒤 최종 RGB를 각각 `[0,final_alpha]`로 clamp한다. separable path도 동일한 최종 clamp 순서를 사용한다.
- Box Blur radius는 `[0,32]`이고 Q16 kernel을 사용하지 않는다. 각 pass에서 고정 divisor `n=2r+1`로 `floor((nonnegative_sum + floor(n/2))/n)` half-up reduce해 byte scratch/output을 만든다. Transparent border의 0 sample도 divisor에 포함하고 edge renormalization은 없다. radius 0은 detached byte-exact copy다.
- Gaussian radius `r`는 `[0,8]`이며 order `2r` binomial row를 사용한다. coefficient는 `C(2r,i) * 2^(16-2r)`라 정확한 Q16 정수이고 합은 정확히 65536이므로 residual correction은 없다. separable Q32 규칙을 사용하며 radius 0은 identity다.
- Gaussian kernel checksum은 radius별 coefficient를 왼쪽부터 signed 32-bit little-endian으로 serialize한 뒤 FNV-1a 64로 계산한다: `r0=4d22107f9dcb30cc`, `r1=a4ee37f98269a495`, `r2=2c9b26cf4fb17115`, `r3=613dc51b0fbcab85`, `r4=8d26018468815473`, `r5=65f0292fbe04ded0`, `r6=3740e30e4e98c3a2`, `r7=ab57a9f203cbcc36`, `r8=b5dc37c1758a80d1`.
- Sharpen의 stored Q16 kernel은 `[0,-65536,0; -65536,327680,-65536; 0,-65536,0]`이고 3×3 signed reduction을 사용한다.
- Unsharp는 Gaussian byte output과 원본 premultiplied byte의 `delta=source-blur`에 `adjustment = p>=0 ? (p+128)/256 : -(((-p)+128)/256)`, `p=delta*amount_q8_8`을 적용한다. alpha channel의 `source+adjustment`를 `[0,255]`로 clamp한 뒤 RGB channel 결과를 각각 `[0,final_alpha]`로 clamp한다. amount 0은 detached byte-exact identity다.
- Blur/convolution/sharpen/unsharp는 alpha를 포함한 premultiplied RGBA 네 channel을 처리한다. Transparent border는 `(0,0,0,0)`이고 edge renormalization은 하지 않는다.
- scratch budget은 `[0, MaxNeighborhoodScratchBytes]`이며 상한 초과는 image validation 전에 `InvalidParameter`다. scratch payload byte 수는 checked `uint64_t` 곱셈/덧셈으로 계산한다. overflow는 `SizeOverflow`, required bytes가 caller budget 또는 512 MiB 공통 상한을 넘으면 `ResourceLimitExceeded`다.
- peak simultaneously-live scratch payload는 destination active pixel 수 `P=width*height`에 대해 separable/Gaussian `32*P` byte(Q16 `int64_t` RGBA), Box `4*P` byte, Unsharp amount>0 `36*P` byte(Q16 scratch+blurred RGBA staging), 3×3/Sharpen 및 separable·Box·Gaussian radius-0/Unsharp amount-0 identity `0` byte로 고정한다. Unsharp amount 0은 radius 값의 범위만 option 단계에서 검증하고 effective halo를 0으로 사용한다. container metadata/allocator overhead는 cap 계산에 포함하지 않는다.
- 필요한 모든 scratch/staging allocation은 destination mutation 전에 끝낸다. `std::bad_alloc` 또는 `std::length_error`는 `noexcept` 경계를 넘기지 않고 `ResourceLimitExceeded`로 변환하며 destination 전체를 보존한다. scratch 계산·budget 검사는 allocation 전에 수행한다.
- 모든 region, halo, kernel, radius, allocation 크기를 destination mutation 전에 검증한다. radius/resource limit 오류는 각각 `InvalidParameter`/`ResourceLimitExceeded`다.
- whole image와 적절한 tile+halo 호출은 active destination byte가 정확히 같아야 한다.

### 4.6 Group 5 effect·edge·noise/dither 계약

```cpp
struct ThresholdSpec final { std::uint8_t threshold; };
struct PosterizeSpec final { std::uint16_t levels; }; // [2,256]
struct SolarizeSpec final { std::uint8_t threshold; };
struct ImageOrigin final { std::uint32_t x; std::uint32_t y; };
struct NoiseSpec final { std::uint64_t seed; std::uint8_t amplitude; };

ProcessResult ProcessThreshold(ImageView, MutableImageView, ThresholdSpec) noexcept;
ProcessResult ProcessPosterize(ImageView, MutableImageView, PosterizeSpec) noexcept;
ProcessResult ProcessSolarize(ImageView, MutableImageView, SolarizeSpec) noexcept;
ProcessResult ProcessSepia(ImageView, MutableImageView) noexcept;
ProcessResult ProcessDeterministicNoise(ImageView, MutableImageView,
    ImageOrigin, NoiseSpec) noexcept;
ProcessResult ProcessOrderedDither4x4(ImageView, MutableImageView,
    ImageOrigin) noexcept;
ProcessResult ProcessSobelEdge(NeighborhoodSourceView,
    NeighborhoodDestinationView, BorderMode) noexcept;
ProcessResult ProcessLaplacianEdge(NeighborhoodSourceView,
    NeighborhoodDestinationView, BorderMode) noexcept;
ProcessResult ProcessEmboss(NeighborhoodSourceView,
    NeighborhoodDestinationView, BorderMode) noexcept;
```

point effect 규칙:

- Threshold는 straight RGB의 `Luma709Q8`가 threshold 이상이면 255, 미만이면 0인 grayscale이다. equality는 white다.
- Posterize는 각 straight channel에 `index=floor((x*(levels-1)+127)/255)`, `out=floor((index*255+floor((levels-1)/2))/(levels-1))`를 적용한다.
- Solarize는 `x >= threshold`인 straight channel만 `255-x`로 바꾼다.
- Sepia는 straight RGB에 Q14 matrix `[6439,12599,3097; 5718,11239,2753; 4456,8749,2146]`를 곱하고 각 row에 8192를 더해 `>>14`, byte clamp한다.
- 위 효과는 alpha를 보존하고 결과 straight RGB를 한 번 premultiply한다.

noise/dither 규칙:

- Noise는 full-image coordinate `(origin.x+local_x, origin.y+local_y)`와 explicit seed만 사용한다. 두 coordinate addition은 `uint32_t` 범위에서 checked 연산하며 overflow면 destination mutation 전에 `SizeOverflow`를 반환한다.
- 각 R/G/B channel key는 `seed ^ ((std::uint64_t{x} << 32) | std::uint64_t{y}) ^ channel_constant`다. SplitMix64는 모든 unsigned 연산을 modulo 2^64로 수행한다: `z=key+0x9e3779b97f4a7c15; z=(z^(z>>30))*0xbf58476d1ce4e5b9; z=(z^(z>>27))*0x94d049bb133111eb; hash=z^(z>>31)`.
- `signed_noise = static_cast<std::int32_t>(hash % static_cast<std::uint64_t>(2 * amplitude + 1)) - static_cast<std::int32_t>(amplitude)`를 straight channel에 더해 clamp한다. modulo 결과를 signed 32-bit로 명시 변환한 뒤 subtraction하므로 C++ unsigned promotion이 없다. channel constants는 R=0, G=`0x243f6a8885a308d3`, B=`0x13198a2e03707344`다.
- frozen hash/noise vectors `(seed,x,y,amplitude):(Rhash,Ghash,Bhash)->(Rnoise,Gnoise,Bnoise)`는 `(0,0,0,10):(e220a8397b1dcdaf,2cb0f69f4abea221,3bb548a553e612ba)->(6,-6,3)`, `(1,2,3,20):(a8391e4528c2a97f,d8df73b58dceeec6,d52cef87697459bd)->(11,9,-15)`, `(123456789abcdef0,1024,2048,255):(1e954a8841ac2400,5dc0e991bf38a98e,204e9b4925288631)->(83,-254,142)`다.
- Ordered Dither는 straight RGB의 `Luma709Q8`와 Bayer 4×4 `[0,8,2,10; 12,4,14,6; 3,11,1,9; 15,7,13,5]`를 full-image origin으로 index한다. `luma*16 >= matrix*256+128`이면 straight white, 아니면 straight black이다. coordinate overflow는 Noise와 동일하게 `SizeOverflow`다.
- noise와 dither는 기존 alpha를 보존하고 결과 straight RGB를 정확히 한 번 premultiply한다. alpha 0은 canonical `(0,0,0,0)`이며 tile/whole 결과가 byte-exact 같아야 한다.

edge 규칙:

- Sobel은 straight `Luma709Q8`에 Gx `[-1,0,1;-2,0,2;-1,0,1]`, Gy `[-1,-2,-1;0,0,0;1,2,1]`를 적용하고 `clamp_byte(floor((abs(gx)+abs(gy)+2)/4))` grayscale을 출력한다.
- Laplacian은 `[0,1,0;1,-4,1;0,1,0]` 결과의 absolute value를 byte clamp한다.
- Emboss는 zero-sum `[-2,-1,0;-1,0,1;0,1,2]` 결과에 128을 더해 clamp한다. complete sampled neighborhood가 constant인 interior pixel과 Clamp/Mirror constant field만 straight 128이다. Transparent exterior는 constancy를 깨며 constant-100 2×2 fixture의 row-major straight output을 `[255,128;128,0]`으로 고정한다.
- edge/emboss는 Group 4 neighborhood·halo·border validator를 사용한다. output alpha는 source neighborhood의 같은 full-image coordinate alpha이며 grayscale/emboss straight RGB를 그 alpha로 premultiply한다.
- 모든 operation은 option/region/source를 검증한 뒤에만 destination을 변경한다. invalid spec과 arithmetic/resource failure는 destination 전체를 보존한다.

## 5. 5개 기능 그룹

1. **기본 명도·톤** — Brightness, Contrast, Exposure, Gamma, Invert, Desaturate, LUT/buffer contract
2. **Levels·Curves** — Histogram, Levels, composite/per-channel Curves
3. **색상 이동** — Hue/Saturation, RGB offset, Colorize, 후속 Vibrance/Temperature/Tint
4. **공간 필터** — Convolution, Box/Gaussian Blur, Sharpen, Unsharp Mask
5. **효과·분석** — Threshold, Posterize, Solarize, Sepia, Sobel/Laplacian, Emboss, deterministic Noise/Dither

Group 1·2·3·4·5의 위 명시된 기본 항목을 이번 대화에서 구현한다.

## 6. 불변 SHA 파이프라인

```text
PLAN_ANCHOR_SHA
  -> Group 1+2+3+4+5 common contract/foundation commit
  -> GROUP12345_CONTRACT_SHA
      -> Wave 1: G1-A, G1-B, G2-A
      -> Wave 2: G2-B, G2-C, G3-A
      -> Wave 3: G3-B, G4-A, G5-A
  -> G4-A convolution landing commit
  -> G4_CONVOLUTION_SHA
      -> Wave 4: G4-B, G5-B
  -> G4-B Gaussian landing commit
  -> G4_GAUSSIAN_SHA
      -> Wave 5: G4-C (G4_GAUSSIAN_SHA), G5-C (G4_CONVOLUTION_SHA)
  -> leaf별 별도 landing commit 및 immediate push
  -> shared seam / project / combined-regression commit
  -> GROUP12345_INTEGRATION_SHA
  -> descendant RESULT metadata commit

GROUP12345_INTEGRATION_SHA
  -> 새 대화 Application/renderer/UI/GPU/LinearSrgbV2 integration
```

규칙:

- planning commit을 push한 뒤 `PLAN_ANCHOR_SHA`를 캡처한다.
- G1~3, G4-A, G5-A, G5-B worktree는 pushed `GROUP12345_CONTRACT_SHA`에서만 만든다.
- G4-B/G5-C는 pushed `G4_CONVOLUTION_SHA`, G4-C는 pushed `G4_GAUSSIAN_SHA`에서만 만든다.
- descendant RESULT metadata commit은 input base가 아니다.
- metadata에는 code anchor를 기록하고, metadata tip 자체는 push 후 채팅/외부 handoff에만 보고한다. commit이 자기 SHA를 기록하게 하지 않는다.
- branch 이름이나 unpushed SHA를 input identity로 사용하지 않는다.

## 7. 현재 대화의 contract/foundation owner

실행 주체: 현재 대화 주 에이전트  
실행 순서: plan push 후, 모든 leaf dispatch 전 직렬 strict TDD

소유:

```text
src/OctoPaint.ImageProcessing/include/octopaint/image/ImageView.h
src/OctoPaint.ImageProcessing/include/octopaint/image/ProcessResult.h
src/OctoPaint.ImageProcessing/include/octopaint/image/BorderMode.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Lut.h
src/OctoPaint.ImageProcessing/include/octopaint/image/TransferLut.h
src/OctoPaint.ImageProcessing/include/octopaint/image/BasicTone.h
src/OctoPaint.ImageProcessing/include/octopaint/image/BasicColor.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Histogram.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Levels.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Curves.h
src/OctoPaint.ImageProcessing/include/octopaint/image/ColorMath.h
src/OctoPaint.ImageProcessing/include/octopaint/image/ColorAdjustments.h
src/OctoPaint.ImageProcessing/include/octopaint/image/NeighborhoodImageView.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Convolution.h
src/OctoPaint.ImageProcessing/include/octopaint/image/GaussianBlur.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Sharpen.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Effects.h
src/OctoPaint.ImageProcessing/include/octopaint/image/EdgeDetection.h
src/OctoPaint.ImageProcessing/include/octopaint/image/Dither.h
src/OctoPaint.ImageProcessing/src/ImageView.cpp
src/OctoPaint.ImageProcessing/src/ProcessResult.cpp
src/OctoPaint.ImageProcessing/src/Lut.cpp
src/OctoPaint.ImageProcessing/src/TransferLut.cpp
src/OctoPaint.ImageProcessing/src/ColorMath.cpp
src/OctoPaint.ImageProcessing/src/NeighborhoodImageView.cpp
tests/OctoPaint.ImageProcessing.TestSupport/ImageFixtures.h
tests/OctoPaint.ImageProcessing.Foundation.Tests/**
이 문서 §3~4와 RESULT 구역
```

책임:

- test를 먼저 작성하고 RED를 실행한 뒤 최소 production code를 쓴다.
- complete enum/signature/formula와 exhaustive LUT test를 publish한다.
- neighborhood empty no-op, empty-full sampler `InvalidRegion`/output 보존과 Clamp/Mirror/Transparent border mapping을 Foundation test에서 RED-GREEN으로 고정한다.
- 첫 leaf dispatch 뒤에는 additive/breaking contract request를 모두 거부한다. 변경이 필수면 모든 wave를 중단하고 새 contract commit을 push해 `GROUP12345_CONTRACT_SHA`를 재지정한 뒤 **모든** leaf worktree를 새로 만들며 old-anchor tip은 landing 금지한다.
- fixture는 read-only header-only test support로 publish한다. source와 destination storage를 분리하고 mutable destination access를 명시한다.

## 8. 현재 대화 내부 병렬 분배

모든 raw leaf tip은 **review input**이다. leaf는 `REQUESTS.md`, project, solution, shared docs 또는 main을 수정하지 않는다. integration owner가 leaf별 독립 landing commit에 완료 request record를 추가하고 즉시 push한다.

### Wave 1 — 동시 3개

#### G1-A Basic Tone

```text
branch: feat/imageproc-g1-tone
worktree: /home/beelink/octopaint-worktrees/imageproc-g1-tone
owns:
  src/OctoPaint.ImageProcessing/src/BasicTone.cpp
  tests/OctoPaint.ImageProcessing.BasicTone.Tests/main.cpp
```

Brightness → Contrast → Exposure 순서로 RED-GREEN-REFACTOR하고 neutral, extrema, partial alpha, padded stride, in-place, overlap, invalid spec, failure atomicity를 검증한다.

#### G1-B Gamma·Invert·Desaturate

```text
branch: feat/imageproc-g1-color
worktree: /home/beelink/octopaint-worktrees/imageproc-g1-color
owns:
  src/OctoPaint.ImageProcessing/src/BasicColor.cpp
  tests/OctoPaint.ImageProcessing.BasicColor.Tests/main.cpp
```

Gamma → Invert → 3개 Desaturate 방식 순서로 RED-GREEN-REFACTOR한다. Gamma full LUT checksum과 canonical RGB/alpha fixtures를 포함한다.

#### G2-A Histogram

```text
branch: feat/imageproc-g2-histogram
worktree: /home/beelink/octopaint-worktrees/imageproc-g2-histogram
owns:
  src/OctoPaint.ImageProcessing/src/Histogram.cpp
  tests/OctoPaint.ImageProcessing.Histogram.Tests/main.cpp
```

transparent policy, straight RGB, Luma709Q8, alpha bins, count total, overflow/failure atomicity를 검증한다.

### Wave 2 — 동시 3개

Wave 1 slot이 반환된 뒤 시작하며 input은 여전히 `GROUP12345_CONTRACT_SHA`다.

#### G2-B Levels

```text
branch: feat/imageproc-g2-levels
worktree: /home/beelink/octopaint-worktrees/imageproc-g2-levels
owns:
  src/OctoPaint.ImageProcessing/src/Levels.cpp
  tests/OctoPaint.ImageProcessing.Levels.Tests/main.cpp
```

identity, black/white clamp, output remap, gamma, composite-before-channel, malformed range, alpha/stride/alias를 검증한다.

#### G2-C Curves

```text
branch: feat/imageproc-g2-curves
worktree: /home/beelink/octopaint-worktrees/imageproc-g2-curves
owns:
  src/OctoPaint.ImageProcessing/src/Curves.cpp
  tests/OctoPaint.ImageProcessing.Curves.Tests/main.cpp
```

identity/inverse, exact segment interpolation, channel isolation/order, duplicate/out-of-order/missing endpoints, borrowed span lifetime 동안의 완전 사전검증을 검증한다.

#### G3-A Hue·Saturation

```text
branch: feat/imageproc-g3-hue-saturation
worktree: /home/beelink/octopaint-worktrees/imageproc-g3-hue-saturation
owns:
  src/OctoPaint.ImageProcessing/src/HueSaturation.cpp
  tests/OctoPaint.ImageProcessing.HueSaturation.Tests/main.cpp
```

Hue Shift → Saturation → combined Hue/Saturation 순서로 RED-GREEN-REFACTOR한다. primary/secondary, gray invariant, `±120°`, `±360°`, saturation 0/1x/2x, grid checksum, alpha/stride/alias를 검증한다.

### Wave 3 — 동시 3개

Wave 2 slot이 반환된 뒤 시작하며 input은 여전히 `GROUP12345_CONTRACT_SHA`다.

#### G3-B RGB Offset·Colorize

```text
branch: feat/imageproc-g3-colorize
worktree: /home/beelink/octopaint-worktrees/imageproc-g3-colorize
owns:
  src/OctoPaint.ImageProcessing/src/Colorize.cpp
  tests/OctoPaint.ImageProcessing.Colorize.Tests/main.cpp
```

RGB Offset → Colorize 순서로 RED-GREEN-REFACTOR한다. offset clamp, strength 0/1, HSL lightness 관계, transparent/partial alpha, failure atomicity를 검증한다.

#### G4-A Neighborhood·Convolution·Box Blur

```text
branch: feat/imageproc-g4-convolution
worktree: /home/beelink/octopaint-worktrees/imageproc-g4-convolution
owns:
  src/OctoPaint.ImageProcessing/src/Convolution.cpp
  tests/OctoPaint.ImageProcessing.Convolution.Tests/main.cpp
```

pushed foundation의 region/halo validation·border sampling seam 재검증 → separable/3×3 convolution → Box Blur 순서로 RED-GREEN-REFACTOR한다. impulse, constant, radius 0, all border modes, insufficient halo, tile/whole equality, overlap, premultiplied invariant를 검증한다. separable/Box는 `ScratchBudget{required-1}`에서 destination 불변 `ResourceLimitExceeded`, exact required에서 success, identity의 zero-scratch success를 검증한다.

G4-A complete packet을 검증한 integration owner는 이 leaf를 별도 request-record landing commit으로 즉시 main에 push하고 그 code SHA를 `G4_CONVOLUTION_SHA`로 게시한다.

#### G5-A Threshold·Posterize·Solarize·Sepia

```text
branch: feat/imageproc-g5-effects
worktree: /home/beelink/octopaint-worktrees/imageproc-g5-effects
owns:
  src/OctoPaint.ImageProcessing/src/Effects.cpp
  tests/OctoPaint.ImageProcessing.Effects.Tests/main.cpp
```

threshold equality, posterize level/rounding, solarize equality, Q14 sepia matrix, straight/premultiplied conversion, alpha/stride/alias와 invalid spec atomicity를 검증한다.

### Wave 4 — 동시 2개

G4-B worktree는 pushed `G4_CONVOLUTION_SHA`, G5-B worktree는 `GROUP12345_CONTRACT_SHA`에서 만든다.

#### G4-B Gaussian Blur

```text
branch: feat/imageproc-g4-gaussian
worktree: /home/beelink/octopaint-worktrees/imageproc-g4-gaussian
owns:
  src/OctoPaint.ImageProcessing/src/GaussianBlur.cpp
  tests/OctoPaint.ImageProcessing.GaussianBlur.Tests/main.cpp
```

radius 0~8 exact binomial Q16 kernel checksum, constant/impulse, border, alpha, tile/whole equality를 검증한다. Gaussian은 `ScratchBudget{required-1}` failure atomicity, exact required success, radius 0 zero-scratch success와 upper-bound option precedence를 포함한다.

G4-B complete packet을 검증한 integration owner는 이 leaf를 별도 request-record landing commit으로 즉시 main에 push하고 그 code SHA를 `G4_GAUSSIAN_SHA`로 게시한다.

#### G5-B Deterministic Noise·Ordered Dither

```text
branch: feat/imageproc-g5-dither
worktree: /home/beelink/octopaint-worktrees/imageproc-g5-dither
owns:
  src/OctoPaint.ImageProcessing/src/Dither.cpp
  tests/OctoPaint.ImageProcessing.Dither.Tests/main.cpp
```

SplitMix64 seed/channel vectors, amplitude 0/extrema, Bayer 4×4 equality, full-image origin, tile/whole equality, alpha와 failure atomicity를 검증한다.

### Wave 5 — 동시 2개

G4-C worktree는 pushed `G4_GAUSSIAN_SHA`, G5-C worktree는 pushed `G4_CONVOLUTION_SHA`에서 만든다.

#### G4-C Sharpen·Unsharp Mask

```text
branch: feat/imageproc-g4-sharpen
worktree: /home/beelink/octopaint-worktrees/imageproc-g4-sharpen
owns:
  src/OctoPaint.ImageProcessing/src/Sharpen.cpp
  tests/OctoPaint.ImageProcessing.Sharpen.Tests/main.cpp
```

fixed 3×3 sharpen, amount 0 identity, Gaussian-based unsharp, extrema clamp, alpha invariant, border와 tile/whole equality를 검증한다. Unsharp amount>0의 `36*P` budget-minus-one failure atomicity/exact-budget success와 amount 0 zero-scratch success를 포함한다.

#### G5-C Sobel·Laplacian·Emboss

```text
branch: feat/imageproc-g5-edge
worktree: /home/beelink/octopaint-worktrees/imageproc-g5-edge
owns:
  src/OctoPaint.ImageProcessing/src/EdgeDetection.cpp
  tests/OctoPaint.ImageProcessing.Edge.Tests/main.cpp
```

horizontal/vertical step fixtures, Clamp/Mirror 및 interior에서 constant Sobel/Laplacian 0·Emboss 128, Transparent constant-100 2×2의 exact edge/emboss golden, all border modes, halo rejection, source-alpha preservation과 tile/whole equality를 검증한다.

### leaf 공통 금지 경로

```text
public contract headers
foundation/TestSupport files
*.sln
*.vcxproj*
CMakeLists.txt
build*.bat
Workspace.* / MainWindow.*
기존 Core/Application/WinUI source
README / docs / PROGRESS.md / REQUESTS.md / RESULT
main 직접 push
```

### leaf 완료 packet

```text
Status
Resolved start SHA
Local branch tip
Fetched remote tip
Start-SHA ancestry result
Exact ownership diff result
RED command/result and GREEN command/result
Werror command/result
ASan+UBSan command/result
Contract requests
Limitations
```

## 9. Group 1·2·3·4·5 landing 및 integration owner

실행 주체: 현재 대화 주 에이전트  
main과 shared seam의 유일한 owner

공유 소유:

```text
OctoPaint.sln
CMakeLists.txt
src/OctoPaint.ImageProcessing/OctoPaint.ImageProcessing.vcxproj*
tests/OctoPaint.ImageProcessing.*.Tests/*.vcxproj*
build-headless-tests.bat
build-release.bat
README.md
README_KO.md
README_JA.md
docs/ARCHITECTURE.md
docs/EDITOR_ARCHITECTURE.md
docs/IMAGE_PROCESSING_IMPLEMENTATION_PLAN.md
PROGRESS.md
REQUESTS.md
```

landing protocol:

1. fetch remote leaf branch
2. `Status=COMPLETE`, exact start SHA, remote/local tip, ancestry, ownership, tests를 확인한다.
3. leaf diff를 적용한다.
4. 해당 leaf 완료 entry를 `REQUESTS.md`에 추가한다.
5. 하나의 coherent landing commit을 만들고 즉시 main push한다.
6. 다음 leaf로 이동한다.
7. foundation과 각 leaf landing 때 해당 source/test를 project와 CMake에 등록한다. 모든 leaf가 landed된 뒤 runner/README/docs 및 combined regression을 별도 shared-seam commit으로 push한다.

request-record protocol:

- planning/amendment commit, contract/foundation commit, 열세 leaf landing commit, shared integration commit, RESULT metadata commit 각각에 그 commit으로 완료되는 작업의 `REQUESTS.md` entry를 포함하고 즉시 push한다.
- raw leaf tip은 review input이라 `REQUESTS.md`를 포함하지 않는다.
- 서로 다른 leaf와 shared integration을 하나의 commit 또는 하나의 request entry로 합치지 않는다.

branch-local direct gate는 CMake 등록 여부와 무관하게 다음 source list를 사용한다.

```text
COMMON     = ImageView.cpp ProcessResult.cpp Lut.cpp TransferLut.cpp ColorMath.cpp NeighborhoodImageView.cpp
FOUNDATION = COMMON + Foundation.Tests/main.cpp
G1-A       = COMMON + BasicTone.cpp + BasicTone.Tests/main.cpp
G1-B   = COMMON + BasicColor.cpp + BasicColor.Tests/main.cpp
G2-A   = COMMON + Histogram.cpp + Histogram.Tests/main.cpp
G2-B   = COMMON + Levels.cpp + Levels.Tests/main.cpp
G2-C   = COMMON + Curves.cpp + Curves.Tests/main.cpp
G3-A   = COMMON + HueSaturation.cpp + HueSaturation.Tests/main.cpp
G3-B   = COMMON + Colorize.cpp + Colorize.Tests/main.cpp
G4-A   = COMMON + Convolution.cpp + Convolution.Tests/main.cpp
G4-B   = COMMON + Convolution.cpp + GaussianBlur.cpp + GaussianBlur.Tests/main.cpp
G4-C   = COMMON + Convolution.cpp + GaussianBlur.cpp + Sharpen.cpp + Sharpen.Tests/main.cpp
G5-A   = COMMON + Effects.cpp + Effects.Tests/main.cpp
G5-B   = COMMON + Dither.cpp + Dither.Tests/main.cpp
G5-C   = COMMON + EdgeDetection.cpp + Edge.Tests/main.cpp
```

각 leaf는 같은 source list로 `g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -I src/OctoPaint.ImageProcessing/include -I tests/OctoPaint.ImageProcessing.TestSupport ...`를 실행한다. sanitizer gate는 `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`를 추가한다. RED는 test를 먼저 작성해 같은 link 명령이 missing behavior/assertion으로 실패한 실제 결과를 기록하고 GREEN/Werror와 sanitizer를 다시 실행한다.

집중 test runner:

- top-level CMake/CTest target은 ImageProcessing library와 14개 test executable(Foundation, BasicTone, BasicColor, Histogram, Levels, Curves, HueSaturation, Colorize, Convolution, GaussianBlur, Sharpen, Effects, Dither, Edge)을 빌드·실행한다.
- `build-headless-tests.bat`는 WiX/WinUI packaging 없이 MSBuild 또는 CMake build와 headless tests를 실행한다.
- `build-release.bat`는 packaging 전에 focused runner 또는 동일 23개 전체 headless executable을 fail-fast로 실행한다.
- 기존 9개 + 새 14개 = 23개 headless test executable을 README 3개 언어와 release script에 동기화한다.

통합 verification:

- CMake GCC `-std=c++23 -Wall -Wextra -Wpedantic -Werror`
- 새 14개 test 실행
- 새 14개 ASan+UBSan 실행
- 기존 portable 9개 build/run 회귀
- `git diff --check`, Markdown UTF-8/fence/link, XML parse
- Windows MSVC는 실제로 실행한 경우에만 PASS. 아니면 `NOT_RUN`과 이유를 기록한다.

통합 code anchor는 shared-seam code/test commit을 push한 직후 캡처한 `GROUP12345_INTEGRATION_SHA`다. RESULT metadata는 그 descendant다.

## 10. 새 대화에서 독립 진행할 integration workstream

다섯 CPU 그룹이 `GROUP12345_INTEGRATION_SHA`로 통합된 뒤에만 다음을 별도 새 대화에서 시작한다. 이번 현재 대화는 이 파일들을 수정하지 않는다.

### I1 — Application operator adapter·registry

```text
start: GROUP12345_INTEGRATION_SHA
branch: feat/imageproc-application-adapter
owns: Application의 typed operator adapter, versioned parameter schema, command/preview transaction tests
forbidden: CPU reference algorithm 수정, WinUI/D3D wiring
```

### I2 — GPU parity·LinearSrgbV2

```text
start: GROUP12345_INTEGRATION_SHA
branch: feat/imageproc-gpu-parity
owns: 별도 render module의 HLSL kernels, CPU-vs-GPU golden comparison, linear profile adapter
forbidden: CPU EncodedSrgbV1 결과를 암묵적으로 변경
```

Windows D3D/MSVC 실행 환경에서만 PASS를 기록하고 WSL-only 결과는 `NOT_RUN`이다.

### I3 — Adjustment/filter UI와 Workspace wiring

```text
start: I1 integration anchor
branch: feat/imageproc-adjustment-ui
owns: WinUI dialogs/panels, Workspace command 연결, preview/cancel/apply flow, UI automation
forbidden: CPU/GPU algorithm 또는 file-format contract 임의 변경
```

I1/I2/I3는 각자의 plan, exact start SHA, ownership, request-record landing 및 integration gate를 새 대화에서 다시 publish한다.

## 11. 공통 fixture와 acceptance

read-only fixture: `tests/OctoPaint.ImageProcessing.TestSupport/ImageFixtures.h`

최소 fixture:

- 0×0, 1×1, odd width/height
- 마지막 row padding이 span에 없는 valid buffer
- row padding sentinel
- opaque black/white/gray/primary colors
- alpha 0, 1, 127, 128, 254, 255
- exact alias, detached destination, partial/full-span overlap
- invalid stride, undersized span, unsupported enum cast
- malformed premultiplied source
- neutral identity와 extrema
- destination prefill sentinel 및 failure atomicity

골든 결과는 외부 이미지 library runtime 결과에 의존하지 않는다. 각 leaf는 작은 exact vectors와 필요 시 전체 256 LUT checksum을 저장한다.

## 12. 현재 대화 완료 정의

1. 이 계획이 감사 지적을 반영해 검증·commit·push된다.
2. Group 1·2·3·4·5 public contract와 foundation이 strict TDD로 push된다.
3. Wave 1~3 각 세 leaf와 Wave 4~5 각 두 leaf가 별도 worktree에서 완료 packet을 제출한다.
4. 열세 leaf가 별도 request-record landing commit으로 main에 즉시 push된다.
5. shared project/CMake/runner/docs integration과 combined regression이 push된다.
6. 새 14개 portable test와 기존 9개 회귀가 통과한다.
7. Windows evidence는 실제 상태대로 PASS 또는 `NOT_RUN`이다.
8. `GROUP12345_INTEGRATION_SHA`와 downstream I1/I2/I3 start instruction을 RESULT에 기록한다. I3는 I1 integration anchor에 의존한다.

## 13. RESULT

계획 상태: `COMPLETE`
Plan anchor: plan commit push 후 기록  
Group 1·2·3·4·5 contract SHA: contract commit push 후 기록
Group 1·2·3·4·5 integration anchor: integration code commit push 후 기록
RESULT metadata tip: push 후 외부 handoff에만 보고  
Windows MSVC/WinUI evidence: `NOT_RUN` — WSL에서 실행하지 않음

### leaf 결과

```text
G1-A Status: NOT_STARTED
G1-B Status: NOT_STARTED
G2-A Status: NOT_STARTED
G2-B Status: NOT_STARTED
G2-C Status: NOT_STARTED
G3-A Status: NOT_STARTED
G3-B Status: NOT_STARTED
G4-A Status: NOT_STARTED
G4-B Status: NOT_STARTED
G4-C Status: NOT_STARTED
G5-A Status: NOT_STARTED
G5-B Status: NOT_STARTED
G5-C Status: NOT_STARTED
```

각 완료 시 §8의 complete packet으로 교체한다.

### Group 1·2·3·4·5 통합 결과

```text
Status: NOT_STARTED
Plan anchor:
Contract anchor:
G1-A landing commit:
G1-B landing commit:
G2-A landing commit:
G2-B landing commit:
G2-C landing commit:
G3-A landing commit:
G3-B landing commit:
G4-A landing commit / G4_CONVOLUTION_SHA:
G4-B landing commit / G4_GAUSSIAN_SHA:
G4-C landing commit:
G5-A landing commit:
G5-B landing commit:
G5-C landing commit:
Combined ownership audit: NOT_RUN
Portable Werror tests: NOT_RUN
Portable sanitizer tests: NOT_RUN
Existing 9-test regression: NOT_RUN
Windows MSVC/WinUI: NOT_RUN
Integration anchor SHA:
Downstream I1/I2/I3 input instruction (I3 starts from I1 integration anchor):
```
