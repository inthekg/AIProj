# 태스크 목록

| ID | 태스크 | 완료 조건 | 예상 h | 실제 h | AI 태그 | 커밋 |
|---|---|---|---|---|---|---|
| T01 | [M3 · AI] 공통 타입 · 데이터 행 · LogAIGame (설계 3.1 · 3.2) — 선행 없음 | Build.bat 성공 · 경고 0. Private/Tests의 빈 테스트 1개가 Session Frontend에 보임 | 1.0 |  |  |  |
| T02 | [M3 · AI] FShopProgress (구매 · 결산 · 파산 · 임대료) + 테스트 5 (3.3) — 선행 T01 | AIGame.ShopProgress.* 5개 통과 | 1.0 |  |  |  |
| T03 | [M3 · AI] UShopGameInstance: 데이터 검증 · 조회 · 구매 · FillDayInput · ApplyDayResult, 저장 제외 (3.5) — 선행 T02 | 빌드 성공. T04 뒤 PIE 시작 로그에 검증 통과 · 진행 값 출력 | 1.0 |  |  |  |
| T04 | [M3 · 나] CSV 4개 → DataTable 4개, BP_ShopGameInstance(DataTable · 재료 이미지), Project Settings Game Instance Class — 선행 T03 | PIE에서 검증 check에 걸리지 않음, 한글 DisplayName 정상 | 0.5 |  |  |  |
| T05 | [M4 · AI] UShopSaveGame · 불러오기(버전) · SaveProgress · 백그라운드 저장 구독 (3.4 · 3.5) — 선행 T04 | PIE: 구매 → PIE 재시작 후 값 유지, 결산 저장 로그 | 1.0 |  |  |  |
| T06 | [M4 · 나] 실기기 저장 확인 — 선행 T05 · T10 | 홈 버튼 → 강제 종료 → 재실행 값 유지, 게임 오버 즉시 초기화 (R41 · R42 · R44) | 0.5 |  |  |  |
| T07 | [M1 · AI] FBusinessSim 뼈대: 이벤트 힙 · Advance · StartDay(임대료) · DayEnd · 시드 + 테스트 5 (3.6) — 선행 T01 | Events.SameTimeOrder · ZeroLockProcessed · DayEnd.AtDuration · Rent.ChargedAtStart · Random.SameSeedSameDay 통과 | 1.5 |  |  |  |
| T08 | [M1 · AI] 손님: 도착 간격 · 주문(가중치 · 아무거나) · 대기열 상한 · 인내심(지연 삭제) · 외형 + 테스트 7 — 선행 T07 | Customer.ArrivalInterval · QueueFullNoShow · OrderOnlyUnlocked · AnyOrderRate · LeavesAfterExpire · ServedNotExpired · LookIndexInRange 통과 | 1.5 |  |  |  |
| T09 | [M1 · AI] ABusinessGameMode: 타이머 1개 · HandleChanges · 델리게이트 7 · 입력 함수 · 백그라운드/복귀 · 결산 연결 (3.7) — 선행 T03 · T08 | 빌드 성공. T10 뒤 PIE 로그로 도착 · 만료 · 영업 종료 → 결산 저장, 일시정지 중 시간 멈춤 | 1.5 |  |  |  |
| T10 | [M1 · 나] L_Lobby · L_Business 맵, BP_BusinessGameMode(Default Pawn None · 사람 이미지 3~4), 로비 GameMode BP, Default Map — 선행 T09 | PIE에서 로비 → 영업 레벨 이동, 3분 뒤 결산 로그 | 0.5 |  |  |  |
| T11 | [M2 · AI] 작업대 전이표 · 입력 잠금 · 재료비 · 쓰레기 · 버리기 + 테스트 5 — 선행 T07 | Workbench.TransitionTable · TrashCount · InputLocked, Cost.ChargedOnTap · AllowsDebt 통과 | 1.5 |  |  |  |
| T12 | [M2 · AI] 찜기 게이지 · 다시 찌기 + 테스트 2 — 선행 T11 | Steamer.GaugeConsumed · Refill 통과 | 0.5 |  |  |  |
| T13 | [M2 · AI] 배분 · 진열대(순서 · 상한 · 폐기) · 도착 즉시 구매 · 판매가 · 결산 집계 + 테스트 9 — 선행 T08 · T12 | Distribute.* 3 · Shelf.OrderAndRemove · Price.SingleAndMix · Customer.BuyFromShelfOnArrive · AnyPicksByCount · DayEnd.DiscardAndClear · ResultCounts 통과, AIGame.* 33개 전부 통과 | 1.5 |  |  |  |
| T14 | [M2 · 나] PIE 제작 루프: 임시 BP 버튼으로 재료 · 찜기 · 버리기 — 선행 T10 · T13 | 설계 4.1 · 4.4 시나리오를 PIE 로그로 재현 | 0.5 |  |  |  |
| T15 | [M5 · 나] 로비 위젯: 일차 · 골드 · "파산위기!" · 다음 해금(이름 · 비용 · 임대료 +N) · 찜기 업그레이드 · 영업 시작 — 선행 T05 · T10 | 구매하면 화면 갱신, 빚이면 표지판 표시 | 1.0 |  |  |  |
| T16 | [M5 · 나] 영업 HUD: 상단(남은 시간 · 매출 · 표지판 · 일시정지) · 재료통 · 작업대 · 찜기 · 버리기 — 선행 T14 | 버튼으로 4.1 정상 흐름 플레이 | 1.5 |  |  |  |
| T17 | [M5 · 나] 손님 줄 슬롯 5: 사람 이미지 · 머리 위 말풍선 · 인내심 게이지 — 선행 T16 | 도착 · 이탈 · 판매 때 슬롯이 당겨짐 | 1.0 |  |  |  |
| T18 | [M5 · 나] 진열대 3 × 5: 제품 겹침 이미지 · 개수 · 행 위치 k/(n+1) — 선행 T16 | 칸 추가 · 제거, 행 수 변화 확인 | 1.0 |  |  |  |
| T19 | [M5 · 나] 결산 · 게임 오버 팝업, 일시정지 오버레이(계속하기) — 선행 T16 | 4.6 시나리오 재현, 앱 복귀 때 오버레이(영업 중일 때만) | 1.0 |  |  |  |
| T20 | [M5 · 나] 연출: 재료 올리기 · 감싸기 · 쓰레기(모자이크 → 아래로), 길이 = Balance 시간 — 선행 T16 | 4.2 · 4.4 시나리오에서 연출과 입력 잠금이 맞음 | 0.5 |  |  |  |

- 출처: docs/02_design/architecture.md 6절 (2단계 설계 6단계, 2026-11-19). 예상 h는 AI 태스크의 사람 검토 시간 포함
- Must 예산 대비: M1 5.0/5 · M2 4.0/4 · M3 3.5/3 · M4 1.5/2 · M5 6.0/6 — 합계 20.0/20h. Must 밖: Should(효과음), 실기기 SafeZone 점검(GDD 리스크 3), 플레이 테스트 후 수치 조정
