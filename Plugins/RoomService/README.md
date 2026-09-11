# RoomService — Unreal Engine 5.7

프로젝트 게임 코드에 의존하지 않는 Runtime 플러그인이다. Master는 Unreal Commandlet이며
HTTPServer로 방 API를 제공하고 각 방에 전용 서버 프로세스 하나를 실행한다.
클라이언트와 게임 서버는 Unreal HTTP를 사용한다. 별도 웹 프레임워크나 런타임은 필요 없다.

## 구성

```text
Title WBP → RoomServiceClientSubsystem → RoomMaster Commandlet
                                           ├─ port pool / processes / reservations
                                           ├─ RoomServiceBackend
                                           │   ├─ Authenticate
                                           │   ├─ PublishRoom / RemoveRoom
                                           │   ├─ DiscoverRooms
                                           │   └─ IssueConnection / ValidateConnection
                                           ├─ GameServer :7100 → RoomServiceGameModeBase
                                           └─ GameServer :7101 → RoomServiceGameModeBase
```

`FRoomServiceInfo`는 RoomId, Title, MapId, State, CurrentPlayers, MaxPlayers, bPrivate,
Attributes를 담는다. Attributes는 아이콘 키/게임 모드 등 프로젝트별 문자열 데이터를 담는다.
게임 코드·맵 에셋·아이콘 에셋은 플러그인에 들어가지 않는다.
게임 서버도 첫 Ready 보고 전에 같은 구조체를 Master에서 받아 `RoomDefinition`에 보관하고
BP 이벤트 `OnRoomDefinitionReady`를 호출한다. 프로젝트별 Attributes는 이 이벤트에서 적용한다.

`CreateRoom(Definition)`은 구조체의 Title, MapId, MaxPlayers, bPrivate, Attributes를 사용한다.
RoomId/State/CurrentPlayers는 Master가 결정하며 입력값을 신뢰하지 않는다.
MaxPlayers가 0이면 설정 기본값, 양수이면 설정의 최대값까지 허용한다.

## 다른 프로젝트에 붙이기

1. 이 폴더를 `<Project>/Plugins/RoomService`에 복사한다.
2. 프로젝트 Build.cs의 PublicDependencyModuleNames에 `RoomService`를 추가한다.
3. 기존 C++ GameMode의 부모 `AGameModeBase`를 `ARoomServiceGameModeBase`로 바꾸고
   `RoomServiceGameModeBase.h`를 include한다. 기존 Super 호출은 유지한다.
4. 준비 중에는 `SetRoomServiceState(Waiting)`, 참가를 닫을 때는 `Playing`,
   방을 끝낼 때는 `Ending`을 호출한다. BP에서도 같은 함수를 사용할 수 있다.
5. 프로젝트 `Config/DefaultRoomService.ini`에 서버 경로와 허용 맵을 설정한다.
6. 프로젝트의 Editor/Server 타깃을 직접 빌드하고 게임 서버를 패키징한다.

방 구조체만으로 새 방을 요청할 수 있다. 다만 프로젝트의 경기 시작/종료를 플러그인이
추측할 수 없으므로 GameMode 상태 연결은 필요하다. 기존 부모가 AGameMode인 프로젝트는
상속을 바로 바꾸면 MatchState 동작을 잃으므로 해당 프로젝트에 맞는 통합이 추가로 필요하다.

`-RoomId` 없는 PIE, Listen Server, 기존 직접 접속은 관리 모드가 켜지지 않는다.
관리 방에서는 토큰 없는 직접 IP 접속을 거절한다. 관리 방은 생성 시 선택한 맵에서
한 판을 실행하는 구조이며, seamless travel/방 내 맵 변경은 구현 범위에 포함되지 않는다.

## 설정

프로젝트 `Config/DefaultRoomService.ini` 예시:

```ini
[/Script/RoomService.RoomServiceSettings]
MasterUrl=http://127.0.0.1:7000
MasterPort=7000
BindAddress=127.0.0.1
AdvertisedHost=127.0.0.1
ServerExecutable=C:/Packaged/MyGameServer/MyGame/Binaries/Win64/MyGameServer.exe
FirstGamePort=7100
LastGamePort=7199
MaxRooms=8
MaxPlayers=8
StartupTimeout=90.0
HeartbeatInterval=2.0
HeartbeatTimeout=15.0
ReservationTimeout=30.0
EmptyRoomTimeout=60.0
EndingTimeout=10.0
RequestTimeout=110.0
Maps=(("Arena","/Game/Maps/Arena"),("Snow","/Game/Maps/Snow"))
```

- ServerExecutable은 패키지 최상단 bootstrap exe가 아니라 실제 내부 서버 exe의 절대 경로다.
  셸을 거치지 않고 직접 실행한다. 맵은 패키징에 포함해야 한다.
- Maps는 MapId → long package path 허용 목록이다. 오브젝트 접미사 `.Arena`를 붙이지 않는다.
  TMap 설정이므로 위의 `Maps=((...),(...))` 형태로 쓴다.
