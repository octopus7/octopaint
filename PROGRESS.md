# OctoPaint 진행 현황

최종 업데이트: 2026-08-06 KST

이 체크리스트는 저장소 수준의 구현 현황을 나타냅니다. 완료된 항목은 `[x]`로 표시하고, 진행 중이거나 대기 중인 작업은 체크하지 않은 채 상태 표기를 덧붙입니다.

## 프로젝트 기반

- [x] C++23 Visual Studio 2022 x64 솔루션
- [x] 교체 가능한 WinUI 3 프런트엔드 경계
- [x] UI 중립 Application API와 플랫폼 독립 Core
- [x] 제품, 편집기 아키텍처 및 파일 형식 설계 문서
- [x] 영어, 한국어 및 일본어 README 문서
- [x] 생성된 네이티브 빌드 및 Windows 패키징 출력물에 대한 저장소 제외 규칙
- [x] 후보 기능 검토 문서 `CANDIDATE_FEATURES.md`

## 문서 및 작업 기록

- [x] 안정적이며 재사용되지 않는 문서 ID를 사용하는 다중 문서 열기
- [x] 활성 문서 전환 및 닫기
- [x] 문서별 실행 취소 및 다시 실행 기록
- [x] 문서별 저장 리비전 및 변경 상태 추적
- [x] 문서 작업 기록 및 스냅샷에 레이어 명령 통합
- [ ] 자동 저장 및 충돌 복구 저널

## 핵심 편집 워크플로

- [ ] 잘라내기·복사·붙여넣기 및 외부 클립보드 이미지 교환
- [ ] 새 안정 ID와 COW 타일 공유를 사용하는 문서·레이어 복제
- [ ] 사용자 지정 단축키 저장 및 scope별 충돌 검사
- [ ] UI 중립 명령 레지스트리를 사용하는 Command Palette
- [ ] 버전형 환경설정 저장·마이그레이션·초기화
- [ ] 순서가 보존되는 복수 파일 및 이미지 Drag & Drop 열기·가져오기
- [ ] 문서별 Undo History 스냅샷 패널 및 특정 이력 시점 이동

## 래스터 및 레이어 코어

- [x] 고정 크기 256×256 premultiplied RGBA8 타일
- [x] 희소 타일 할당 및 투명 타일 제거
- [x] 불변 타일 페이로드 공유 및 타일 단위 Copy-on-Write
- [x] 안정적인 ID를 사용하는 래스터 및 그룹 레이어
- [x] 새 문서 최초 래스터 레이어의 불투명 흰색 캔버스 초기화
- [x] 삽입, 제거 및 이동을 검증하는 순서형 중첩 레이어 트리
- [x] 블렌딩 모드 도메인 계약
- [x] 도메인, 스냅샷, 명령 및 테스트에 레이어 불투명도/투명도 잠금 통합
- [ ] 래스터 레이어 마스크 및 클리핑 관계
- [ ] 합성, RGB, 알파 및 이름 지정 채널
- [ ] Merge Down·Merge Visible·Flatten 명령
- [ ] 복수 레이어 선택 및 원자적 일괄 속성 변경
- [ ] 편집 가능한 Text Layer
- [ ] 단색·gradient·pattern Fill Layer
- [ ] 비파괴 Layer Effects
- [ ] 레이어 색상 태그·검색·필터
- [ ] embedded/external Linked·Smart Object 계열 레이어

## 페인팅 및 선택 도구

- [x] 앤티앨리어싱 없는 연결 픽셀 방식 Pencil 엔진 및 테스트
- [x] 결정론적 시간/압력 기반 Airbrush 엔진 및 테스트
- [x] 브러시 크기·경도·간격·Flow·Opacity 옵션
- [x] 압력에 따른 dab size 및 opacity 적용 토글
- [x] Stroke Stabilizer
- [x] Pencil/Airbrush 스트로크의 Application 원자적 적용·Undo/Redo 및 BGRA 스냅샷
- [x] 활성 도구·전경/배경색·브러시·압력·Stabilizer 설정을 보관하는 UI 중립 `EditorState`
- [ ] soft/hard tip을 지원하는 일반 Paint Brush
- [ ] 색 alpha 및 마스크·채널 coverage를 대상으로 하는 Eraser
- [ ] 활성 레이어·합성 결과를 선택할 수 있는 Eyedropper
- [ ] tile 경계를 넘는 tolerance 기반 Paint Bucket
- [ ] Linear·Radial Gradient 도구와 preview transaction
- [ ] 버전형 Brush Preset 관리·가져오기·내보내기
- [ ] 단일 Undo를 보장하는 대칭·미러 페인팅
- [ ] 고정 source revision 기반 Clone Stamp 및 Healing
- [x] 사각형 및 타원형 선택 윤곽 마스크 생성
- [x] 자유형 및 다각형 올가미 마스크 생성
- [x] 문서별 선택 상태·Undo/Redo 및 픽셀 경계 스냅샷
- [x] Move Layer 명령 통합
- [ ] 선택 영역 바꾸기, 추가, 빼기 및 교차 모드
- [ ] 도구 미리보기 오버레이 및 제스처당 단일 명령 확정

