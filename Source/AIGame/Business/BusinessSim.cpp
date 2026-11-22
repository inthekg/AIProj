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
	ClearDay();
	NextCustomerId = 1;
	NextEventSeq = 0;
	InputLockEndTime = 0.0;
	SteamerGauge = Input.Steamer.MaxGauge;
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

EBusinessChange FBusinessSim::TapIngredient(EIngredient Ingredient, double Now)
{
	// 순서 중요: 해금 검사가 IsBaseIngredient보다 먼저다 
	const FIngredientRow* Row = Input.Ingredients.Find(Ingredient);
	if (Row == nullptr || IsDayOver() || Now < InputLockEndTime)
	{
		return EBusinessChange::None;
	}

	const EWorkbenchInput WorkbenchInput = IsBaseIngredient(Ingredient) ? EWorkbenchInput::Base : EWorkbenchInput::Fruit;
	const EWorkbenchState NextState = NextWorkbenchState(WorkbenchState, WorkbenchInput);
	
	//입력 무시라면
	if (NextState == WorkbenchState)
	{
		return EBusinessChange::None;
	}

	// 쓰레기가 되는 탭도 재료비를 낸다(architecture.md 3.6 전이표 메모)
	Result.IngredientCost += Row->Cost;
	switch (NextState)
	{
	case EWorkbenchState::Base:
		WorkbenchState = NextState;
		WorkbenchRecipe.Base = Ingredient;
		InputLockEndTime = Now + Input.Balance.PlaceTime;
		return EBusinessChange::Workbench | EBusinessChange::Gold;

	case EWorkbenchState::BaseFruit:
		WorkbenchState = NextState;
		WorkbenchRecipe.Fruit = Ingredient;
		InputLockEndTime = Now + Input.Balance.PlaceTime;
		return EBusinessChange::Workbench | EBusinessChange::Gold;

	case EWorkbenchState::Trash:
		TrashWorkbench(Now);
		return EBusinessChange::Workbench | EBusinessChange::Gold;

	case EWorkbenchState::Empty:
	case EWorkbenchState::Wrapping:
		// 재료 입력으로는 갈 수 없는 상태
		break;
	}
	checkNoEntry();
	return EBusinessChange::None;
}

EBusinessChange FBusinessSim::TapSteamer(double Now)
{
	if (IsDayOver() || Now < InputLockEndTime || SteamerGauge == 0)
	{
		return EBusinessChange::None;
	}

	const EWorkbenchState NextState = NextWorkbenchState(WorkbenchState, EWorkbenchInput::Steamer);
	if (NextState == WorkbenchState)
	{
		return EBusinessChange::None;
	}

	--SteamerGauge;
	Result.IngredientCost += Input.Balance.RiceCakeCost;
	// 순서 중요: 아래 switch의 case가 모두 바로 반환하므로 다시 찌기 예약을 먼저 한다
	if (SteamerGauge == 0)
	{
		SteamerRefillEndTime = Now + Input.Steamer.RegenTime;
		PushEvent(SteamerRefillEndTime, EBusinessEventType::SteamerRefilled);
	}

	switch (NextState)
	{
	case EWorkbenchState::Wrapping:
		WorkbenchState = NextState;
		InputLockEndTime = Now + Input.Balance.WrapTime;
		//제작 완료
		PushEvent(InputLockEndTime, EBusinessEventType::WorkbenchDone);
		return EBusinessChange::Workbench | EBusinessChange::Steamer | EBusinessChange::Gold;

	case EWorkbenchState::Trash:
		TrashWorkbench(Now);
		return EBusinessChange::Workbench | EBusinessChange::Steamer | EBusinessChange::Gold;

	case EWorkbenchState::Empty:
	case EWorkbenchState::Base:
	case EWorkbenchState::BaseFruit:
		// 찜기 입력으로는 갈 수 없는 상태
		break;
	}
	checkNoEntry();
	return EBusinessChange::None;
}

EBusinessChange FBusinessSim::TapDiscard(double Now)
{
	if (IsDayOver() || Now < InputLockEndTime)
	{
		return EBusinessChange::None;
	}
	if (NextWorkbenchState(WorkbenchState, EWorkbenchInput::Discard) == WorkbenchState)
	{
		return EBusinessChange::None;
	}

	TrashWorkbench(Now);
	return EBusinessChange::Workbench;
}

