---
description: 현재 변경사항을 CLAUDE.md와 해당 태스크의 설계 절 기준으로 리뷰한다. 코드는 수정하지 않는다.
disable-model-invocation: true
---

태스크 $ARGUMENTS 의 변경사항(git diff HEAD와 git status의 새 파일)을 리뷰해줘. 코드는 고치지 말고 지적만 해.
기준: CLAUDE.md, tasks.md의 $ARGUMENTS 행, 그 행이 가리키는 architecture.md 절과 ADR.

- 설계 일치: 헤더 선언 · 책임 · 상태 소유자가 architecture.md 3절과 같은가
- 범위: 태스크 밖 파일 수정, 계획에 없던 클래스 · 함수 · 변수
- 시간 · 이벤트: Tick, SetTimer 추가, GameMode 밖에서 GetTimeSeconds, 힙에 포인터, HandleChanges 누락
- 테스트: 규칙을 바꿨는데 테스트 없음, 근거 없이 바뀐 기대값, 아무것도 검사하지 않는 테스트
- UE: UPROPERTY 없는 UObject 포인터, UE 5.5에 없는 API, Super 누락, Build.cs 의존 누락
- 데이터 · 문구: 하드코딩 수치, C++ 안의 한국어 문구, Row 멤버와 CSV 컬럼 순서
- 모바일: 백그라운드/복귀, 저장 실패 처리
- 주석: 책임 한 줄 · 계약 주석 누락, 코드를 읽어 주는 주석

형식: [높음/중간/낮음] 파일:줄 — 문제 — 근거(CLAUDE.md 규칙 또는 설계 절) — 제안
정확성 · 설계 위반이 아닌 취향 문제는 [낮음]으로만 적고, 지적할 것이 없으면 없다고 쓴다.
