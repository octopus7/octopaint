# OctoPaint 진행 현황

최종 업데이트: 2026-08-10 KST

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
- [x] 도메인, 스냅샷, 명령 및 테스트에 레이어 표시·불투명도·잠금·투명도 잠금·블렌드 모드 통합
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
- [x] 활성 선택 마스크로 Pencil/Airbrush 적용 및 dirty bounds 제한
- [ ] Move Layer 픽셀 이동 명령과 캔버스 제스처 연결 — 현재 `MoveLayerCommand`는 레이어 트리의 재배치만 수행하며 도구 버튼은 픽셀 이동을 실행하지 않음
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
- [x] 사각형·타원·자유형·다각형 선택 제스처와 애니메이션 Marching Ants 표시 연결
- [x] Tools/Layers 패널의 MainWindow 도킹·부유 창 분리·자동 재도킹
- [x] 도킹 전환 중복 억제, 창 이벤트 수명 보호 및 실패 복구
- [x] 레이어 패널 표시 및 활성화·추가·삭제·표시·불투명도·잠금 컨트롤
- [x] 문서별 Undo/Redo 메뉴 상태·레이블·실행 연결
- [ ] 캔버스 뷰포트, 확대/축소, 이동, 눈금자 및 안내선

## 렌더링 및 이미지 작업

- [x] SwapChainPanel 기반 Direct3D 11/Direct2D 캔버스 렌더러와 WARP 폴백
- [x] Windows 10 DXGI/SwapChain COM 호환성과 실행 직후 생존 검증
- [x] 이미지와 분리된 애니메이션 Marching Ants 선택 경계 오버레이
- [x] 결정적 premultiplied BGRA8 CPU 정확성 참조 합성 경로
- [ ] CPU 참조 합성과 Direct3D 결과 일치 검증
- [x] 16개 블렌딩 모드 합성기
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
- [x] 릴리스 배치에서 9개 headless 테스트 실행 및 실패 전파
- [x] `VERSION` 버전 정보 원본
- [x] 버전을 포함한 `OctoPaint-<version>-win-x64.zip` 빌드 흐름
- [x] 버전을 포함한 `OctoPaint-<version>-win-x64.msi` WiX 빌드 정의
- [ ] ZIP 내용 검증 — 현재 릴리스 배치는 staging의 `OctoPaint.exe`와 생성된 ZIP 존재만 확인하며 archive 내부 목록·재추출·실행은 검증하지 않음
- [ ] WiX Toolset 5 이상이 설치된 컴퓨터에서 MSI 컴파일 검증
- [ ] 지속적 통합 빌드 및 테스트 워크플로
- [ ] 성능 및 대용량 문서 스트레스 테스트

## 현재 제공 범위

- [ ] 🚧 도구 엔진, WinUI 도구 모음/색상 선택기 및 문서-레이어 명령 완성 및 통합
- [x] 전체 Debug x64 솔루션 빌드 및 모든 헤드리스 테스트 실행
- [ ] 독립적으로 완료된 각 작업 단위를 커밋한 뒤 즉시 푸시

## 2026-08-10 소스 구현 감사

### 기준과 검증 한계

- 기준 커밋: `878d098` (`feat: composite layers and wire layer controls`)
- 실제 소스, 공개 API, WinUI 이벤트 연결, 렌더 경로, 9개 테스트 실행 파일, 릴리스 배치 및 기존 요구사항을 대조했다.
- frontend-neutral Core/Application 소스와 9개 headless test main을 WSL의 `g++ 15.2.0 -std=c++23`으로 빌드해 모두 통과했다.
- 같은 9개 실행 파일은 ASan+UBSan과 leak detection에서도 모두 통과했다. 다만 `-Wall -Wextra -Wpedantic -Werror` 빌드는 `Workspace.cpp`와 일부 테스트의 `-Wmissing-field-initializers`에서 실패해 warning-clean gate는 아직 통과하지 못했다.
- 프로젝트·WiX·manifest·XAML XML 파일 16개는 구조 파싱을 통과했지만 이는 XAML/WiX 컴파일이나 Windows 실행 검증을 대신하지 않는다.
- 이번 감사 환경에는 Visual Studio, Windows App SDK, WinUI 3 런타임이 없으므로 정식 MSVC 솔루션 빌드·GUI 실행·MSI 설치는 다시 수행하지 않았다. 2026-08-09 Windows Debug/Release 빌드, 9개 headless 테스트 및 실행 시작 검증은 `REQUESTS.md`의 기존 기록을 근거로만 구분해 유지한다.

