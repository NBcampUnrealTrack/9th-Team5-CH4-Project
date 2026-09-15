# Title hierarchy 기반 RoomService 연결

Public/Private 모두 Dedicated 방식이다. C++만 수정했으며 WBP 에셋은 편집하지 않았다.
빌드/UHT/BP 실행은 사용자가 확인한다.

## Title hierarchy

```text
WBP_Title (HostOrJoinWidget)
└─ Root Overlay
   ├─ Overlay_Title        ← 기존 로고/타이틀 버튼 영역
   ├─ WBP_Join             ← 기존 위젯 유지
   ├─ WBP_Settings         ← 기존 위젯 유지
   ├─ WBP_ChoiceMap        ← 기존 위젯 (선택; 제거 가능)
   ├─ WBP_RoomService      ← DRRoomServiceWidget
   └─ WBP_CreateRoom       ← DRCreateRoomWidget; 마지막에 배치해 앞에 표시
```

RoomService와 CreateRoom을 **Overlay_Title 안에 넣지 않는다**. 루트 Overlay 아래 형제로 둔다.
이름은 WBP_RoomService, WBP_CreateRoom으로 정확히 지정한다.
RoomServiceWidgetClass/CreateRoomWidgetClass는 제거했다. CreateWidget/AddToViewport가 필요 없다.
시작할 때 C++가 두 화면을 Collapsed로 설정한다. 열기/닫기는 Visibility만 바꾼다.
Title 방 접속 버튼 → HandlePublicMatchClicked(Self). 돌아가기는 Title 메뉴와 포커스를 복구한다.

## WBP_RoomService

Parent Class = DRRoomServiceWidget.
현재 스크린샷의 RoomListView, QuickMatch, CreateRoom, PrivateJoin, Exit 이름을 그대로 쓴다.
RoomListView는 BlueprintReadOnly로 노출했으므로 기존 Get RoomListView 경고가 해소된다.
단, 기존 BP가 목록 응답/아이템 생성을 수행한다면 C++ 처리와 중복되므로 해당 로직은 제거한다.

ListView의 Entry Widget Class = WBP_RoomEntry.
Class Defaults의 MapDefinitionTable = 기존 Title 맵 DataTable.

### 퀵매치 enum

Class Defaults → Rooms → QuickMatchMode:

| 값 | 동작 |
| --- | --- |
| AnyMap (기본값) | 맵 구분 없이 Public + Waiting + 빈자리 방 참가 |
| SelectedMap | QuickMatchMapId와 같은 맵의 Public + Waiting + 빈자리 방 참가 |

QuickMatchMapId 기본값은 SamplePlayMap이다. AnyMap에서는 **방이 없을 때 새로 만들 맵**으로만
사용한다. 기존 자동 생성 정책을 유지한다. 두 모드 모두 Private/Playing/만석 방을 제외한다.
클라이언트에서 오래된 목록을 고르는 방식이 아니라 Master에서 예약과 함께 처리한다.
기존 QuickMatch(MapId) API는 지정 맵 동작을 유지한다. 새 API는 QuickMatchWithMode(Mode, MapId).

### 버튼 Dispatcher 연결 (Target = Self)

| 버튼 | 함수 |
| --- | --- |
| QuickMatch | HandleQuickMatchClicked |
| CreateRoom | HandleCreateRoomClicked |
| PrivateJoin | HandlePrivateJoinClicked |
| Exit | HandleExitClicked |
| 새로고침을 추가할 경우 | RefreshRooms |

커스텀 WBP_TitleButton의 실제 Button.OnClicked → 기존 Dispatcher → 위 함수로 연결한다.
OnRoomListReceived/OnConnectionReceived는 C++에서 이미 처리하므로 BP에서 중복 연결하지 않는다.

### 현재 스크린샷에 없는 위젯

StatusText, PrivateJoinPanel, RoomCodeInput은 **선택 바인딩**으로 바꿨다. 없어도 WBP를 컴파일한다.

- StatusText(TextBlock)를 추가하면 요청/오류가 자동 표시된다.
- 다른 상태 UI를 쓰려면 OnStatusChanged 이벤트의 Message 또는 StatusMessage를 사용한다.
- PrivateJoinPanel과 그 안의 RoomCodeInput(EditableTextBox)이 없으면 Private 접속 버튼은
  비활성화한다. 컴파일 성공을 위해 숨겨진 가짜 입력창을 생성하지 않는다.
