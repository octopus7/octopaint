# 질문과 답변 기록

## 질문 (23:59:45 KST)

복수 표현 써야하지 않나?

### 답변

맞다. 여러 항목을 한 파일에 누적하므로 `REQUESTS.md`와 `QUESTIONS.md`처럼 복수형 파일명을 사용하는 것이 의미상 더 정확하다. 파일명과 영구 지침의 참조를 복수형으로 변경했다.

---

## 질문 (00:00:41 KST)

2026년 시점 윈도우에서 그래픽툴을 만든다고 했을 때 UI는 무엇으로 작업하는 것이 좋은가? Qt는 피하고 싶다.

### 답변

현재 비어 있는 `octopaint`를 새로 설계한다면 기본 선택은 **C#/.NET 10 + Avalonia UI + SkiaSharp**다. Avalonia는 Windows에서 Skia와 Direct3D를 사용하고, 사용자 정의 Skia 렌더링과 별도 렌더 스레드용 API를 제공하므로 이미지 편집기의 패널 UI와 고성능 캔버스를 한 기술 스택으로 구성하기 좋다. 향후 macOS나 Linux 지원 가능성도 남는다.

단, 캔버스·레이어·브러시 엔진은 Avalonia 컨트롤에 직접 묶지 않고 독립 라이브러리로 설계하는 것이 좋다. UI는 툴바, 속성 패널, 레이어 패널, 메뉴와 단축키를 담당하고, 렌더러는 타일 기반 이미지 처리와 GPU/CPU 합성을 담당하게 분리한다.

Windows 전용 네이티브 외형과 Windows API 통합이 최우선이면 **WinUI 3 + C#/C++ + 독립 Direct2D/Direct3D 캔버스**가 두 번째 선택이다. Microsoft는 WinUI 3를 신규 네이티브 Windows 앱의 권장 UI로 안내하지만, 2026년 Windows App SDK 2.0 기준으로도 안정판 InkCanvas가 없고 Win2D의 WinUI 3 지원에는 미지원 항목이 있으므로 페인팅 엔진 전체를 해당 UI 계층에 의존시키는 것은 피하는 편이 안전하다.

WPF는 성숙한 도구와 컨트롤 생태계가 장점이지만, 기본 Direct3D 연동이 오래된 D3D9 기반 `D3DImage` 중심이라 새 고성능 그래픽 도구의 첫 선택으로는 권하지 않는다. Dear ImGui는 C++ 프로토타입이나 내부 도구에는 매우 빠르고 도킹도 지원하지만, 일반 사용자용 제품의 접근성·텍스트 입력·네이티브 사용감까지 직접 다듬어야 하므로 주 UI로는 권하지 않는다.

결론은 **Avalonia를 앱 UI에, SkiaSharp를 캔버스에 사용하고 문서 모델과 렌더링 코어를 UI에서 분리하는 구성**이다.

참고 자료:

