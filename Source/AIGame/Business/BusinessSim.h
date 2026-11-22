#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "Shop/ShopTypes.h"

/** 작업대 입력 종류. 전이표의 열 */
enum class EWorkbenchInput : uint8
{
	Base,
	Fruit,
	Steamer,
	Discard
};

enum class EBusinessEventType : uint8
{
	CustomerArrive,
	CustomerExpire,
	SteamerRefilled,
	WorkbenchDone,
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

	/** 재료통 탭. 무시한 입력이면 None*/
	EBusinessChange TapIngredient(EIngredient Ingredient, double Now);

	/** 찜기 탭. 무시한 입력이면 None */
	EBusinessChange TapSteamer(double Now);

	/** 버리기 버튼. 무시한 입력이면 None */
	EBusinessChange TapDiscard(double Now);

	/**Now 이하인 이벤트를 (시각, 순번) 순으로 모두 처리한다.*/
	EBusinessChange ProcessDueEvents(double Now);

	TOptional<double> GetNextEventTime() const;

	/** 작업대 전이표 */
	static EWorkbenchState NextWorkbenchState(EWorkbenchState Current, EWorkbenchInput WorkbenchInput);

	const TArray<FCustomer>& GetWaitingCustomers() const { return WaitingCustomers; }
	EWorkbenchState GetWorkbenchState() const { return WorkbenchState; }
	const FRecipe& GetWorkbenchRecipe() const { return WorkbenchRecipe; }
	int32 GetSteamerGauge() const { return SteamerGauge; }

	double GetSteamerRefillEndTime() const { return SteamerRefillEndTime; }

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

	EWorkbenchState WorkbenchState = EWorkbenchState::Empty;

	/** 작업대에 올린 재료. 쓰레기가 된 입력은 넣지 않고, 작업대가 Empty가 될 때 비운다 */
	FRecipe WorkbenchRecipe;

	/** 월드 시간(초). 재료통 · 찜기 · 버리기 입력 무시가 끝나는 시각 */
	double InputLockEndTime = 0.0;

	int32 SteamerGauge = 0;

	/** 월드 시간(초). 다시 찌는 중이 아니면 0 */
	double SteamerRefillEndTime = 0.0;

	FDayResult Result;

	/** 월드 시간(초). 화면의 남은 시간 계산용 원본 시각 */
	double DayEndTime = 0.0;

private:
	void PushEvent(double Time, EBusinessEventType Type, int32 CustomerId = 0);

	/** 작업대를 Empty로 되돌리고 레시피를 비운다 */
	void ClearWorkbench();

	/** 작업대를 쓰레기로 바꾸고 쓰레기 수 +1 · TrashTime 동안 입력 잠금 · WorkbenchDone 예약을 함께 한다. */
	void TrashWorkbench(double Now);
};
