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
	WaitingCustomers.Reset();
	NextCustomerId = 1;
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

	const double FirstArrivalTime = Now + Random.FRandRange(Input.Day.ArrivalMin, Input.Day.ArrivalMax);
	PushEvent(FirstArrivalTime, EBusinessEventType::CustomerArrive);

	return EBusinessChange::Customers | EBusinessChange::Workbench | EBusinessChange::Steamer
		| EBusinessChange::Shelf | EBusinessChange::Gold;
}

EBusinessChange FBusinessSim::ProcessDueEvents(double Now)
{
	EBusinessChange Changes = EBusinessChange::None;
	while (!EventHeap.IsEmpty() && EventHeap.HeapTop().Time <= Now)
	{
		FBusinessEvent Event;
		EventHeap.HeapPop(Event);

		switch (Event.Type)
		{
		case EBusinessEventType::CustomerArrive:
		{
			PushEvent(Event.Time + Random.FRandRange(Input.Day.ArrivalMin, Input.Day.ArrivalMax), EBusinessEventType::CustomerArrive);

			FCustomer Customer;
			Customer.bAnyOrder = Random.FRand() < Input.Day.AnyOrderRate;
			if (!Customer.bAnyOrder)
			{
				int32 BaseWeightSum = 0;
				int32 FruitWeightSum = Input.Balance.NoFruitWeight;
				for (const TPair<EIngredient, FIngredientRow>& Pair : Input.Ingredients)
				{
					if (IsBaseIngredient(Pair.Key))
					{
						BaseWeightSum += Pair.Value.OrderWeight;
					}
					else
					{
						FruitWeightSum += Pair.Value.OrderWeight;
					}
				}

				int32 BaseRoll = Random.RandRange(0, BaseWeightSum - 1);
				for (const TPair<EIngredient, FIngredientRow>& Pair : Input.Ingredients)
				{
					if (!IsBaseIngredient(Pair.Key))
					{
						continue;
					}
					Customer.Order.Base = Pair.Key;
					BaseRoll -= Pair.Value.OrderWeight;
					if (BaseRoll < 0)
					{
						break;
					}
				}

				// 0보다 작으면 '과일 없음'(Order.Fruit 기본값 None)
				int32 FruitRoll = Random.RandRange(0, FruitWeightSum - 1) - Input.Balance.NoFruitWeight;
				if (FruitRoll >= 0)
				{
					for (const TPair<EIngredient, FIngredientRow>& Pair : Input.Ingredients)
					{
						if (IsBaseIngredient(Pair.Key))
						{
							continue;
						}
						Customer.Order.Fruit = Pair.Key;
						FruitRoll -= Pair.Value.OrderWeight;
						if (FruitRoll < 0)
						{
							break;
						}
					}
				}
			}

			if (WaitingCustomers.Num() >= Input.Day.MaxQueue)
			{
				continue;
			}

			Customer.Id = NextCustomerId++;
			Customer.LookIndex = Random.RandRange(0, Input.CustomerLookCount - 1);
			Customer.ExpireTime = Event.Time + Input.Day.Patience;
			WaitingCustomers.Add(Customer);

			PushEvent(Customer.ExpireTime, EBusinessEventType::CustomerExpire, Customer.Id);
			Changes |= EBusinessChange::Customers;
			continue;
		}
		case EBusinessEventType::CustomerExpire:
		{
			const int32 WaitingIndex = WaitingCustomers.IndexOfByPredicate([&Event](const FCustomer& Waiting)
			{
				return Waiting.Id == Event.CustomerId;
			});
			if (WaitingIndex == INDEX_NONE)
			{
				continue;
			}
			
			WaitingCustomers.RemoveAt(WaitingIndex);
			++Result.MissedCustomers;
			Changes |= EBusinessChange::Customers;
			continue;
		}
		case EBusinessEventType::DayEnd:
			// TODO(T11): 작업대 Empty · 레시피 비우기 (architecture.md 4.7 F1)
			// TODO(T13): 진열 개수 · 작업대를 폐기 수에 더하고 진열대 비우기
			WaitingCustomers.Reset();
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

void FBusinessSim::PushEvent(double Time, EBusinessEventType Type, int32 CustomerId)
{
	FBusinessEvent Event;
	Event.Time = Time;
	Event.Seq = NextEventSeq;
	Event.Type = Type;
	Event.CustomerId = CustomerId;
	EventHeap.HeapPush(Event);
	++NextEventSeq;
}
