[EN](README.md) | [JA](README_JA.md)

# OctoPaint

OctoPaint는 C++23으로 개발하는 Windows 네이티브 이미지 편집기입니다. 첫 프런트엔드는 WinUI 3를 사용하지만, 편집기 코어, 애플리케이션 API, 렌더링 계약, 파일 형식 경계는 특정 UI 프레임워크에 종속되지 않도록 설계합니다.

제품명, 실행 파일명, 창 제목은 모두 `OctoPaint`입니다.

## 계획된 기능

- 다중 문서 편집
- 레이어, 그룹, 블렌딩 모드, 래스터 마스크, 클리핑 관계
- 합성, RGB, 알파, 추가 사용자 지정 채널
- 선택 영역 연산과 저장된 선택 영역
- 명도/대비, 색조/채도, 커브, 채도 제거 조정
- 가우시안 블러를 포함한 이미지 필터
- 크롭, 9방향 기준점 캔버스 크기 조정, 비율 또는 픽셀 단위 이미지 리샘플링
- 레이어를 보존하는 고유 `.ocp` 문서
- PNG 및 JPEG 가져오기/내보내기
- 호환성 보고를 포함한 레이어 기반 PSD 가져오기/내보내기

## 아키텍처

```text
OctoPaint.WinUI (교체 가능한 WinUI 3 프런트엔드)
        |
        v
OctoPaint.Application (UI 중립 명령과 스냅샷)
        |
        v
OctoPaint.Core (플랫폼 독립 문서 도메인)
```

`OctoPaint.Core`는 C++23 표준 라이브러리만 사용합니다. WinUI, WinRT, Win32, Direct3D 및 기타 프런트엔드 전용 형식은 공개 인터페이스 외부에 둡니다. 향후 다른 프런트엔드는 문서 또는 편집기 동작을 다시 작성하지 않고 `OctoPaint.Application`을 사용할 수 있습니다.

## 저장소 구조

- `src/OctoPaint.Core`: 플랫폼 독립 문서 모델
- `src/OctoPaint.Application`: UI 중립 명령과 불변 스냅샷
- `src/OctoPaint.WinUI`: 교체 가능한 WinUI 3 어댑터와 `OctoPaint` 실행 파일
- `tests`: Core, Application, 도메인, 도구, 레이어, 합성, 편집기 상태, 페인팅, 선택을 검증하는 9개 헤드리스 테스트 실행 파일
- `docs`: 아키텍처, 제품, 편집기, 파일 형식 설계 문서

## 설계 문서

- [아키텍처와 프런트엔드 교체 규칙](docs/ARCHITECTURE.md)
- [제품 요구사항](docs/PRODUCT_REQUIREMENTS.md)
- [편집기 아키텍처](docs/EDITOR_ARCHITECTURE.md)
- [파일 형식과 상호 운용성](docs/FILE_FORMATS.md)

## 빌드

Visual Studio 2022에서 `OctoPaint.sln`을 열고 `x64` 플랫폼을 선택하거나 다음 명령을 실행합니다.

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" .\OctoPaint.sln /restore /m /p:Configuration=Debug /p:Platform=x64
```

빌드 후 9개 헤드리스 검증 전체를 다음과 같이 실행합니다.

```powershell
$tests = @(
  "OctoPaint.Core.Tests", "OctoPaint.Application.Tests", "OctoPaint.Core.Domain.Tests",
  "OctoPaint.Tools.Tests", "OctoPaint.Application.Layer.Tests", "OctoPaint.Application.Composite.Tests",
  "OctoPaint.Application.EditorState.Tests", "OctoPaint.Application.Paint.Tests", "OctoPaint.Application.Selection.Tests"
)
foreach ($test in $tests) {
  & ".\out\bin\x64\Debug\$test\$test.exe"
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

## 릴리스 패키지

[WiX Toolset 5 이상](https://docs.firegiant.com/wix/using-wix/)을 설치하고 `VERSION`을 `major.minor.patch` 형식으로 갱신한 뒤 다음 파일을 실행합니다.

```bat
build-release.bat
```

스크립트는 의존성을 복원하고 자체 포함 Release x64 앱과 9개 헤드리스 테스트를 빌드한 뒤 해당 테스트를 실행하고 아래 파일을 생성합니다.

- `out\release\OctoPaint-<version>-win-x64.zip`
- `out\release\OctoPaint-<version>-win-x64.msi`

현재는 산출물 존재 여부만 확인합니다. 패키징된 앱 실행, ZIP 내부 검사·재추출, MSI 설치·업그레이드·복구·제거는 검증하지 않습니다.

## 프로젝트 상태

현재 저장소에는 동작하는 인메모리 편집 세로 슬라이스가 있습니다. 문서별 Undo/Redo가 있는 다중 문서 상태, 희소 타일 Raster/Group 레이어, 16개 블렌딩 모드 CPU 합성, Pencil/Airbrush, replace 방식 선택 도구 4종, WinUI/D3D 캔버스와 레이어 컨트롤이 구현되어 있습니다. 영구 Open/Save와 파일 형식, dirty-close 보호, viewport 탐색, 실제 Move Layer 픽셀 이동 및 계획된 편집 기능 대부분은 아직 미완성입니다. 소스 감사 기준의 현황과 우선순위는 [PROGRESS.md](PROGRESS.md)를 참고하세요.
