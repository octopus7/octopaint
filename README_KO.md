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
- `tests/OctoPaint.Core.Tests`: 애플리케이션과 코어의 헤드리스 검증
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

헤드리스 검증은 다음과 같이 실행합니다.

```powershell
.\out\bin\x64\Debug\OctoPaint.Core.Tests\OctoPaint.Core.Tests.exe
```

## 프로젝트 상태

현재 저장소에는 초기 C++23 솔루션 스캐폴드와 첫 프런트엔드 독립 작업 공간 흐름이 구현되어 있습니다. 위의 광범위한 편집 및 상호 운용 기능은 설계 목표이며 단계적으로 구현할 예정입니다.