- [Avalonia의 Windows 렌더링 구조](https://docs.avaloniaui.net/docs/platform-specific-guides/windows/)
- [Avalonia 사용자 정의 렌더링](https://docs.avaloniaui.net/docs/graphics-animation/custom-rendering)
- [Microsoft의 Windows UI 프레임워크 선택 안내](https://learn.microsoft.com/en-us/windows/apps/)
- [WinUI 3의 기능 차이와 InkCanvas 상태](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/migrate-to-windows-app-sdk/what-is-supported)
- [WinUI 3용 Win2D 기능 현황](https://microsoft.github.io/Win2D/WinUI3/html/Features.htm)

---

## 질문 (00:05:12 KST)

성능 때문에 .NET도 사용할 수 없다면 어떤 UI 구성이 좋은가?

### 답변

그 조건에서 `octopaint`의 기본 선택은 **C++23 + Win32 + Dear ImGui docking + Direct3D 11**이다.

- Win32는 창, 메시지 루프, 펜·마우스 입력, IME, 클립보드와 파일 드롭을 담당한다.
- Dear ImGui는 메뉴, 도구막대, 레이어·속성 패널과 도킹 UI를 담당한다.
- 캔버스는 ImGui의 그리기 명령으로 구현하지 않고 별도의 Direct3D 텍스처와 렌더 패스로 구현한다.
- Direct2D와 DirectWrite는 벡터 도형 및 텍스트 렌더링에, WIC는 Windows 기본 이미지 코덱에 사용한다.
- 브러시, 레이어 합성, 필터는 GPU 셰이더로 처리하고 문서 모델과 Undo 시스템은 렌더러에서 분리한다.

초기 렌더러는 Direct3D 12보다 Direct3D 11을 권한다. 2D 편집기에서 D3D12가 자동으로 더 빠른 것은 아니며, D3D12는 리소스 상태·디스크립터·CPU/GPU 동기화를 애플리케이션이 직접 관리해야 한다. 렌더러 인터페이스를 분리해 실제 프로파일링 결과 D3D12의 명시적 제어가 필요할 때 교체하는 편이 낫다.

Dear ImGui의 2026년 docking 브랜치는 관리되고 있고 Win32 + DirectX 11/12 공식 백엔드와 멀티 뷰포트를 제공한다. 다만 일반 소비자용 앱 수준의 접근성, 복잡한 IME, 화면 읽기 도구, 네이티브 컨트롤 동작은 추가 구현이 필요하다.

그 부분이 더 중요하다면 **WinUI 3 C++/WinRT + Direct3D SwapChainPanel**이 대안이다. 이는 .NET이 아닌 네이티브 C++ 구성이며 Microsoft가 신규 Windows 앱에 권장하는 UI를 사용할 수 있다. 반면 최대 제어와 최소 런타임 계층이 목표라면 Win32 + Dear ImGui 구성이 더 적합하다.

참고 자료:

- [Dear ImGui docking 공식 안내](https://github.com/ocornut/imgui/wiki/Docking)
- [Win32 + DirectX 11/12 백엔드 구성](https://github.com/ocornut/imgui/wiki/Getting-Started)
- [Direct3D 11에서 Direct3D 12로 이전할 때 필요한 관리 항목](https://learn.microsoft.com/en-us/windows/win32/direct3d12/porting-from-direct3d-11-to-direct3d-12)
- [WinUI 3의 C++ 지원](https://learn.microsoft.com/en-us/windows/apps/)

---

## 질문 (00:06:20 KST)

WinUI 3은 무엇인가?

### 답변

WinUI 3은 Microsoft가 Windows 10과 Windows 11용 데스크톱 앱을 만들기 위해 제공하는 최신 Windows 전용 UI 프레임워크다. Windows App SDK에 포함되며 XAML로 화면을 정의하고 C# 또는 C++/WinRT로 동작을 구현한다.

이름은 비슷하지만 UWP 자체가 아니며, WPF나 Windows Forms의 새 버전도 아니다. 일반 데스크톱 창인 HWND 위에서 실행되고 Win32 API와 상호 운용할 수 있다. C++/WinRT 프로젝트로 만들면 .NET 런타임이나 가비지 컬렉터 없이 네이티브 C++ 앱으로 사용할 수 있다.

버튼, 메뉴, 목록, 레이아웃, 테마, 접근성과 같은 일반 UI를 Windows 11의 Fluent 디자인으로 만들기 좋다. Direct3D 또는 Direct2D로 만든 그래픽 결과는 `SwapChainPanel`이나 Composition 상호 운용을 통해 XAML 화면 안에 표시할 수 있다.

Qt와 비교하면 Windows에서 더 자연스러운 외형과 OS 통합을 얻는 대신 Windows 전용이고, Qt보다 데스크톱 도구용 생태계와 일부 고급 컨트롤이 부족하다. 순수 Win32보다 UI를 빠르게 만들 수 있지만 Windows App SDK 런타임, XAML 계층, 배포 구성이 추가된다.

`octopaint`에 사용한다면 WinUI 3은 메뉴, 도구막대, 레이어 및 속성 패널, 설정 창만 담당하고 실제 캔버스·브러시·필터·레이어 합성은 독립적인 Direct3D C++ 엔진이 담당해야 한다. 최대 성능과 최소 계층이 목표라면 Win32 + Dear ImGui가 더 직접적이고, 제품 수준의 Windows UI와 접근성이 중요하면 WinUI 3 C++/WinRT가 더 적합하다.

참고 자료:

- [Microsoft WinUI 3 개요](https://learn.microsoft.com/en-us/windows/apps/winui/winui3/)
- [Windows App SDK 개요](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/)
- [WinUI와 DirectX/Direct2D Composition 상호 운용](https://learn.microsoft.com/en-us/windows/uwp/composition/composition-native-interop)

---

## 질문 (00:10:27 KST)

CUDA를 사용하지 않고 Direct3D를 사용하는가?

### 답변

그렇다. `octopaint`의 기본 GPU 백엔드는 CUDA가 아니라 Direct3D로 구성하는 것이 적합하다. CUDA와 Direct3D는 대체 관계라기보다 목적이 다르다.

- Direct3D는 화면 출력, 캔버스 텍스처, 레이어 합성, 브러시 렌더링과 함께 HLSL 컴퓨트 셰이더를 이용한 범용 병렬 계산도 처리한다.
- CUDA는 NVIDIA GPU 전용 범용 연산 플랫폼이다. NVIDIA GPU와 CUDA Toolkit이 필요하므로 기본 백엔드로 사용하면 AMD 및 Intel GPU 사용자를 지원할 수 없다.

페인팅 도구의 확대·축소, 회전, 블렌딩, 마스크, 색상 변환, 블러, 샤픈과 대부분의 브러시 처리는 Direct3D 11/12의 픽셀 셰이더 또는 컴퓨트 셰이더로 충분하다. 같은 GPU 텍스처를 렌더링과 필터 처리에 계속 사용하므로 Direct3D와 CUDA 사이의 리소스 공유 및 동기화 계층도 피할 수 있다.

CUDA는 대형 AI 모델, CUDA 전용 라이브러리, 매우 특수한 수치 연산에서 명확한 성능 이점이 확인될 때 NVIDIA용 선택적 플러그인으로 추가하는 것이 좋다. 기본 구조는 `Direct3D 렌더링 + HLSL 컴퓨트 셰이더 + CPU SIMD 폴백`으로 두는 것이 적합하다.

참고 자료:

- [Direct3D 11 컴퓨트 셰이더](https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-features)
- [Direct3D 12 그래픽 및 컴퓨트 파이프라인](https://learn.microsoft.com/en-us/windows/win32/direct3d12/pipelines-and-shaders-with-directx-12)
- [NVIDIA CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/index.html)

---

## 질문 (00:11:22 KST)

WinUI 3과 Rust 조합이 가능한가?

### 답변

기술적으로 가능하지만, 2026년 현재 `순수 Rust + WinUI 3`는 공식적인 일급 개발 경로가 아니므로 제품의 기본 구성으로는 권하지 않는다. Microsoft의 WinUI 3 프로젝트 템플릿과 XAML 빌드 도구는 C#과 C++/WinRT를 중심으로 제공된다.

Microsoft의 `windows-rs`는 Rust에서 Win32, COM과 WinRT API를 호출할 수 있게 한다. 그러나 Windows App SDK에 별도로 포함되는 `Microsoft.UI.Xaml` 형식의 바인딩 생성, XAML 컴파일, 앱 부트스트랩, 패키징과 리소스 생성까지 Rust용으로 완성된 공식 WinUI 3 도구 체인이 제공되는 것은 아니다. 커뮤니티의 Rust WinUI 3 바인딩은 존재하지만 일부 API만 지원하거나 프로젝트 수명이 불확실할 수 있다.

Rust를 사용하려면 다음 혼합 구성이 가장 현실적이다.

```text
WinUI 3 C++/WinRT 셸
├─ 창, 메뉴, 패널, 접근성
└─ Direct3D 표시 영역
        ↕ 안정된 C ABI
Rust 코어
├─ 문서 및 레이어 모델
├─ Undo/Redo
├─ 파일 처리
└─ CPU 이미지 알고리즘
```

렌더링까지 Rust가 담당해야 한다면 `windows` 크레이트로 Direct3D를 사용하고, C++ WinUI 셸과는 공유 텍스처 핸들이나 명확한 COM 포인터 소유권 규칙을 통해 연결할 수 있다. FFI는 픽셀이나 브러시 포인트마다 호출하지 않고 문서 열기, 명령 제출, 프레임 렌더링처럼 굵은 단위로 설계해야 한다.

모든 코드를 Rust로 유지하는 것이 최우선이면 WinUI 3보다 `windows-rs + Win32 + Direct3D`에 Dear ImGui 계열 Rust 바인딩을 조합하는 편이 구현 위험이 낮다.

결론은 **WinUI 3을 꼭 사용한다면 C++/WinRT 셸 + Rust 코어**, **순수 Rust가 우선이면 WinUI 3을 사용하지 않는 구성**이다.

참고 자료:

- [Microsoft의 WinUI 3 지원 언어 안내](https://learn.microsoft.com/en-us/windows/apps/)
- [Microsoft windows-rs](https://github.com/microsoft/windows-rs)
- [Rust for Windows 공식 안내](https://learn.microsoft.com/en-us/windows/dev-environment/rust/rust-for-windows)

---

## 질문 (00:32:37 KST)

설계된 기능은 아직 구현하지 않은 것인가?

### 답변

그렇다. 현재 구현된 범위는 C++23/WinUI 3 프로젝트 골격, 교체 가능한 프런트엔드 경계, 최소 문서 생성 흐름과 헤드리스 테스트다. 다중 문서, 레이어·블렌딩·마스크·채널·선택, 조정·필터, Crop·Canvas Resize·Image Resample, `.ocp`, PNG/JPEG와 PSD 호환 기능은 설계 문서로만 정의되어 있고 실제 기능 구현은 아직 시작 전이다.