## WinUI 편집기 셸

- [x] 네이티브 `OctoPaint.exe` 및 `OctoPaint` 창 제목
- [x] 기본 문서 생성 셸
- [x] 다중 문서 탭 표시
- [x] 상단 메뉴바 아래 Tool Options Bar
- [x] 스타일러스 감도 설정 대화상자
- [x] 상호 배타적인 활성 도구를 제공하는 왼쪽 세로 도구 모음
- [x] 교체/초기화 기능이 있는 겹쳐진 전경색/배경색 견본
- [x] RGB 및 16진수 값과 동기화되는 HSV/알파 색상 선택기
- [x] WinUI 도구·색상·Brush·압력·Stabilizer 컨트롤과 Application `EditorState` 양방향 연결
- [x] 캔버스 포인터 캡처·스타일러스 압력 수집과 Pencil/Airbrush 스트로크 확정·재표시
- [x] 캔버스 이탈·재진입 구간 단절, 입력 설정 고정 및 포인터 수명 안정화
- [x] Tools/Layers 패널의 MainWindow 도킹·부유 창 분리·자동 재도킹
- [x] 도킹 전환 중복 억제, 창 이벤트 수명 보호 및 실패 복구
- [ ] 레이어 패널 표시 및 레이어 속성 컨트롤
- [ ] 캔버스 뷰포트, 확대/축소, 이동, 눈금자 및 안내선

## 렌더링 및 이미지 작업

- [x] SwapChainPanel 기반 Direct3D 11/Direct2D 캔버스 렌더러와 WARP 폴백
- [x] Windows 10 DXGI/SwapChain COM 호환성과 실행 직후 생존 검증
- [x] 이미지와 분리된 애니메이션 Marching Ants 선택 경계 오버레이
- [ ] CPU 정확성 참조 합성 경로 및 Direct3D 결과 일치 검증
- [ ] 블렌딩 모드 합성기
- [ ] 명도/대비 조정
- [ ] 색조/채도 및 색상 조정
- [ ] Curves 조정
- [ ] Desaturation 조정
- [ ] Gaussian Blur
- [ ] Crop
- [ ] 9방향 기준점 Canvas Resize
- [ ] 비율 및 픽셀 단위 Image Resampling

## 저장 및 상호운용성

- [ ] 검증 및 원자적 교체를 지원하는 고유 레이어 형식 `.ocp` 읽기/쓰기
- [ ] PNG 읽기/쓰기
- [ ] 명시적 투명도 병합을 지원하는 JPEG 읽기/쓰기
- [ ] 레이어를 인식하는 PSD 읽기/쓰기
- [ ] PSD 호환성 및 손실 변환 보고서

## 빌드, 패키징 및 검증

- [x] 헤드리스 Core 테스트
- [x] Application 다중 문서/작업 기록 테스트
- [x] 희소 타일 및 레이어 도메인 테스트
- [x] 페인팅·선택·압력 감도·Stroke Stabilizer 테스트
- [x] Application 레이어 명령 및 문서 격리 테스트
- [x] Application 편집 도구 상태 및 입력 검증 테스트
- [x] Application Pencil/Airbrush 적용·Undo/Redo·BGRA 스냅샷 테스트
- [x] 릴리스 배치에서 6개 headless 테스트 실행 및 실패 전파
- [x] `VERSION` 버전 정보 원본
- [x] 버전을 포함한 `OctoPaint-<version>-win-x64.zip` 빌드 흐름
- [x] 버전을 포함한 `OctoPaint-<version>-win-x64.msi` WiX 빌드 정의
- [x] ZIP 내용 검증
- [ ] WiX Toolset 5 이상이 설치된 컴퓨터에서 MSI 컴파일 검증
- [ ] 지속적 통합 빌드 및 테스트 워크플로
- [ ] 성능 및 대용량 문서 스트레스 테스트

## 현재 제공 범위

- [ ] 🚧 도구 엔진, WinUI 도구 모음/색상 선택기 및 문서-레이어 명령 완성 및 통합
- [x] 전체 Debug x64 솔루션 빌드 및 모든 헤드리스 테스트 실행
- [ ] 독립적으로 완료된 각 작업 단위를 커밋한 뒤 즉시 푸시