### 현재 실제로 연결된 세로 슬라이스

- `Workspace`의 다중 문서 생성·활성화·닫기, 문서별 revision·dirty 상태·Undo/Redo와 Raster/Group 레이어 명령이 구현되어 있다.
- 256×256 희소 타일과 불변 payload 공유, Pencil/Airbrush 래스터화, 압력·간격·불투명도·Stabilizer, 선택 영역 제한 및 alpha lock이 Application 명령 경계까지 연결되어 있다.
- 사각형·타원·자유형·다각형 선택은 replace 방식으로 확정되고 문서 history와 애니메이션 Marching Ants 오버레이에 연결되어 있다.
- 전체 Raster/Group 트리의 표시·불투명도·16개 blend mode를 CPU에서 합성하고 WinUI 캔버스가 활성 레이어 대신 합성 스냅샷을 표시한다.
- WinUI에서 문서 탭, Undo/Redo, 기본 레이어 추가·삭제·활성화·표시·불투명도·잠금, Pencil/Airbrush와 선택 도구, 색상 및 입력 옵션, 패널 도킹/부유가 실제 handler에 연결되어 있다.

### 구현은 일부 있으나 제품 흐름이 끝나지 않은 부분

1. **P0 — 저장과 데이터 유실 방지**
   - `Workspace`에는 새 문서와 `MarkSaved`만 있고 파일 경로, serializer, open/save/save-as port가 없다.
   - File 메뉴의 Open, Save, Save As에는 handler가 없고 dirty 탭도 확인 없이 `CloseDocument`를 호출한다.
   - `Workspace::ExecuteCommand`는 명령으로 문서를 먼저 변경한 뒤 redo 제거와 history `push_back`을 수행한다. Paint와 Selection 경로와 달리 사전 `reserve`나 일반 rollback 경계가 없어 history 할당 실패 시 변경만 남고 revision/Undo 기록이 누락될 수 있다. `LayerTree::Move`도 source를 먼저 제거한 뒤 할당 가능한 destination insert를 수행해 실패 시 subtree를 잃을 수 있다.
   - `MarkSaved(DocumentId)`는 실제로 기록된 불변 revision의 영수증 없이 호출 시점의 current revision을 clean으로 표시한다. 저장 중 추가 편집이 생기면 아직 기록되지 않은 revision까지 clean으로 잘못 표시할 수 있으므로 persistence 도입 전에 `SaveReceipt` 기반 계약이 필요하다.
   - `docs/FILE_FORMATS.md`는 안정 UUID, 문서 그래프, 원자 저장과 복구를 상세히 정의하지만 현재 `DocumentId`·`LayerId`는 process-unique `uint64_t`이고 Core `Document`는 title과 canvas size만 보관하므로 구현 모델과 영구 형식 사이의 매핑 계약부터 확정해야 한다.
   - `.ocp`, PNG, JPEG, PSD 구현과 자동 저장·복구 저널도 아직 없다.
2. **P1 — 화면에 보이지만 실행되지 않는 명령과 도구**
   - 25개 `MenuFlyoutItem` 중 handler가 연결된 것은 New, Undo, Redo, New Layer, New Group, Delete Layer의 6개뿐이며 나머지 19개는 표시 전용 항목이다.
   - New도 대화상자 없이 항상 1920×1080 문서를 생성하므로 사용자가 캔버스 크기나 초기 배경을 선택할 수 없다.
   - 키보드 accelerator도 Polygonal Lasso 확정·취소용 Enter/Escape만 있어 Undo/Redo를 포함한 일반 편집 단축키가 없다.
   - Move Layer 도구는 선택 상태만 바뀐다. 현재 동명의 Application 명령은 부모·sibling 순서 재배치이며 요구사항의 픽셀 translation과 다르다.
   - 레이어 이름·순서·alpha lock·blend mode 명령은 backend에 있으나 현재 Layers 패널에서 직접 조작할 수 없다.
3. **P1 — 캔버스 상호작용과 피드백**
   - 렌더러는 항상 패널에 맞춰 중앙 정렬하며 사용자 zoom, pan, reset-view 상태와 입력 경로가 없다.
   - 페인트는 pointer release 때 한 번 반영되어 스트로크 중 preview가 없고, Move/selection도 공통 preview transaction 계약이 없다.
   - Airbrush의 시간 누적은 pointer event의 경과 시간만 사용하며 별도 timer가 없어 포인터를 정지한 동안에는 분사가 계속 누적되지 않는다.
   - 감도 대화상자의 Stabilizer Smoothing은 `EditorState`에는 저장되지만 `PaintStrokeRequest`에는 strength만 전달되어 실제 스트로크에 적용되지 않는다.
   - 잠긴 레이어나 Group에 페인팅하는 등의 거부 결과가 UI에서 명확한 진단으로 표시되지 않는다.
4. **P1 — 선택과 편집 제약의 나머지**
   - 선택 coverage는 현재 binary이고 결합은 replace만 가능하다. add/subtract/intersect, invert, clear, feather, expand, contract, 채널 저장·복원은 없다.
   - 레이어 mask·clipping·channel 및 edit-target 모델이 없어 요구사항의 전체 페인팅 제약을 아직 만족하지 못한다.
5. **P1 — 성능 및 품질 게이트**
   - `RefreshView`마다 전체 캔버스 크기의 CPU 합성 스냅샷과 Group 중간 버퍼를 다시 만들며 dirty-tile cache나 GPU 결과 일치 검증이 없다.
   - history는 byte budget·checkpoint·eviction 없는 무제한 vector이고 paint 명령은 tile payload를 공유하더라도 전체 sparse-store metadata를 before/candidate/after로 복제한다. opacity 연속 변경도 preview 한 건이 아니라 값 변경마다 history를 추가한다.
   - blend mode enum은 mutation 경계에서 범위를 검증하지 않아 잘못된 값이 문서에 들어간 뒤 합성 시점에 실패할 수 있다. Paint 결과는 무시되고 selection 예외는 조용히 삼켜져 사용자에게 원인과 복구 방법이 보이지 않는다.
   - 릴리스 배치는 9개 headless 테스트와 산출물 존재 여부를 확인하지만 ZIP 내부 내용, 저장·파일 손상·dirty-close·실제 WinUI 상호작용 테스트와 지속적 통합 워크플로는 없다.
   - 패키지는 signing, checksum, MSI 설치·실행·제거 smoke gate가 없고 `VERSION`을 실행 파일 version resource와 동기화하는 경로도 없다.
6. **P2 — 확정 범위의 미착수 기능군**
   - Paint Brush, Eraser, Eyedropper, Bucket, Gradient, preset, symmetry, Clone/Healing.
   - clipboard, 문서·레이어 복제, command registry·palette, 단축키, 환경설정, Drag & Drop, Undo History 패널.
   - 조정, Gaussian Blur, Crop, Canvas Resize, Image Resampling, mask·channel, Merge 계열, 고급 레이어와 PSD 호환성 보고.
   - canvas keyboard interaction, live accessibility status, marching ants의 reduce-motion 적용, UI 문자열 resource화와 실제 UI localization.

### 권장 구현 순서와 완료 기준

