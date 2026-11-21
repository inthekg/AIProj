#include "BusinessSim.h"

bool FBusinessEvent::operator<(const FBusinessEvent& Other) const
{
	// TArray 힙은 같은 키끼리의 순서를 보장하지 않으므로 순번으로 넣은 순서를 지킨다(architecture.md 1.5)
	if (Time != Other.Time)
	{
		return Time < Other.Time;
	}
	return Seq < Other.Seq;
}

EBusinessChange FBusinessSim::StartDay(const FBusinessDayInput& InInput, double Now)
{
	Input = InInput;
	Random.Initialize(Input.RandomSeed);
	EventHeap.Reset();
	NextEventSeq = 0;
	Result = FDayResult();

	Result.Rent = Input.Balance.BaseRent;
	for (const TPair<EIngredient, FIngredientRow>& Pair : Input.Ingredients)
	{
		Result.Rent += Pair.Value.RentIncrease;
	}

	DayEndTime = Now + Input.Day.Duration;
	// 순서 중요: DayEnd를 가장 먼저 넣어 순번을 가장 작게 한다 — 영업 종료와 같은 시각의 이벤트보다 먼저 처리된다(architecture.md 4.2)
	PushEvent(DayEndTime, EBusinessEventType::DayEnd);

	// 첫 도착도 도착 간격으로 뽑는다(Scripts/balance_sim.py와 같은 방식). 영업 종료 뒤로 잡혀도 DayEnd가 힙을 비우므로 따로 막지 않는다
	const double FirstArrivalTime = Now + Random.FRandRange(Input.Day.ArrivalMin, Input.Day.ArrivalMax);
	PushEvent(FirstArrivalTime, EBusinessEventType::CustomerArrive);

	return EBusinessChange::Customers | EBusinessChange::Workbench | EBusinessChange::Steamer
		| EBusinessChange::Shelf | EBusinessChange::Gold;
}

EBusinessChange FBusinessSim::Advance(double Now)
{
	EBusinessChange Changes = EBusinessChange::None;
	while (!EventHeap.IsEmpty() && EventHeap.HeapTop().Time <= Now)
	{
		FBusinessEvent Event;
		EventHeap.HeapPop(Event);

		// 각 case는 continue로 다음 이벤트로 간다. switch를 빠져나오면 처리하지 않은 종류다
		switch (Event.Type)
		{
		case EBusinessEventType::CustomerArrive:
			// TODO(T08): 다음 도착 예약 · 주문 · 재고 구매 · 대기열 입장 (architecture.md 3.6 이벤트 처리 표)
			continue;
		case EBusinessEventType::DayEnd:
			// TODO(T08): 대기열 비우기
			// TODO(T11): 작업대 Empty · 레시피 비우기 (architecture.md 4.7 F1)
			// TODO(T13): 진열 개수 · 작업대를 폐기 수에 더하고 진열대 비우기
			// 남은 이벤트는 처리하지 않고 버린다(ADR-0004 D2)
			EventHeap.Reset();
			Changes |= EBusinessChange::Customers | EBusinessChange::Workbench | EBusinessChange::Steamer
				| EBusinessChange::Shelf | EBusinessChange::Gold | EBusinessChange::DayEnded;
			continue;
		}
		checkNoEntry();
	}
	return Changes;
}

TOptional<double> FBusinessSim::GetNextEventTime() const
{
	if (EventHeap.IsEmpty())
	{
		return TOptional<double>();
	}
	return EventHeap.HeapTop().Time;
}

void FBusinessSim::PushEvent(double Time, EBusinessEventType Type)
{
	FBusinessEvent Event;
	Event.Time = Time;
	Event.Seq = NextEventSeq;
	Event.Type = Type;
	EventHeap.HeapPush(Event);
	++NextEventSeq;
}