/* Empty->Base->Fruit(선택)->Steamer가 정상 루트*/
EWorkbenchState FBusinessSim::NextWorkbenchState(EWorkbenchState Current, EWorkbenchInput WorkbenchInput)
{
	switch (Current)
	{
	case EWorkbenchState::Empty:
		switch (WorkbenchInput)
		{
		case EWorkbenchInput::Base:
			return EWorkbenchState::Base;
		case EWorkbenchInput::Fruit:
		case EWorkbenchInput::Steamer:
			return EWorkbenchState::Trash;
		case EWorkbenchInput::Discard:
			return EWorkbenchState::Empty;
		}
		break;
	case EWorkbenchState::Base:
		switch (WorkbenchInput)
		{
		case EWorkbenchInput::Fruit:
			return EWorkbenchState::BaseFruit;
		case EWorkbenchInput::Steamer:
			return EWorkbenchState::Wrapping;
		case EWorkbenchInput::Base:
		case EWorkbenchInput::Discard:
			return EWorkbenchState::Trash;
		}
		break;
	case EWorkbenchState::BaseFruit:
		switch (WorkbenchInput)
		{
		case EWorkbenchInput::Steamer:
			return EWorkbenchState::Wrapping;
		case EWorkbenchInput::Base:
		case EWorkbenchInput::Fruit:
		case EWorkbenchInput::Discard:
			return EWorkbenchState::Trash;
		}
		break;
	case EWorkbenchState::Wrapping:
	case EWorkbenchState::Trash:
		return Current;
	}
	checkNoEntry();
	return Current;
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
				Customer.Order = PickComboOrder();
			}

			// 순서 중요: 재고 확인이 대기열 상한 검사보다 먼저다
			if (TryBuyFromShelf(Customer))
			{
				Changes |= EBusinessChange::Shelf | EBusinessChange::Gold;
				continue;
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
		case EBusinessEventType::SteamerRefilled:
			SteamerGauge = Input.Steamer.MaxGauge;
			SteamerRefillEndTime = 0.0;
			Changes |= EBusinessChange::Steamer;
			continue;
		case EBusinessEventType::WorkbenchDone:
			// 순서 중요: 배분이 WorkbenchRecipe를 읽으므로 ClearWorkbench보다 먼저다.
			if (WorkbenchState == EWorkbenchState::Wrapping)
			{
				Changes |= DistributeProduct();
			}
			ClearWorkbench();
			Changes |= EBusinessChange::Workbench;
			continue;
		case EBusinessEventType::DayEnd:
			// GDD 하루 진행: 남은 재고와 작업대 위 재료는 폐기한다. 순서 중요: 진열대 · 작업대를 비우기 전에 센다
			for (const FShelfEntry& Entry : Shelf)
			{
				Result.DiscardCount += Entry.Count;
			}
			
			if (WorkbenchState != EWorkbenchState::Empty && WorkbenchState != EWorkbenchState::Trash)
			{
				++Result.DiscardCount;
			}
			ClearDay();
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

void FBusinessSim::Sell(const FRecipe& Recipe, int32 WaitingIndex)
{
	const int32 BasePrice = Input.Ingredients.FindChecked(Recipe.Base).Price;
	if (Recipe.Fruit == EIngredient::None)
	{
		Result.Sales += BasePrice;
	}
	else
	{
		const int32 FruitPrice = Input.Ingredients.FindChecked(Recipe.Fruit).Price;
		Result.Sales += FMath::RoundToInt((BasePrice + FruitPrice) * Input.Balance.MixMultiplier);
	}
	++Result.SoldCount;
	if (WaitingIndex != INDEX_NONE)
	{
		WaitingCustomers.RemoveAt(WaitingIndex);
	}
}

void FBusinessSim::ClearWorkbench()
{
	WorkbenchState = EWorkbenchState::Empty;
	WorkbenchRecipe = FRecipe();
}

void FBusinessSim::TrashWorkbench(double Now)
{
	WorkbenchState = EWorkbenchState::Trash;
	// 쓰레기 수는 연출이 끝날 때가 아니라 판정 순간에 센다(architecture.md 3.6 전이표 메모)
	++Result.TrashCount;
	InputLockEndTime = Now + Input.Balance.TrashTime;
	PushEvent(InputLockEndTime, EBusinessEventType::WorkbenchDone);
}

void FBusinessSim::ClearDay()
{
	Shelf.Reset();
	WaitingCustomers.Reset();
	// 작업대를 되돌리지 않으면 감싸기 연출이 다시 재생된다(architecture.md 4.7 F1)
	ClearWorkbench();
	EventHeap.Reset();
	SteamerRefillEndTime = 0.0;
}

FRecipe FBusinessSim::PickComboOrder()
{
	FRecipe Order;
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
		Order.Base = Pair.Key;
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
			Order.Fruit = Pair.Key;
			FruitRoll -= Pair.Value.OrderWeight;
			if (FruitRoll < 0)
			{
				break;
			}
		}
	}
	return Order;
}