- MasterUrl은 클라이언트가 접속하는 주소다. 같은 PC의 자식 서버는 MasterPort의 loopback에 보고한다.
- BindAddress는 `127.0.0.1` 또는 `0.0.0.0`이며, 외부 PC에 공개할 때 AdvertisedHost를
  도달 가능한 IPv4/DNS 주소로 바꾸고 Master TCP 포트와 게임 UDP 포트를 연결한다.
- HeartbeatTimeout은 HeartbeatInterval의 3배보다 커야 한다. StartupTimeout은
  HeartbeatTimeout보다, RequestTimeout은 StartupTimeout + 5초보다 커야 한다.
- ReservationTimeout은 HeartbeatTimeout보다, EmptyRoomTimeout은 ReservationTimeout보다 커야 한다.
- BackendClass를 생략하면 `ULocalRoomServiceBackend`를 사용한다.

## 실행

Editor 모듈과 Server 타깃을 **사용자가 빌드한 뒤** 다음을 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  'C:/Projects/MyGame/MyGame.uproject' -run=RoomMaster -unattended -nullrhi
```

개발용으로 프로젝트 Config/DefaultRoomService.ini에 `bUseEditorServer=True`를 설정하면
Master 자신과 동일한 실행 파일에 프로젝트 경로, 맵, `-server`를 전달해 방을 실행한다.
이 모드에서는 ServerExecutable 설정과 서버 패키징이 필요 없다. 클라이언트 에디터도
Master와 동일한 엔진을 사용해야 한다. 에디터의 Launch Separate Server는 꺼둔다.
`bUseEditorServer=False`로 바꾸면 기존 패키징 서버 실행 모드를 사용한다.
코드 빌드 전에는 Master와 자식 서버, 에디터를 종료해 DLL 잠금을 해제한다.

현재 Master 실행 형태는 UnrealEditor-Cmd에 로드되는 Commandlet이다. 별도 독립 Master.exe
타깃은 만들지 않았다. Master PC에도 해당 프로젝트의 빌드된 Editor 모듈과 엔진이 필요하다.
자식 방 프로세스는 설정의 패키징된 GameServer.exe다. ServerExecutable이 비어 있거나
존재하지 않으면 Master는 설정 오류를 반환하며 실행을 멈춘다.

간단한 방 목록 확인:

```powershell
Invoke-RestMethod -Uri 'http://127.0.0.1:7000/rooms' -Method Post `
  -ContentType 'application/json' -Body '{"op":"list"}'
```

방 만들기(Ready까지 응답 대기):

```powershell
$body = @{
    op = 'create'
    mapId = 'Arena'
    title = 'Test room'
    private = $false
    maxPlayers = 4
    attributes = @{ icon = 'ArenaPreview' }
} | ConvertTo-Json
$connection = Invoke-RestMethod -Uri 'http://127.0.0.1:7000/rooms' -Method Post `
    -ContentType 'application/json' -Body $body -TimeoutSec 120
