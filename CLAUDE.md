# AIGame — UE 5.5 Android 모바일 (5일 프로젝트)

## 문서 위치
- 기획: docs/01_planning/GDD.md
- 설계: docs/02_design/architecture.md, 결정 기록: docs/02_design/adr/ (파일명 NNNN-제목.md)
- 데이터: Data/*.csv → DataTable (스키마 · 수치 근거: docs/02_design/data-schema.md)
- 밸런스 시뮬레이션: Scripts/balance_sim.py (Data/*.csv를 읽는다)
- 태스크: docs/03_impl/tasks.md (태스크 ID는 T01 형식, 이 파일은 읽기만 한다)
- AI 작업 기록: docs/ai-log/log.md

## 기술 환경
- Unreal Engine 5.5. 다른 버전 API를 쓰지 않는다
- 엔진 소스(선언 확인용): D:/UE_5.5/Engine/Source, D:/UE_5.5/Engine/Plugins
- 타깃: Android arm64 (Development / Shipping). 개발은 Windows 에디터에서 한다
- 컴파일: Scripts/Build.bat  /  패키징: Scripts/PackageAndroid.bat [Development|Shipping] (기본 Development)
- 빌드·패키징은 요청받았을 때만 실행하고, 위 배치 파일만 쓴다 (엔진의 Build.bat·RunUAT를 직접 호출하지 않는다)
- Build.bat 실행 전에 에디터가 켜져 있는지 묻는다 (Live Coding이 켜져 있으면 외부 빌드가 거부됨)
- 패키징을 요청받으면 Source/ 아래 파일이 Binaries/Win64/UnrealEditor-AIGame.dll보다 최근에 수정됐는지 확인하고, 그렇다면 Build.bat을 먼저 실행할지 묻는다 

## 작업 방식
- 구현 전 계획을 제시하고 승인을 기다린다. 계획에는 다음을 넣는다
  - 수정·생성할 파일
  - 새 클래스·함수·변수와 각각의 추가 이유 (단순성 원칙의 성능/정확성 중 어느 경우인지)
  - 이 프로젝트에서 처음 쓰는 엔진 API와 그 선언이 있는 엔진 헤더 경로 (엔진 기능 환각 대비) 
  - 추가할 Build.cs 의존 모듈
  - 검증 방법 (컴파일, PIE 시나리오, 실기기 확인 항목)
- 한 번에 태스크 하나만 다룬다. 범위 밖 파일은 수정하지 않는다
- git commit / push는 하지 않는다. 사람이 diff를 리뷰한 뒤 커밋한다
- 태스크를 마치면 다음 순서로 보고한다
  - 변경한 파일
  - 빌드: 요청받아 실행했으면 결과 요약 줄(성공/실패)과 경고 원문, 실행하지 않았으면 '미빌드'
  - '확인 필요'로 남긴 것
  - 사람이 확인·작업할 항목 (PIE·실기기 확인, BP·UMG·에셋 작업, DataTable 재임포트)
- 태스크를 마치면 docs/ai-log/log.md 표에 한 줄 초안을 추가한다. 판정·근거 칸은 비워 두고 사람이 채운다. 커밋 칸은 세션 정보만 적는다

## 근거와 보고
- 처음 쓰는 엔진 API의 선언을 엔진 소스에서 찾지 못하면 쓰지 않고 '확인 필요'로 보고한다
- 다른 모듈의 헤더를 include하면 Source/AIGame/AIGame.Build.cs의 의존 모듈을 같은 변경에서 추가한다
- GDD·CSV에 없는 게임 규칙·수치·문구가 필요하면 지어내지 말고 질문한다
- 요청이 GDD·architecture.md와 충돌하면 구현하지 말고 충돌 지점을 보고한다
- 실행하지 않은 것을 실행했다고 쓰지 않는다. PIE·실기기 확인은 사람이 한다
- 같은 빌드 오류를 2번 고쳐도 해결되지 않으면 멈추고, 오류 원문과 원인 가설을 보고한다
- 테스트를 통과시키려고 테스트나 기대값을 고치지 않는다. 경고를 #pragma나 캐스팅으로 숨기지 않는다

## 아키텍처 규칙 (설계: docs/02_design/architecture.md)
- architecture.md의 클래스 목록에 없는 C++ 클래스는 만들지 않는다. 필요하면 계획에 이유를 적고 승인받는다 (자동화 테스트 매크로가 만드는 테스트 클래스는 예외)
- C++ 클래스는 UShopGameInstance · UShopSaveGame · ABusinessGameMode · FBusinessSim, 타입은 FShopProgress와 ShopTypes.h의 구조체 · enum이다. 헤더 선언의 기준은 architecture.md 3절이다
- 영업 규칙은 FBusinessSim에만 둔다. FBusinessSim은 UObject · World · 시계를 쓰지 않고, 모든 함수가 현재 시각(Now)을 인자로 받는다(ADR-0004)
- 로직·데이터·저장은 C++, 위젯 배치·연출은 BP. BP는 C++이 노출한 함수와 델리게이트만 쓴다
- 게임 진행 상태는 소유 클래스 한 곳에서 관리하고, UI는 델리게이트를 구독해 갱신한다. 진행 값(Day · Gold · UnlockStep · SteamerLevel)은 FShopProgress가 소유하며 결산(ApplyDayResult)과 로비 구매에서만 바뀐다
- 게임 로직과 상태 변경에는 Tick을 쓰지 않는다. 시간 경과는 만료 시각 + Timer·이벤트로 처리한다
- 시간 기준은 월드 시간(GetTimeSeconds, 일시정지 시 함께 멈춤)으로 통일한다. GetTimeSeconds는 ABusinessGameMode만 읽어 FBusinessSim에 넘긴다
- 영업 중 시간 이벤트는 FBusinessSim의 이벤트 힙(TArray 힙 함수) 하나로만 예약한다. 타이머는 ABusinessGameMode의 핸들 1개뿐이고 다른 SetTimer를 추가하지 않는다(ADR-0004)
- 힙 이벤트는 손님을 ID로 가리킨다. 포인터 · 오브젝트 참조를 넣지 않는다(ADR-0009)
- FBusinessSim을 호출한 뒤에는 반드시 ABusinessGameMode::HandleChanges를 부른다(Now 이하 이벤트 처리 → 델리게이트 → 타이머 재설정)
- 위젯의 매 프레임 갱신은 화면 전용 '연출 값'만 바꾼다. 게임 상태(만료 시각·골드·게이지 등)는 바꾸지 않고, 로직은 연출 값을 읽지 않는다
- 시간이 흐르는 표시(인내심·남은 시간)는 DeltaTime을 누적하지 말고 '원본 시각 − 현재 월드 시각'으로 계산한다
- 기획 수치는 Data/*.csv → DataTable. 코드에 하드코딩하지 않는다. CSV에는 수치와 화면 이름(FText)만 두고, 이미지 에셋 참조는 BP 기본값에 둔다(ADR-0014)
- CSV 컬럼을 바꾸면 Row 구조체 · data-schema.md · Scripts/balance_sim.py의 읽기 코드를 같은 변경에서 고친다. 수치를 바꾸면 balance_sim.py를 다시 돌려 data-schema.md '수치 초안'을 갱신한다. balance_sim.py의 BASES는 IsBaseIngredient와 같게 둔다
- FBusinessSim · FShopProgress에 규칙을 추가하거나 바꾸면 Private/Tests의 테스트(AIGame.<대상>.<규칙>, IMPLEMENT_SIMPLE_AUTOMATION_TEST)를 같은 변경에서 추가한다. 기대값 변경은 GDD · ADR 근거가 있을 때만 한다(ADR-0007)
- 저장 필드를 추가·변경하면 SaveVersion을 올린다
- Windows 에디터에서만 동작하는 코드를 쓰지 않는다. 저장은 SaveGame 슬롯 API로 하고, 플랫폼 분기(#if PLATFORM_*)·JNI·절대 경로 파일 입출력은 계획에서 승인받는다

## 단순성 원칙 (설계·구현 공통, SOLID와 충돌하면 이 원칙이 우선)
- 같은 시간복잡도로 동작하는 방법이 여럿이면, 새 클래스·함수·변수·분기가 가장 적고 호출 단계가 가장 얕은 방법을 고른다
- 코드나 설계를 늘리는 것(새 클래스·함수·간접 계층·분기 추가)은 아래 두 경우에만 한다
  - 성능: 시간복잡도가 낮아지거나(전/후 복잡도와 n의 크기를 계획에 적는다) 측정으로 확인한 병목을 줄일 때
  - 정확성: GDD 요구사항이나 엔진 계약(생명주기·저장 실패·앱 백그라운드 등)을 지키는 데 필요할 때
- 만들지 않는 것
  - "나중에 필요할 수도 있는" 확장 지점·옵션·매개변수·설정값
  - 다른 함수를 전달만 하는 래퍼 함수, 한 곳에서만 쓰는 헬퍼·매니저 클래스 (같은 코드가 3곳 이상 반복되면 함수로 뺀다)
  - 다른 값으로 계산할 수 있는 상태를 따로 저장하는 변수 (성능을 위한 캐시는 예외, 이유를 주석에 적는다)
  - 같은 값을 여러 계층에서 반복 검사하는 코드 (입력이 들어오는 경계에서 한 번 검사한다)
  - 엔진에 이미 있는 기능(TArray 힙 함수·Algo·FMath·FTimerManager 등)을 다시 구현한 코드
- 짧게 쓰라는 뜻이 아니다. 한 줄에 여러 동작을 넣지 않는다 (중첩 삼항 연산자, 부작용이 있는 식 연결 금지)

## 설계 원칙 (SOLID — 이유가 있을 때만 적용)
- S: 책임 한 줄에 '그리고'가 들어가면, 두 부분이 서로 다른 이유로 바뀌는지 계획에 적는다. 같은 이유로 바뀌거나 코드가 작으면 합쳐 둔다
- O: 기획 수치는 DataTable에서 FindRow로 읽는다(평균 O(1)). 매 프레임처럼 반복되는 경로가 생길 때만 초기화 때 값으로 캐시한다. 예외: 영업 규칙(FBusinessSim)은 하루 시작 때 필요한 행을 FBusinessDayInput으로 복사해 받는다 — DataTable 없이 테스트하기 위한 경계다(architecture.md 1.6). 종류가 고정된 집합(재료·작업대 상태·입력 종류)은 enum으로 구분하고 종류별 분기는 switch로 쓴다. switch는 default 없이 모든 값을 나열하고 끝에 checkNoEntry()를 둔다
- L: 엔진 생명주기 함수(BeginPlay·EndPlay·NativeConstruct·NativeDestruct·Initialize·Deinitialize 등)를 override하면 Super를 호출한다. 부모 동작을 대체하려고 Super를 생략하면 이유를 주석에 적고, 부모가 보장하던 결과(반환값·호출 계약)는 지킨다
- I: BP에는 화면이 실제로 쓰는 함수·델리게이트만 노출한다 (멤버는 기본 BlueprintReadOnly)
- D: 로직 클래스는 위젯 클래스를 include·참조하지 않는다. 화면 갱신은 델리게이트로만 알린다
- 구현체가 하나뿐인 UInterface·추상 클래스는 만들지 않는다

## 파일 형식
- UTF-8이 아닌 파일을 발견하면 수정하지 말고 보고한다
- 화면에 표시할 한국어 문구는 C++에 쓰지 않는다. 위젯(BP)이나 CSV에 둔다

## 코딩 컨벤션
- UE 네이밍: A(Actor) U(UObject) F(구조체) E(열거형) I(인터페이스), bool은 b 접두사
- BP에 노출하는 열거형은 UENUM(BlueprintType) + enum class E이름 : uint8. BP가 쓰지 않는 내부 열거형(BusinessSim.h)은 enum class E이름 : uint8만 쓴다(I 원칙)
- UObject 포인터 멤버는 UPROPERTY() + TObjectPtr
- 헤더는 전방 선언, include는 .cpp에. .generated.h는 헤더의 마지막 include
- 본문이 한 줄인 접근자(값 반환·단순 대입)만 헤더의 클래스 본문 안에 정의한다
- 상태를 바꾸지 않는 멤버 함수는 const. FString·TArray·구조체 인자는 const&, int32·float·포인터는 값으로 전달한다
- auto는 반복자·람다에만 쓰고, 범위 for는 const T& 또는 T&로 받는다
- 매직 넘버를 쓰지 않는다(0·1·-1 제외). 기획 수치는 CSV, 그 밖의 상수는 static constexpr 멤버로 둔다
- 종류 구분은 문자열 대신 enum. 문자열은 화면 표시 FText, 이름 키(DataTable 행 이름 등) FName, 가공 FString으로 구분한다
- Row 구조체 멤버는 CSV 컬럼 순서와 같게 둔다. 패딩 최적화는 대량 생성·순회하는 구조체에만, 측정 후 적용한다
- 반드시 있어야 하는 참조가 없으면 check로 멈춘다(Shipping에서는 제거되므로 부작용 있는 식을 넣지 않는다). 없을 수 있는 경우는 if + UE_LOG로 처리하고, 조용히 return하지 않는다
- 로그는 LogAIGame 카테고리로 남긴다. LogTemp를 쓰지 않는다
- GetAllActorsOfClass나 이름 문자열 검색으로 객체를 찾지 않는다. 초기화 때 참조를 보관하거나 Subsystem으로 접근한다

## 주석 규칙
- 헤더에는 호출하는 쪽이 알아야 할 계약(언제 부르나·전제 조건·부작용)을 /** */로, .cpp에는 '왜 이렇게 했나'를 //로 쓴다. 한국어로 쓰고 식별자·엔진 용어는 원문 그대로
- UPROPERTY·UFUNCTION 바로 위의 주석은 에디터 툴팁이 된다. 툴팁에 보이면 안 되는 메모는 //~ 로 쓴다
- 반드시 다는 곳
  - UCLASS/USTRUCT 선언 위: 책임 한 줄 (architecture.md와 같은 문장)
  - public 함수·BP 노출 멤버·델리게이트 위: /** */ 한 줄 계약. 이름만으로 뜻이 드러나는 한 줄 접근자는 생략한다
  - 뻔한 대안을 버린 곳: `// ADR-NNNN: <한 줄 이유>` 또는 `// SOLID-<S|O|L|I|D>: <버린 방식> 대신 <택한 방식> — <얻는 것>`. 실제로 대안을 검토한 곳에만 단다
  - 엔진·플랫폼의 직관적이지 않은 동작에 기대는 줄(예: 일시정지 때 멈추는 시간, 0 이하 간격이면 해제되는 타이머): 동작 + 확인한 출처(헤더 파일명). 확인 못 했으면 `// TODO(태스크ID): 확인 필요`
  - 호출 순서가 바뀌면 깨지는 줄: `// 순서 중요: <이유>`
- 쓰지 않는 것: 코드를 그대로 읽어 주는 주석, 변경 이력, 주석 처리한 옛 코드, 확인 안 한 추측
- 코드를 바꾸면 그 코드를 설명하는 주석도 같은 변경에서 고친다
- 미완성은 `// TODO(태스크ID): 내용` 형식만 쓴다

## 금지
- Content/ 아래 .uasset / .umap 수정 (바이너리). 에디터 작업이 필요하면 보고의 '사람이 확인·작업할 항목'에 적는다
- Config/*.ini, .uproject는 수정 전에 확인받는다
- Binaries/, Intermediate/, 엔진 소스는 수정하지 않는다