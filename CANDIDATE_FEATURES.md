# OctoPaint 기능 후보 검토 목록

> 이 문서는 구현 승인이 아니라 사용자 검토를 위한 후보 목록이다. 여기에 적힌 기능을 자동으로 구현하거나 현재 범위로 간주해서는 안 된다.

사용자가 후보를 명시적으로 승인한 뒤에만 다음 순서로 승격한다.

1. 채택된 기능을 `docs/PRODUCT_REQUIREMENTS.md`의 요구사항과 `docs/EDITOR_ARCHITECTURE.md`의 경계·데이터 계약에 반영한다.
2. 구현 단위와 완료 기준을 정한 뒤 `PROGRESS.md`에 추적 항목을 추가한다.
3. 위 문서 반영이 끝난 뒤 별도의 구현 요청으로 작업을 시작한다.

각 후보의 검토 결정에서 하나만 선택한다: `[ ] 채택  [ ] 보류  [ ] 제외`.

## 기존 확정 범위와의 구분

다음 항목은 이미 제품 요구사항, 진행표 또는 사용자의 확정 구현 지시에 포함되어 있으므로 이 문서의 후보가 아니다: 멀티 문서, 문서별 Undo/Redo, 저장 상태 추적, 자동 저장·충돌 복구 저널, 픽셀/그룹/조정 레이어, 레이어 마스크·클리핑·블렌딩 모드, 채널, 선택 영역 결합·반전·페더·확장·축소, Pencil/Airbrush, 브러시 크기·경도·간격·Flow·Opacity, 스타일러스 압력 동역학과 감도 설정, Stroke Stabilizer, Move Layer, alpha lock, 색 조정과 Curves, Gaussian blur, Crop, 9방향 Canvas Resize, Image Resampling, `.ocp`, PNG, JPEG, PSD, D3D11 렌더링과 CPU 대체 경로.

아래 후보가 이 기능들과 연결될 때는 `existing scope`로 표기하고 새 요구사항과 기존 요구사항의 경계를 함께 검토한다.