```

`$connection`의 endpoint와 token은 클라이언트 `ConnectToRoom`에 전달한다.
BP에서는 `OnConnectionReceived → ConnectToRoom`을 연결한다. 자동 이동과 연결 응답 이벤트를
분리했으므로 WBP에서 로딩/실패 표시를 처리할 수 있다.

## BP 연결

GameInstance에서 `GetSubsystem(RoomServiceClientSubsystem)`을 가져온다.

| 버튼/이벤트 | 호출/데이터 |
| --- | --- |
| 방 리스트 열기/새로고침 | RequestRoomList → OnRoomListReceived |
| 목록의 입장 | JoinRoom(RoomId) |
| 맵 선택 후 퀵매치 | QuickMatch(MapId) |
| 방 만들기 | Make FRoomServiceInfo → CreateRoom |
| Private 토글 | Definition.bPrivate |
| 연결 응답 | OnConnectionReceived → ConnectToRoom |
| 요청 실패 | OnRequestFailed(Error) |
| 닫기 | CancelRequest + WBP 닫기 |

한 subsystem에서 한 번에 하나의 요청을 진행한다. 중복 클릭은 `request_in_progress`를 반환한다.
클라이언트 취소는 응답을 무시한다. 이미 실행된 서버/예약은 예약 TTL과 빈 방 timeout으로 정리한다.
강제 즉시 취소/재시도 idempotency 키는 구현하지 않았다.

아이콘은 MapId로 기존 프로젝트의 DataTable에서 찾거나 Attributes의 키로 해석한다.
기존 WBP와 에셋 레이아웃은 이 플러그인이 변경하지 않는다.

## 생성·입장·정리 계약

1. Master가 설정 범위의 미사용 UDP 포트를 확인하고 자기 풀에서 확보한다.
2. 프로세스를 실행한다. 이 시점의 Starting 방은 목록에 나타나지 않는다.
3. GameMode BeginPlay 이후 NetDriver와 실제 바인딩 포트를 확인하고 방 구조체를 받아
   OnRoomDefinitionReady를 실행한 뒤 Waiting을 보고한다.
   이 첫 보고가 Ready다.
4. Master가 Backend.PublishRoom 성공을 확인하고 방을 등록한다.
5. 정원 예약 → Backend.IssueConnection → endpoint와 일회용 토큰을 반환한다.
6. 서버 PreLoginAsync에서 Master에 토큰 소비를 요청한다. InitNewPlayer가 로컬 예약/상태/정원을
   다시 확인한다. 토큰 검증 중에도 좌석을 점유한다.
7. 서버의 접속 토큰 목록과 실제 관리 플레이어 수 보고로 예약 좌석을 실제 좌석으로 전환한다.
   예약+실제 인원을 이중 집계하지 않는다.

퀵매치는 같은 MapId의 Public + Waiting + 빈자리 방을 찾는다. 준비 중인 Public 방에
빈 예약 자리가 있으면 Ready까지 함께 대기하며, 없으면 새 방을 만든다.
Private 방은 목록/퀵매치에서 제외한다. 비밀번호 방식이 아니며 방 ID를 아는 사용자만
JoinRoom을 요청할 수 있다. 입장 시 정원/상태/토큰 검증은 동일하다.

Ending은 종단 상태이며 EndingTimeout 이후 프로세스를 종료한다. 시작 실패, heartbeat 만료,
빈 방 만료, 실제 프로세스 종료도 목록에서 제거한다. **프로세스 종료 확인 후** 포트를 반환한다.
Master가 비정상 종료되면 자식은 LeaseTimeout 내의 응답 단절을 감지하고 종료를 요청한다.
Master 재시작은 기존 방을 복구하지 않는다. 새 Master는 바인딩된 게임 포트를 건너뛴다.

## 외부 백엔드 연결

`URoomServiceBackend`를 상속하고 설정의 BackendClass에 C++ 클래스 경로를 지정한다.

| 경계 | 책임 |
| --- | --- |
| Authenticate | 로그인 credential 검증과 identity 반환 |
| PublishRoom / RemoveRoom | 세션/로비 등록·업데이트·해제 |
| DiscoverRooms | 외부 디렉터리에서 후보 조회 |
| IssueConnection | endpoint와 연결 티켓 발급 |
| ValidateConnection | 티켓 검증 |

비동기 콜백은 Game Thread에서 정확히 한 번 호출해야 한다. Master는 늦은 콜백에 대해
방 존재/상태/예약 유효성을 재검사한다. 외부 디렉터리의 오래된 인원수를 입장 허가로 쓰지 않는다.
인증에서 얻은 Identity는 디스커버리와 접속 티켓 발급에 전달하므로 계정별 정책을 적용할 수 있다.
EOS 모듈은 기본 플러그인에 의존성으로 넣지 않았다. EOS 어댑터 플러그인이 RoomService에
의존하도록 구성하면 게임 코드와 자체 Master 구현을 그대로 사용할 수 있다.

현재 endpoint는 IPv4/DNS:port, 티켓은 128자 이하 영숫자/underscore/hyphen이다.
EOS 로그인 토큰을 그대로 URL에 넣기보다 짧은 불투명 접속 티켓으로 교환한다.
기본 구현은 익명 로컬 인증과 메모리 디렉터리이며 실제 EOS 로그인/계정 소유권 검증은 없다.
공개 서비스 운영에 필요한 TLS/요청자별 rate limit/계정 인증은 별도 운영·백엔드 통합 과제다.
플러그인 자체 로그는 티켓을 출력하지 않지만 엔진 travel/command-line 로그와 OS 프로세스
정보에는 일시적 접속 자격이 남을 수 있으므로 해당 로그를 공개하지 않는다.

## 검증

자동화 테스트 `RoomService.Master.ReservationAndReplay`를 추가했다.
실제 Master 예약 로직으로 정원 초과, 일회용 토큰 재사용, 실제 인원+예약 집계,
Playing/오래된 heartbeat 입장 차단을 확인한다. 이 작업에서는 빌드/테스트 실행을 하지 않았다.

사용자 빌드 후 Unreal Automation 창에서 RoomService를 검색해 실행한다.
실제 서버 점검은 다음 순서로 한다.

1. Master 시작 → 잘못된 MapId 거부 → Public 생성 → Ready 전에는 목록에 없음 확인.
2. 클라이언트 2개를 같은 방에 접속. 실제/예약 인원과 정원 초과 거부 확인.
3. Private 생성 → 목록/퀵매치 제외 → RoomId를 이용한 입장은 성공 확인.
4. 동일 맵 동시 퀵매치 → 정원 안에서는 같은 방, 가득 차면 새 프로세스 확인.
5. 토큰 없는 접속, 재사용 토큰, 만료 토큰이 거부되는지 확인.
6. 경기 시작 → 새 입장 거부. 결과 화면 → EndingTimeout 뒤 방/프로세스 정리 확인.
7. 방 프로세스 강제 종료 → 목록 제거 → 해당 UDP 포트 재사용 확인.
8. 시작 실패, 포트 선점, 최대 방 수, 빈 방 timeout, Master 종료/재시작 확인.

테스트 결과는 실행 전까지 미검증이다. 패키징/실접속/장애 복구의 성공을 코드 작성만으로
보장하지 않는다.