- Private 입력창을 추가하면 확인 → HandlePrivateJoinConfirmClicked,
  닫기 → HandlePrivateJoinCloseClicked로 연결한다. 입력은 전체 RoomId이며 IP가 아니다.

## WBP_CreateRoom (스크린샷 레이아웃 유지)

**스크린샷의 부모는 아직 DRTitleMapChoiceWidget이다. DRCreateRoomWidget으로 변경해야 한다.**
Title hierarchy에서 이름을 WBP_CreateRoom으로 지정한다.

| 위치/기능 | 위젯 이름 | 종류 |
| --- | --- | --- |
| 최상단 기존 Overlay | Overlay_ChoiceMap | Overlay |
| 왼쪽 맵 미리보기 | ChoosedImageMap | Image |
| 오른쪽 맵 선택 | ComboBoxString_ChoiceMap | ComboBoxString |
| 오른쪽 방 이름 | RoomTitleInput | EditableTextBox |
| 기존 텍스트 설정 행을 쓰는 경우 | RoomTitleRow | DRTitleTextSettingRowWidget 자식 WBP |
| 오른쪽 Private 체크 | PrivateCheckBox | CheckBox |
| 생성 버튼 | CreateMap | 기존 버튼 위젯 |
| 상태/오류 표시 (선택) | StatusText | TextBlock |

방 이름은 RoomTitleInput / RoomTitleRow **둘 중 하나**만 배치한다. 기존 커스텀 텍스트 입력 행을
사용한다면 RoomTitleRow로 이름을 정하면 내부 GetSettingText()로 읽는다. 둘 다 없으면 생성은
비활성화한다. 이미지 프레임만으로는 텍스트를 입력할 수 없으므로 실제 입력 위젯이 있어야 한다.
Private 체크박스는 스크린샷의 기본 Check Box 이름을 PrivateCheckBox로 바꾼다.

Class Defaults의 MapDefinitionTable = 기존 Title DataTable.
MaxRoomPlayers = 0이면 서버 설정 기본값, 양수이면 해당 인원 제한 요청.

- 생성 버튼 → HandleCreateMapClicked(Self)
- 닫기 버튼 → HandleCloseChoiceMapClicked(Self)

맵/미리보기는 기존 C++가 처리한다. 방 제목은 공백 제거 후 1~80자이며 Private도 Dedicated로
생성한다. MapId는 DataTable의 맵 패키지를 서버 Maps 설정과 대조해 결정한다.
StatusText를 추가하지 않는 경우 OnFeedbackChanged(Message)로 커스텀 상태 UI를 연결한다.
요청 중에는 입력/생성을 막고 닫기는 취소로 동작한다. ClientTravel 이후에는 닫기를 차단한다.

## WBP_RoomEntry

**스크린샷의 부모는 아직 UserWidget이다. DRRoomEntryWidget으로 변경해야 한다.**
RoomId(TextBlock), RoomImage(Image), RoomName(TextBlock), RoomJoinCount(TextBlock),
RoomState(TextBlock)는 현재 이름 유지. 오른쪽 WBP_TitleButton 이름을 Join으로 지정한다.
Join 클릭 Dispatcher → HandleJoinClicked(Self).

행 배경 Button/목록 선택/더블클릭에 접속을 연결하지 않는다. C++가 UDRRoomListItem 데이터와
이미지/상태/인원을 갱신하므로 기존 BP_RoomListItem 캐스트/목록 채우기 그래프는 사용하지 않는다.

## 확인 범위

코드 검색/공백 검사만 수행했다. 빌드는 실행하지 않았다.
기존 RoomService.Master.ReservationAndReplay 테스트에 AnyMap의 다른 맵 참가,
SelectedMap의 다른 맵 제외, Private/Playing 제외 검증을 추가했다. 테스트 실행은 미검증이다.

빌드 후 WBP 부모/이름을 지정하고 컴파일한 다음, 열기→닫기→재열기, 생성창 입력/취소,
행 클릭 무접속/입장 버튼 접속, 서로 다른 맵의 방을 이용한 퀵매치를 확인한다.