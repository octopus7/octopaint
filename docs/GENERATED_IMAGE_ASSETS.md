# 생성 이미지 에셋 카탈로그

이 문서는 ImageGen으로 만든 OctoPaint 이미지 에셋의 목적과 최종 사용 경로를 기록하는 단일 카탈로그다. 작업 중인 에이전트와 구현자는 이미지를 임의로 다시 생성하거나 다른 파일로 대체하지 말고, 아래 목적과 경로를 확인해 해당 파일을 꺼내 사용한다.

아래 에셋은 ImageGen 생성과 후처리를 마쳤으며 표에 기록된 최종 경로에 배치되어 있다.

## 에셋 목록

| 구분 | 에셋 | 만든 목적 | 최종 크기 | 최종 경로 |
| --- | --- | --- | --- | --- |
| 앱 아이콘 | 프랑스 모자를 쓰고 붓을 든 은은한 민트색 문어 | OctoPaint의 앱, 실행 파일, 창 및 패키지에서 제품 정체성을 나타내는 대표 아이콘 마스터 | 1024×1024 PNG | `src/OctoPaint.WinUI/Assets/Generated/App/octopaint-app-icon.png` |
| 스플래시 배경 | 은은한 무지개 솜사탕 구름 | OctoPaint 앱 시작 화면에서 캐릭터 뒤에 별도 레이어로 표시하는 순수 배경 이미지 | 1600×900 PNG | `src/OctoPaint.WinUI/Assets/Generated/Splash/octopaint-splash-background.png` |
| 스플래시 전경 캐릭터 | 어린 소녀 메이드 캐릭터. 상반신 중심 구도이며, 양손이 프레임 하단에서 치마/앞치마 양옆을 잡고 정중히 인사한다. 비성적이고 단정한 긴소매·높은 칼라 의상을 입은 정교한 고품질 애니메이션 일러스트 스타일이며, 은발 양갈래의 결·광택·그라데이션을 고품질로 채색한다. 하의 치마는 상의의 검은 부분과 같은 검은 천 재질이고 그 위에 흰 앞치마를 둔다. 앞치마 허리나 중앙에는 검은 리본을 달지 않으며, 흰 앞치마의 왼쪽 아래 끝과 오른쪽 아래 끝에 작은 검은 리본을 하나씩 총 2개 단다. | OctoPaint 앱 시작 화면에서 배경 앞에 별도 레이어로 표시하는 투명 전경 캐릭터 이미지 | 1024×1024 투명 PNG | `src/OctoPaint.WinUI/Assets/Generated/Splash/octopaint-splash-character.png` |
| 스플래시 정적 폴백 합성 | 스플래시 배경과 전경 캐릭터를 정적인 한 장면으로 합성 | 애니메이션 비활성 또는 Reduce Motion 환경에서 OctoPaint 앱 시작 화면에 표시 | 1600×900 PNG | `src/OctoPaint.WinUI/Assets/Generated/Splash/octopaint-splash.png` |
| 툴 아이콘 | 연필 | 픽셀 단위의 선을 그리는 `Pencil` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/pencil.png` |
| 툴 아이콘 | 에어브러시 | 분사형으로 색을 누적하는 `Airbrush` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/airbrush.png` |
| 툴 아이콘 | 사각 선택 | 직사각형 영역을 선택하는 `RectangularMarquee` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/rectangular-marquee.png` |
| 툴 아이콘 | 타원 선택 | 타원형 영역을 선택하는 `EllipticalMarquee` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/elliptical-marquee.png` |
| 툴 아이콘 | 자유형 올가미 | 자유롭게 그린 경계로 영역을 선택하는 `FreehandLasso` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/freehand-lasso.png` |
| 툴 아이콘 | 다각형 올가미 | 꼭짓점을 연결한 다각형 경계로 영역을 선택하는 `PolygonalLasso` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/polygonal-lasso.png` |
| 툴 아이콘 | 레이어 이동 | 현재 레이어의 문서 좌표 오프셋을 이동하는 `MoveLayer` 도구를 세로 툴바에서 식별 | 64×64 PNG | `src/OctoPaint.WinUI/Assets/Generated/Tools/move-layer.png` |

## 파생 산출물

| 산출물 | 원본 | 만든 목적 | 최종 경로 |
| --- | --- | --- | --- |
| Windows 앱 아이콘 | `octopaint-app-icon.png` 1024×1024 마스터 | 실행 파일과 Windows 앱 리소스에서 요구하는 다중 크기 아이콘 제공 | `src/OctoPaint.WinUI/Assets/OctoPaint.ico` |

스플래시 PNG 3종에는 제작자 표기 등의 텍스트나 저장소 링크 버튼을 굽지 않는다. 이 파일들은 순수한 배경·캐릭터 및 그 정적 합성 이미지이며, 텍스트와 상호작용 요소는 프런트엔드 UI 레이어에서 별도로 렌더링한다.

## ImageGen 원본 아카이브

모든 ImageGen 생성 원본은 최종 앱 에셋과 분리하여 `artwork/imagegen-sources/2026-08-06/`에 보관한다. `초안`은 검토 과정에서 제외된 시안이고, `최종 선택 원본`은 후처리하여 위 에셋 목록의 최종 파일을 만드는 입력이다.