## Essential workflow

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| 잘라내기·복사·붙여넣기와 클립보드 이미지 교환 | 다른 앱 및 문서 사이에서 픽셀과 선택 내용을 이동하는 가장 기본적인 편집 흐름이다. | 선택 영역과 레이어 명령(`existing scope`), Windows 클립보드 어댑터, 대용량 데이터 지연 복사. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 문서·레이어 복제 명령 | 반복 작업과 변형 전 안전한 사본 생성을 빠르게 한다. | 안정 ID 재발급, Copy-on-Write 타일, 명령/Undo(`existing scope`). 비용: 낮음~중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 사용자 지정 단축키와 충돌 검사 | 전문 편집 작업의 속도를 높이고 다른 도구에서 전환하는 사용자의 키맵을 수용한다. | frontend-neutral command ID, 설정 저장, 키보드 레이아웃별 충돌 UI. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 명령 검색 또는 Command Palette | 메뉴 위치를 몰라도 기능을 찾아 실행할 수 있고 키보드 중심 사용성을 높인다. | 명령 레지스트리, 활성 조건, 지역화된 검색어. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 환경설정 저장과 초기화 | 단축키, 도구 기본값, 성능·색관리 설정을 세션 간 유지한다. | 버전이 있는 사용자 설정 스키마, 손상 시 기본값 복구, frontend-neutral 설정 경계. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 파일 및 이미지 Drag & Drop | 탐색기에서 열기·가져오기를 자연스럽게 수행한다. | 파일 열기/가져오기 포트(`existing scope`), WinUI 드롭 어댑터, 복수 파일 정책. 비용: 낮음~중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Undo History 패널과 특정 시점 이동 | 여러 단계를 한눈에 확인하고 원하는 revision으로 빠르게 돌아가게 한다. | 문서별 history(`existing scope`)의 안전한 읽기 스냅샷, 분기 이력 정책. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Painting

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| 일반 Paint Brush와 soft/hard tip | Pencil과 Airbrush 사이의 표준 불투명도 기반 회화 도구가 현재 확정 범위에 없다. | 공통 dab 엔진, 압력·간격·경도, preview/gesture transaction(`existing scope`). 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Eraser 도구 | 투명 픽셀 제거와 마스크 편집의 기본 수단이다. | Brush 엔진, alpha lock(`existing scope`)과 선택/마스크 결합 규칙. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Eyedropper와 합성/활성 레이어 샘플링 | 캔버스에서 직접 색을 얻는 핵심 색 선택 흐름이다. | 렌더 스냅샷 또는 CPU 합성 샘플 포트, foreground color 상태(`existing scope`). 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Paint Bucket과 유사색 허용치 | 닫힌 영역이나 유사색 영역을 빠르게 채운다. | 연결 영역 탐색, 타일 경계 처리, selection/mask/alpha lock(`existing scope`). 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Gradient 도구 | 단색 채우기만으로 만들기 어려운 부드러운 명암과 마스크를 생성한다. | 선형·방사형 gradient 파라미터, 색 보간 정책, preview transaction. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Line 및 기본 Shape 도구 | 직선, 사각형, 타원 같은 반복적인 도형 작성을 지원한다. | 래스터화 규칙, stroke/fill 설정, antialias 정책, preview overlay. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Brush Preset 관리·가져오기·내보내기 | 자주 쓰는 브러시 설정을 재사용하고 공유할 수 있게 한다. | 버전 있는 preset 스키마, tip 자산 저장, 설정 마이그레이션. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 대칭·미러 페인팅 | 캐릭터·패턴 작업에서 반복 스트로크를 줄인다. | 하나의 입력을 복수 문서 좌표로 변환하는 gesture fan-out, 단일 Undo 보장. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Clone Stamp와 Healing | 사진 보정 및 텍스처 복원에 필요한 대표 도구다. | source anchor 상태, 주변 타일 샘플링, healing 알고리즘, 대형 문서 성능. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Selection / transform

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| Free Transform: 이동·크기·회전 | 선택 내용이나 레이어를 한 조작 안에서 배치하는 기본 편집 기능이다. | 부동 선택/preview transaction, 리샘플러(`existing scope`), 변형 핸들. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Skew·Distort·Perspective 변형 | 사진 합성 및 원근면 맞춤에 필요하다. | 일반 변환 행렬/호모그래피, 고품질 resampling, 큰 preview 캐시. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Magic Wand와 Select by Color | 색이 비슷한 영역을 수작업 lasso 없이 선택한다. | 타일 연결 영역, tolerance/color-distance 정책, selection 결합(`existing scope`). 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Quick Mask 편집 모드 | 선택 coverage를 일반 회화 도구로 정밀하게 수정한다. | selection mask와 edit target(`existing scope`), 오버레이 색/불투명도 설정. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 선택 경계에 Stroke·Fill 적용 | 선택 모양을 직접 선이나 면으로 변환하는 반복 작업을 줄인다. | 선택 mask, Brush/Fill 후보, 안쪽·가운데·바깥쪽 경계 정책. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 정렬·분배와 snapping | 여러 레이어를 캔버스, 선택, 가이드 기준으로 정확히 배치한다. | 복수 레이어 선택, bounds 계산, guide/grid(`existing scope`)와 명령 transaction. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Layers

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| Merge Down·Merge Visible·Flatten | 복잡한 레이어를 의도적으로 단순화하고 외부 출력 전 결과를 고정한다. | compositor와 blend/mask/adjustment(`existing scope`), 호환성 경고, Undo 메모리. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 복수 레이어 선택과 일괄 속성 변경 | 이동·정렬·가시성·잠금을 여러 레이어에 한 번에 적용한다. | layer selection state, composite command, 부분 실패 없는 transaction. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Text Layer와 텍스트 편집 | 이미지 편집기의 일반적인 제목·주석·디자인 작업을 지원한다. | DirectWrite shaping, 폰트 대체/포함 정책, 비파괴 text 모델, PSD 매핑. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Vector Shape Layer | 도형을 해상도 독립적으로 유지하고 나중에 수정할 수 있게 한다. | vector path 모델/renderer, fill/stroke, `.ocp` 및 PSD/SVG 변환. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Fill Layer | 단색·gradient·pattern을 비파괴 레이어로 유지한다. | 생성형 layer 노드, operator 파라미터, compositor와 포맷 저장. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Layer Effects | 그림자, 외곽선, glow 같은 일반 디자인 효과를 비파괴로 제공한다. | 효과 그래프, halo 계산, CPU/GPU 일치, PSD 호환성. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Layer 색상 태그·검색·필터 | 큰 문서의 레이어 탐색 속도를 높인다. | metadata 스키마, layer snapshot/UI 검색, `.ocp` 보존. 비용: 낮음~중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Linked 또는 Smart Object 계열 레이어 | 원본을 보존한 반복 배치와 외부 자산 갱신을 지원한다. | 중첩 문서/외부 참조, cache invalidation, 파일 이동·누락 처리, PSD 매핑. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |

## View / navigation

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| Navigator 패널 | 큰 캔버스에서 현재 viewport 위치와 전체 구도를 빠르게 파악한다. | renderer thumbnail/mip(`existing scope`), viewport 양방향 동기화. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Canvas view 회전과 수평 미러 보기 | 손목 방향을 바꾸지 않고 선을 긋거나 좌우 균형을 점검한다. 원본 픽셀은 변경하지 않는다. | viewport transform, pointer 좌표 역변환, overlay 일치. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Grid·Pixel Grid 표시와 snapping | 픽셀 아트 및 정밀 배치를 돕는다. | zoom-dependent overlay, grid origin/spacing 설정, snapping 후보와 공유 계약. 비용: 낮음~중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 같은 문서의 복수 View | 전체 모습과 확대 영역 또는 서로 다른 채널을 동시에 확인한다. | 아키텍처의 `ViewId` 구상, viewport별 상태, 문서 이벤트 fan-out. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Reference Image 패널 | 외부 이미지를 문서에 삽입하지 않고 색과 형태 참고용으로 고정한다. | 별도 자산 수명·색관리·레이아웃 저장, drag/drop. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Canvas-only·전체 화면 모드 | 작은 화면에서 캔버스 작업 공간을 최대화한다. | 패널 가시성 상태 저장, 접근 가능한 복귀 동작. 비용: 낮음 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Color management

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| ICC profile Assign·Convert UI와 정책 | 문서 profile metadata를 실제 색 변환 작업과 연결한다. | 기존 ICC 안정화 계획(`existing scope`), Little CMS급 변환기, 렌더/내보내기 일관성. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Soft Proof와 gamut warning | 인쇄나 대상 profile에서 재현되지 않는 색을 저장 전에 확인한다. | ICC 변환, proof intent, viewport shader/CPU 기준 결과. 비용: 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 16-bit integer 및 float 작업 문서 | 강한 조정·합성에서 banding과 clipping을 줄인다. | tile pixel format 일반화, 모든 operator/renderer/codec 경로, 메모리 증가. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Swatch·Palette 라이브러리와 최근 색 | 브랜드 색과 반복 색을 정확히 재사용한다. | 색 상태(`existing scope`), palette 저장·가져오기 형식, profile 변환. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 표시 색상 숫자 형식 확장 | 8-bit RGB 외에 0~1 float, HSL, CMYK 참고값 등을 확인하게 한다. | color conversion 서비스, 값의 profile/렌더 의존성 설명. 비용: 낮음~중간 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Reliability

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| 정상 종료 후 workspace session 복원 | 충돌 복구와 별개로 열려 있던 문서·탭·viewport를 다음 실행에서 이어간다. | 최근 파일/dirty 상태 정책, 누락 파일 처리, 사용자 opt-out. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 선택적 버전 백업 또는 rolling backup | 사용자가 정상 저장한 이전 상태로 되돌아갈 추가 안전망을 제공한다. | atomic save(`existing scope`), 보존 개수·용량 정책, 개인정보·디스크 관리. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Scratch disk 위치·한도 설정과 부족 사전 경고 | 대형 문서에서 RAM 또는 디스크 부족으로 작업이 갑자기 실패하는 위험을 낮춘다. | 메모리 예산/임시 파일 아키텍처(`existing scope`), 여유 공간 감시, 안전 정리. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 읽기 전용·권한·외부 변경 충돌 처리 | 다른 앱이 파일을 바꾸거나 저장 권한이 없을 때 데이터 유실을 막는다. | 파일 identity/mtime 감시, Save As fallback, 명확한 충돌 interaction port. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 진단 보고서 내보내기 | GPU·코덱·저장 오류를 개인정보를 통제하며 재현 가능하게 보고한다. | 안정된 diagnostic code(`existing scope`), 로그 redaction, 사용자 동의 UI. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 안전 모드 시작 | 반복 시작 실패 시 GPU 가속, 복구 세션, 사용자 설정을 선택적으로 우회한다. | 시작 실패 감지, CPU fallback(`existing scope`), 설정 격리. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Input / accessibility

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| Touch pan·pinch zoom·rotate gesture | 태블릿과 터치 화면에서 캔버스를 자연스럽게 탐색한다. | pointer arbitration, palm rejection, viewport transform. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Stylus barrel button와 eraser-end 매핑 | 펜을 놓지 않고 eyedropper, eraser, pan 등으로 전환한다. | platform-neutral 버튼 의미, 사용자 매핑 설정, 도구 상태 복원. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 키보드만으로 도구·레이어·캔버스 조작 | 마우스를 사용할 수 없는 사용자와 고속 워크플로를 지원한다. | 명령 focus model, shortcut 후보, canvas keyboard navigation. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| High Contrast·Reduce Motion 전체 적용 | OS 접근성 설정에서 툴 상태, 선택 오버레이, preview가 식별 가능하도록 한다. | 테마 token, 비색상 상태 표시, splash 외 모든 animation 감사. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |
| Screen reader용 문서·레이어·도구 상태 요약 | 시각 정보에만 의존하지 않고 현재 편집 맥락을 전달한다. | 접근 가능한 snapshot 설명, live region 남용 방지, UI Automation 테스트. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [ ] 제외 |
| 왼손잡이·소형 화면용 패널 재배치 | 입력 손이 캔버스를 가리는 문제를 줄이고 다양한 창 크기를 지원한다. | docking/layout persistence, 최소 크기·터치 target 규칙. 비용: 중간 | [ ] 채택 [ ] 보류 [ ] 제외 |