1. **P0-1 명령·history 원자성**: 일반 `ExecuteCommand`, Undo, Redo와 cross-parent `LayerTree::Move`가 실패해도 문서·layer tree, history position, revision과 redo branch가 함께 원상태를 유지하도록 reserve/transaction/rollback 계약을 통일한다. fault-injection으로 모든 할당·명령 실패 지점에서 상태 동일성과 subtree 보존을 검증한다.
2. **P0-2 `.ocp` 영속성 기반**: 새 형식을 다시 설계하지 말고 `docs/FILE_FORMATS.md`를 권위 문서로 사용한다. 먼저 process ID와 영구 UUID 매핑, 현재 구현된 최소 문서 그래프, 미지원 required/optional feature 처리와 history 저장 여부를 확정한 뒤 serializer를 구현한다. 실제로 기록한 불변 revision을 담는 `SaveReceipt`만 clean 처리할 수 있어야 하며, save-load-save와 손상·중단 시 마지막 정상 파일 보존 테스트를 완료 기준으로 한다.
3. **P0-3 Open/Save/Save As 및 dirty-close**: frontend-neutral 파일·interaction port와 WinUI picker를 연결하고 탭 닫기·창 종료에서 Save/Discard/Cancel을 강제한다. 취소·실패가 문서나 마지막 정상 파일을 바꾸지 않아야 한다.
4. **P1-1 viewport와 실제 Move Layer**: 문서별 zoom/pan/reset 상태, 좌표 역변환, 픽셀 layer translation과 preview/commit을 구현한다. 선택 경계와 포인터 좌표가 모든 배율에서 일치하고 한 제스처가 한 Undo가 되어야 한다.
5. **P1-2 페인팅 입력 완결**: release 전 preview, 정지 상태 Airbrush timer 누적, Smoothing 전달과 실패 진단을 연결한다. preview와 commit 결과가 같고 취소 시 history·픽셀이 변하지 않아야 한다.
6. **P1-3 선택·기본 편집 명령 완결**: 새 문서 크기·배경 선택, 선택 결합·반전·해제, Cut/Copy/Paste, 레이어 복제와 명령 상태를 공통 registry로 연결한다. 메뉴에 보이는 항목은 실행되거나 명시적으로 disabled 상태여야 한다.
7. **P1-4 입력 검증과 진단**: blend mode 등 enum을 mutation 전에 검증하고 generic command·paint·selection·render 실패를 안정적인 diagnostic code와 복구 메시지로 반환한다. 거부된 요청은 상태와 revision을 바꾸지 않으며 catch-all이 성공처럼 보이는 no-op을 만들지 않아야 한다.
8. **P1-5 Windows CI와 회귀 검증**: Debug/Release x64 빌드, 9개 headless 테스트, ZIP 내용 검증을 Windows runner에서 자동화하고 별도 WinUI smoke 시나리오를 둔다. WiX gate에서는 MSI 생성·설치·실행·upgrade·repair·제거를 검증하며, 릴리스 단계에서 `VERSION` 동기화, checksum과 코드 서명을 확인한다. portable compiler의 missing-field initializer 경고를 정리한 뒤 warnings-as-errors와 case별 machine-readable 결과도 gate로 둔다.
9. **P1-6 합성·history 자원 예산과 GPU 일치**: dirty tile/revision 기반 cache, changed-bounds 소비, history byte budget·checkpoint를 도입한 뒤 CPU 기준 결과와 GPU 결과 허용 오차, 16K·deep-group 문서의 peak memory·latency·업로드 타일 수를 자동 검증한다.
10. **P2-1 접근성·현지화 완결**: canvas keyboard editing, focus·automation summary, live status와 reduce-motion을 연결하고 user-facing 문자열을 resource로 이동한다. 영어·한국어·일본어 fallback과 primary workflow를 자동 검증한다.
11. **P2-2 기능 확장**: 일반 Brush·Eraser·Eyedropper부터 시작해 Bucket·Gradient, mask·channel, 조정·필터·기하 순으로 세로 슬라이스를 완성한다.
12. **P2-3 상호운용성**: `.ocp` 안정화 후 PNG/JPEG를 연결하고 마지막에 PSD 구조 보존과 손실 변환 보고를 구현한다.

### 병렬 구현 시 소유권 제안

공유 seam인 `Workspace.h/.cpp`, `MainWindow.xaml/.cpp`, 솔루션 파일과 이 문서는 통합 담당자 한 명만 수정한다. 첫 병렬 단계는 다음처럼 겹치지 않게 나눌 수 있다.

- 저장 형식 담당: 새 Core serializer 파일과 전용 round-trip/손상 테스트.
- viewport 담당: `D3DCanvasRenderer.*`의 view transform과 renderer 단위 검증.
- 선택 담당: Core 선택 결합 연산과 `OctoPaint.Tools.Tests`.
- 품질 담당: `.github/workflows`와 Windows 빌드·테스트·패키지 검증 스크립트.

각 결과를 통합 담당자가 Application/WinUI에 순서대로 연결하고 전체 Windows gate를 통과시킨 뒤 다음 병렬 단계를 시작한다.