bool FBusinessSim::TryBuyFromShelf(const FCustomer& Customer)
{
	int32 ShelfIndex = INDEX_NONE;
	if (Customer.bAnyOrder)
	{
		if (!Shelf.IsEmpty())
		{
			// D12: 진열된 모찌 하나하나가 같은 확률(개수 비례). 주문 가중치와 같은 누적 방식으로 칸을 고른다
			int32 StockSum = 0;
			for (const FShelfEntry& Entry : Shelf)
			{
				StockSum += Entry.Count;
			}
			int32 StockRoll = Random.RandRange(0, StockSum - 1);
			for (ShelfIndex = 0; ShelfIndex < Shelf.Num(); ++ShelfIndex)
			{
				StockRoll -= Shelf[ShelfIndex].Count;
				if (StockRoll < 0)
				{
					break;
				}
			}
		}
	}
	else
	{
		//진열대에 이미 있는가
		ShelfIndex = Shelf.IndexOfByPredicate([&Customer](const FShelfEntry& Entry)
		{
			return Entry.Recipe == Customer.Order;
		});
	}

	if (ShelfIndex != INDEX_NONE)
	{
		// 순서 중요: 칸을 빼기 전에 그 칸의 조합으로 판다. 손님 대기열에 들어가지 않으므로 ID · 외형 · 만료 이벤트가 없다
		Sell(Shelf[ShelfIndex].Recipe, INDEX_NONE);
		--Shelf[ShelfIndex].Count;
		if (Shelf[ShelfIndex].Count == 0)
		{
			Shelf.RemoveAt(ShelfIndex);
		}
		return true;
	}
	return false;
}

EBusinessChange FBusinessSim::DistributeProduct()
{
	EBusinessChange Changes = EBusinessChange::None;
	const int32 WaitingIndex = WaitingCustomers.IndexOfByPredicate([this](const FCustomer& Waiting)
	{
		return Waiting.bAnyOrder || Waiting.Order == WorkbenchRecipe;
	});

	//원하는 손님이 존재
	if (WaitingIndex != INDEX_NONE)
	{
		Sell(WorkbenchRecipe, WaitingIndex);
		Changes |= EBusinessChange::Customers | EBusinessChange::Gold;
	}
	//진열대 배치
	else
	{
		const int32 ShelfIndex = Shelf.IndexOfByPredicate([this](const FShelfEntry& Entry)
		{
			return Entry.Recipe == WorkbenchRecipe;
		});
		if (ShelfIndex == INDEX_NONE)
		{
			FShelfEntry NewEntry;
			NewEntry.Recipe = WorkbenchRecipe;
			NewEntry.Count = 1;
			Shelf.Add(NewEntry);
			Changes |= EBusinessChange::Shelf;
		}
		else if (Shelf[ShelfIndex].Count < Input.Balance.MaxStockPerRecipe)
		{
			++Shelf[ShelfIndex].Count;
			Changes |= EBusinessChange::Shelf;
		}
		else
		{
			// ADR-0011: 진열대 상한이면 폐기.
			++Result.DiscardCount;
		}
	}
	return Changes;
}