## Interchange

> 검토 확정: 이 절의 후보는 모두 제외한다. 기존 확정 범위인 `.ocp`, PNG, JPEG, PSD 읽기/쓰기는 이 결정의 대상이 아니며 기존 계획을 유지한다.

| 후보 기능 | 필요한 이유 | 의존성·비용 | 검토 결정 |
|---|---|---|---|
| BMP·TIFF·WebP 읽기/쓰기 | Windows 자료, 인쇄 스캔, 웹 자산에서 흔한 래스터 형식을 보완한다. | WIC/별도 codec capability, alpha·bit depth·metadata 정책. 비용: 중간 | [ ] 채택 [ ] 보류 [x] 제외 |
| GIF 및 APNG 읽기/쓰기 | 간단한 웹 애니메이션 자산과 호환한다. | frame/timeline 모델이 현재 없음, palette quantization, disposal/blend 규칙. 비용: 높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| OpenEXR 읽기/쓰기 | HDR·VFX 파이프라인과 float channel 데이터를 교환한다. | float pixel format 후보, 다중 channel 매핑, 외부 codec. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| SVG 가져오기/내보내기 | 로고·아이콘·도형을 벡터 품질로 교환한다. | Vector Shape Layer 후보 또는 명시적 래스터화, 폰트/필터 호환성 보고. 비용: 높음~매우 높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| PDF 가져오기/내보내기 | 문서·인쇄 워크플로와 페이지 기반 자산을 교환한다. | PDF renderer/writer, 다중 페이지 정책, color profile·font 라이선스. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| Camera RAW 가져오기 | 사진 편집 시작점에서 센서 데이터를 높은 품질로 현상한다. | RAW decoder, demosaic·화이트밸런스·렌즈 보정, 16-bit/float 후보. 비용: 매우 높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| EXIF·XMP·orientation metadata 보존·편집 | 촬영 정보와 저작권 정보가 형식 변환 중 유실되는 것을 막는다. | codec별 metadata round-trip, 개인정보 삭제 옵션, `.ocp` mapping. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| Export preset과 batch export | 여러 크기·형식의 결과물을 반복해서 만드는 시간을 줄인다. | codec/export(`existing scope`), background job queue, naming collision 정책. 비용: 중간~높음 | [ ] 채택 [ ] 보류 [x] 제외 |
| Windows 파일 연결·탐색기 thumbnail | `.ocp` 문서를 더블클릭하고 탐색기에서 미리 식별할 수 있게 한다. | MSI 등록, 안전한 thumbnail provider 프로세스, codec 안정성. 비용: 높음 | [ ] 채택 [ ] 보류 [x] 제외 |

## 검토 결과 반영란

- 검토자:
- 검토 날짜:
- 이번에 채택한 후보:
- 보류 사유와 재검토 조건:
- 제외 사유: Interchange 절 전체 제외 확정. 기존 확정 범위인 `.ocp`, PNG, JPEG, PSD는 유지.
- 요구사항·아키텍처·`PROGRESS.md` 승격 작업 요청 링크 또는 기록:
