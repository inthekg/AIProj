# AIGame — UE 5.5 Android 모바일 (5일 프로젝트)

## 문서 위치
- 기획: docs/01_planning/GDD.md
- 설계: docs/02_design/architecture.md, 결정 기록: docs/02_design/adr/
- 태스크: docs/03_impl/tasks.md (태스크 ID 기준으로 작업)

## 기술 환경
- Unreal Engine 5.5. 다른 버전 API를 쓰지 않는다. 확실하지 않으면 엔진 헤더(D:/UE_5.5/Engine/Source, D:/UE_5.5/Engine/Plugins)를 먼저 확인하고, 확인 못 하면 "확인 필요"로 표시한다
- 타깃: Android arm64 (Development / Shipping)
- 컴파일: Scripts/Build.bat  /  패키징: Scripts/PackageAndroid.bat [Development|Shipping] (기본 Development)
- PackageAndroid.bat은 에디터 DLL을 다시 빌드하지 않는다(-skipbuildeditor). C++를 고쳤으면 Build.bat을 먼저 돌린다

## 작업 방식
- 구현 전 계획을 먼저 제시하고 승인을 기다린다. 계획에는 수정·생성할 파일과 새 클래스·함수 목록을 포함한다
- 새 클래스·함수·변수를 추가하기 전에 기존 코드를 확장해 해결할 수 있는지 먼저 검토하고, 추가가 필요하면 이유를 적는다
- 한 번에 태스크 하나만 다룬다. 범위 밖 파일은 수정하지 않는다
- 하나의 태스크가 완성되면 Scripts/Build.bat으로 컴파일하고 결과(경고 포함)를 보고한다. 작은 추가에는 빌드하지않는다. 
- 빌드 전에 에디터가 켜져 있는지 물어본다 (Live Coding이 켜져 있으면 외부 빌드가 거부됨)
- git commit / push는 하지 않는다. 사람이 diff를 리뷰한 뒤 커밋한다
- 태스크를 마치면 docs/ai-log/log.md 표에 한 줄 초안을 추가한다. 판정·근거 칸은 비워 두고 사람이 채운다. 커밋 칸은 커밋 전이므로 세션 정보만 적는다

## 아키텍처 규칙 (설계 단계에서 채움)
- 로직·데이터·저장은 C++, 위젯 배치·연출은 BP. BP는 C++이 노출한 함수와 델리게이트만 쓴다
- 게임 진행 상태는 <소유 클래스> 한 곳에서 관리하고, UI는 델리게이트를 구독해 갱신한다
- Tick은 쓰지 않는다(필요하면 이유를 계획에 적는다). Timer·이벤트 기반으로 만든다
- 기획 수치는 Data/*.csv → DataTable. 코드에 하드코딩하지 않는다

## 코딩 컨벤션
- UE 네이밍: A(Actor) U(UObject) F(구조체) E(열거형) I(인터페이스), bool은 b 접두사
- UObject 포인터 멤버는 UPROPERTY() + TObjectPtr
- 헤더는 전방 선언, include는 .cpp에
- 주석은 핵심 기능인 경우 "왜" 이 방식으로 구현했는지를 한국어로 쓴다. 그 외, 복잡한 로직에는 간단한 설명 주석 추가 

## 금지
- Content/ 아래 .uasset / .umap 수정 (바이너리). 에디터 작업이 필요하면 '사람이 할 일' 목록으로 남긴다
- Config/*.ini, .uproject는 수정 전에 확인받는다
- Binaries/, Intermediate/, 엔진 소스는 수정하지 않는다