| 파일 | 목적 | 상태 |
| --- | --- | --- |
| `00-app-icon-purple-draft.png` | 퍼플 색상의 초기 문어 앱 아이콘 시안 | 초안 |
| `01-tool-pencil-purple-draft.png` | 퍼플 색상의 초기 연필 툴 아이콘 시안 | 초안 |
| `02-tool-airbrush-purple-draft.png` | 퍼플 색상의 초기 에어브러시 툴 아이콘 시안 | 초안 |
| `03-tool-rectangular-marquee-purple-draft.png` | 퍼플 색상의 초기 사각 선택 툴 아이콘 시안 | 초안 |
| `04-tool-elliptical-marquee-purple-draft.png` | 퍼플 색상의 초기 타원 선택 툴 아이콘 시안 | 초안 |
| `05-app-icon-mint-selected-source.png` | 은은한 민트색 문어 앱 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `06-tool-pencil-grayscale-selected-source.png` | 무채색 연필 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `07-tool-airbrush-grayscale-selected-source.png` | 무채색 에어브러시 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `08-splash-combined-adult-draft.png` | 성인 캐릭터와 배경을 한 장으로 생성한 초기 스플래시 합성 시안 | 초안 |
| `09-tool-rectangular-marquee-grayscale-selected-source.png` | 무채색 사각 선택 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `10-tool-elliptical-marquee-grayscale-selected-source.png` | 무채색 타원 선택 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `11-tool-freehand-lasso-grayscale-selected-source.png` | 무채색 자유형 올가미 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `12-tool-polygonal-lasso-grayscale-selected-source.png` | 무채색 다각형 올가미 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `13-tool-move-layer-grayscale-selected-source.png` | 무채색 레이어 이동 툴 아이콘 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `14-splash-background-selected-source.png` | 은은한 무지개 솜사탕 구름 스플래시 배경 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `15-splash-character-adult-draft.png` | 성인 은발 양갈래 메이드의 분리형 스플래시 전경 시안 | 초안 |
| `16-splash-character-flat-draft.png` | 플랫하고 단순한 스타일의 분리형 스플래시 전경 시안 | 초안 |
| `17-splash-character-girl-white-skirt-draft.png` | 흰 치마를 입은 소녀 메이드의 분리형 스플래시 전경 시안 | 초안 |
| `18-splash-character-girl-black-skirt-selected-source.png` | 검은 치마와 중앙 리본이 달린 흰 앞치마를 입은 소녀 메이드 스플래시 전경 시안 | 중앙 리본 초안 |
| `19-splash-character-girl-apron-corner-bows-selected-source.png` | 검은 치마와 양쪽 아래 끝에 작은 검은 리본이 하나씩 달린 흰 앞치마를 입은 소녀 메이드 스플래시 전경 최종 에셋의 생성 원본 | 최종 선택 원본 |
| `20-splash-character-girl-muted-solid-background-new-source.png` | 최종 캐릭터 요구를 참조 이미지 없이 새로 생성한 사용자 후가공용 원본 | 새 생성 원본 |

앱 빌드와 패키징에는 `artwork/imagegen-sources/2026-08-06/` 아카이브를 포함하지 않는다. 빌드에는 `src/OctoPaint.WinUI/Assets/` 아래에 배치된 후처리 완료 최종 에셋과 필요한 파생 산출물만 포함한다.

### 원본 보존 원칙

- 투명 추출 과정에서 품질 문제가 발생할 수 있으므로 `artwork/imagegen-sources/2026-08-06/`의 ImageGen 원본은 절대 덮어쓰거나 후처리하지 않는다.
- `20-splash-character-girl-muted-solid-background-new-source.png`는 기존 캐릭터 수정이나 참조 없이 완전히 새로 생성했으며, 저채도 더스티 청록 단색 배경을 포함한 원본 상태 그대로 보존한다. 이 원본에는 알파 추출, 리사이즈 또는 합성을 적용하지 않는다.
- 알파 추출, 리사이즈, 합성 및 ICO 변환은 모두 `src/OctoPaint.WinUI/Assets/Generated/` 또는 `src/OctoPaint.WinUI/Assets/` 아래의 파생본에만 적용한다.
- 최종 에셋을 다시 후처리해야 할 때는 기존 파생본을 원본처럼 사용하지 않고, 반드시 아카이브의 보존 원본에서 새 파생 작업을 시작한다.

## 후처리 및 검증 원칙

- ImageGen 원본이 실제 사용 크기보다 크게 생성되면 대상 UI와 패키징 요구에 맞게 고품질 리샘플링한다. 원본 비율을 유지하고 아이콘의 핵심 실루엣이 잘리지 않게 한다.
- 툴 아이콘은 무채색 RGBA로 후처리했으며 동일한 캔버스, 시각적 무게, 여백과 스타일을 유지하고 작은 표시 크기에서도 서로 구분되어야 한다.
- 투명 배경이 필요한 에셋은 알파 채널과 가장자리 번짐을 확인하고, 밝은 테마와 어두운 테마 모두에서 식별 가능해야 한다.
- 최종 파일을 배치한 뒤 실제 대상 크기에서 육안 검토하고, 파일 형식, 픽셀 치수, 알파 채널, 경로 및 빌드 포함 여부를 검증한다.
- 후처리로 파생 크기를 추가하거나 경로를 바꾸면 먼저 이 표를 갱신하여 구현자가 참조하는 카탈로그와 실제 파일을 일치시킨다.
