#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "Shop/ShopTypes.h"

enum class EBusinessEventType : uint8
{
	CustomerArrive,
	CustomerExpire,
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

struct FBusinessEvent
{
	/** 월드 시간(초) */
	double Time = 0.0;
	int32 Seq = 0;
	EBusinessEventType Type = EBusinessEventType::DayEnd;
	int32 CustomerId = 0;

	bool operator<(const FBusinessEvent& Other) const;
};

/** 하루 영업 관련 데이터 */
struct FBusinessDayInput
{
	FDayRow Day;
	FSteamerRow Steamer;
	FBalanceRow Balance;

	/** 해금된 재료만 */
	TMap<EIngredient, FIngredientRow> Ingredients;
	int32 GoldAtStart = 0;

	/** 1 이상 */
	int32 CustomerLookCount = 1;
	int32 RandomSeed = 0;
};

/** 영업 하루의 규칙을 현재 시각을 인자로 받아 처리한다 */
class FBusinessSim
{
public:
	//영업 시작 
	EBusinessChange StartDay(const FBusinessDayInput& InInput, double Now);

	/** Now 이하인 이벤트를 (시각, 순번) 순으로 모두 처리한다.*/
	EBusinessChange ProcessDueEvents(double Now);

	/** 힙 맨 앞 이벤트 시각. 영업이 끝나 힙이 비면 unset */
	TOptional<double> GetNextEventTime() const;

	const TArray<FCustomer>& GetWaitingCustomers() const { return WaitingCustomers; }

	double GetDayEndTime() const { return DayEndTime; }
	const FDayResult& GetResult() const { return Result; }
	const FBusinessDayInput& GetInput() const { return Input; }

	int32 GetCurrentGold() const { return Input.GoldAtStart - Result.Rent + Result.Sales - Result.IngredientCost; }

	bool IsDayOver() const { return EventHeap.IsEmpty(); }

private:
	FBusinessDayInput Input;
	FRandomStream Random;

	TArray<FCustomer> WaitingCustomers;
	int32 NextCustomerId = 1;

	/** (Time, Seq) 최소 힙. TArray::HeapPush / HeapPop */
	TArray<FBusinessEvent> EventHeap;
	int32 NextEventSeq = 0;

	FDayResult Result;

	/** 월드 시간(초). 화면의 남은 시간 계산용 원본 시각 */
	double DayEndTime = 0.0;

	void PushEvent(double Time, EBusinessEventType Type, int32 CustomerId = 0);
};
