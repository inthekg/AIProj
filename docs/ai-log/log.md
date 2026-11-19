# AI 한 줄 로그

| 시각 | 태스크 | 도구 | 목적 | 판정 | 근거(짧게) | 커밋/세션 |
|---|---|---|---|---|---|---|
| 2026-11-19 | 2단계 설계 · 1단계 아키텍처 골격 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | 클래스 · 상태 소유자 · 데이터 흐름 · 시간 기준 · 테스트 경계 · 화면 표시 구조 설계, 결정 D1~D9 제시 → ADR-0004~0012 |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
| 2026-11-19 | 2단계 설계 · 2단계 데이터 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | 데이터 원본 비교(enum + DataTable vs switch), 스키마 4표(data-schema.md), 결정 D10 · D11 → ADR-0013 · 0014 |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
| 2026-11-19 | 2단계 설계 · 3단계 클래스 상세 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | 헤더 선언 · UI 계약 · 엔진 API 31개 선언 확인 · 추적표, 결정 D12~D15 → ADR-0015, 자체 점검으로 GetCurrentGold 이중 합산 수정 |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
| 2026-11-19 | 2단계 설계 · 4단계 시나리오 워크스루 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | 정상 1 + 예외 5 시퀀스 다이어그램, 워크스루로 결함 2건(F1 작업대 미초기화 · F2 팝업 위 오버레이) 찾아 3단계 설계 수정 |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
| 2026-11-19 | 2단계 설계 · 5단계 수치 초안 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | Data/*.csv 4개 수치 초안, 밸런스 시뮬레이션 Scripts/balance_sim.py, 결정 D16~D18 → ADR-0016 · 0017 (임대료 · 해금 때 임대료 상승) |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
| 2026-11-19 | 2단계 설계 · 6단계 태스크 분해 · 설계 마감 | Claude 채팅 (레포 · 엔진 소스 폴더 연결) | 태스크 T01~T20(docs/03_impl/tasks.md, Must 20h 예산), 설계 문서 교차 검증으로 충돌 수정(CLAUDE.md 3곳 · GDD v1.3 · architecture 1.5/1.7 · 머리말), 구조 요약 docs/02_design/architecture-summary.html + 구조도 img/architecture-overview.png · .svg |  |  | 세션 https://claude.ai/code/session_01Jkgos16XKhREeYEQiratjL |
