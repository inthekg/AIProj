#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "ShopTypes.h"

/** 힙에 넣는 시간 이벤트 종류(D1) */
enum class EBusinessEventType : uint8
{
	CustomerArrive,
	DayEnd
};

/** FBusinessSim 호출 한 번으로 바뀐 부분. ABusinessGameMode가 이 값으로 델리게이트를 고른다 */
enum class EBusinessChange : uint8
{
	None      = 0,
	Customers = 1 << 0,
	Workbench = 1 << 1,
	Steamer   = 1 << 2,
	Shelf     = 1 << 3,
	Gold      = 1 << 4,
	DayEnded  = 1 << 5
};
ENUM_CLASS_FLAGS(EBusinessChange);

/** 시간 이벤트 하나. 힙은 (Time, Seq) 오름차순 */
struct FBusinessEvent
{
	/** 월드 시간(초) */
	double Time = 0.0;

	/** 같은 Time이면 먼저 넣은 것이 먼저(architecture.md 1.5) */
	int32 Seq = 0;

	EBusinessEventType Type = EBusinessEventType::DayEnd;

	/** (Time, Seq) 순서. TArray 힙 함수의 기본 술어(TLess)가 이 연산자로 비교한다 */
	bool operator<(const FBusinessEvent& Other) const;
};

/** 하루 영업 입력. GameInstance가 행 · 시작 골드를, GameMode가 외형 수 · 시드를 채운다. 테스트는 직접 만든다 */
struct FBusinessDayInput
{
	FDayRow Day;
	FSteamerRow Steamer;
	FBalanceRow Balance;

	/** 해금된 재료만 */
	TMap<EIngredient, FIngredientRow> Ingredients;

	/** 영업 시작 때의 Progress.Gold. 현재 골드 계산의 기준(결산 뒤에도 바뀌지 않는다) */
	int32 GoldAtStart = 0;

	/** 1 이상 */
	int32 CustomerLookCount = 1;

	int32 RandomSeed = 0;
};

/** 영업 하루의 규칙(손님 · 주문 · 인내심, 작업대 · 찜기, 배분 · 진열대, 집계)을 현재 시각을 인자로 받아 처리한다 */
class FBusinessSim
{
public:
	/**
	 * 하루를 시작한다. 이전 상태를 모두 버리고 임대료를 Result.Rent에 기록한 뒤(ADR-0016 · ADR-0017) 영업 종료 · 첫 손님 도착 이벤트를 넣는다.
	 * 영역 플래그(Customers · Workbench · Steamer · Shelf · Gold)를 모두 돌려준다. DayEnded는 넣지 않는다.
	 */
	EBusinessChange StartDay(const FBusinessDayInput& InInput, double Now);

	/**
	 * Now 이하인 이벤트를 (시각, 순번) 순으로 모두 처리한다.
	 * 입력 처리 뒤에도 부른다 — 잠금 시간이 0이면 이벤트 시각이 Now와 같아 타이머로는 처리되지 않는다.
	 */
	EBusinessChange Advance(double Now);

	/** 힙 맨 앞 이벤트 시각. 영업이 끝나 힙이 비면 unset */
	TOptional<double> GetNextEventTime() const;

	double GetDayEndTime() const { return DayEndTime; }
	const FDayResult& GetResult() const { return Result; }

	/** 0보다 작으면 빚. Progress를 읽지 않으므로 결산 뒤에도 값이 같다(architecture.md 1.3) */
	int32 GetCurrentGold() const { return Input.GoldAtStart - Result.Rent + Result.Sales - Result.IngredientCost; }

	/** 영업 종료 이벤트는 영업이 끝날 때까지 힙에 남아 있으므로, 힙이 비면 영업이 끝난 것이다 */
	bool IsDayOver() const { return EventHeap.IsEmpty(); }

private:
	FBusinessDayInput Input;
	FRandomStream Random;

	/** (Time, Seq) 최소 힙. TArray::HeapPush / HeapPop */
	TArray<FBusinessEvent> EventHeap;
	int32 NextEventSeq = 0;

	FDayResult Result;

	/** 월드 시간(초). 화면의 남은 시간 계산용 원본 시각 */
	double DayEndTime = 0.0;

	/** 이벤트를 (Time, 다음 순번)으로 힙에 넣는다 */
	void PushEvent(double Time, EBusinessEventType Type);
};
