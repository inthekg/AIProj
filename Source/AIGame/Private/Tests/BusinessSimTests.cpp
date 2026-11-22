#include "Misc/AutomationTest.h"
#include "Business/BusinessSim.h"

// WITH_DEV_AUTOMATION_TESTS는 UBT가 정의하고 Shipping에서는 0이다 (UEBuildTarget.cs)
#if WITH_DEV_AUTOMATION_TESTS

// 입력값은 리터럴로 쓰고 설계 문서 · 데이터에서 가져온 값에는 출처를 단다. 기대값도 리터럴 — 계산식으로 쓰면 구현과 같은 식을 되풀이한다

// 하루 입력의 공통 부분. 입력: Data/Days.csv 1일차 · Steamer.csv 1레벨 · Balance.csv Default · Ingredients.csv 팥 행, 팥만 해금 · 찜기 1레벨(GDD 시작 조건), 외형 5종(GDD 손님)
// 이름을 모듈 안에서 겹치지 않게 둔다 — unity build에서 다른 테스트 파일과 한 번역 단위로 합쳐질 수 있다
static FBusinessDayInput MakeSimDayInput()
{
	FBusinessDayInput Input;
	Input.Day.Duration = 180.f;
	Input.Day.ArrivalMin = 5.5f;
	Input.Day.ArrivalMax = 8.5f;
	Input.Day.Patience = 25.f;
	Input.Day.MaxQueue = 3;
	Input.Day.AnyOrderRate = 0.10f;
	Input.Steamer.MaxGauge = 3;
	Input.Steamer.RegenTime = 6.f;
	Input.Balance.RiceCakeCost = 2;
	Input.Balance.NoFruitWeight = 8;
	Input.Balance.MixMultiplier = 1.25f;
	Input.Balance.PlaceTime = 0.2f;
	Input.Balance.WrapTime = 0.6f;
	Input.Balance.TrashTime = 0.5f;
	Input.Balance.MaxStockPerRecipe = 5;
	Input.Balance.BaseRent = 50;

	FIngredientRow RedBean;
	RedBean.Cost = 3;
	RedBean.Price = 10;
	RedBean.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::RedBean, RedBean);

	Input.GoldAtStart = 100;
	Input.CustomerLookCount = 5;
	Input.RandomSeed = 1;
	return Input;
}

// 하루를 이벤트마다 진행하며 대기열에 들어온 손님을 들어온 순서로 모은다. 새 손님은 대기열 끝에 들어간다(architecture.md 3.6 ⑤)
static TArray<FCustomer> CollectSimCustomers(const FBusinessDayInput& Input)
{
	TArray<FCustomer> Customers;
	int32 LastCollectedId = 0;
	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	for (TOptional<double> Next = Sim.GetNextEventTime(); Next.IsSet(); Next = Sim.GetNextEventTime())
	{
		Sim.ProcessDueEvents(Next.GetValue());
		const TArray<FCustomer>& Waiting = Sim.GetWaitingCustomers();
		if (!Waiting.IsEmpty() && Waiting.Last().Id != LastCollectedId)
		{
			Customers.Add(Waiting.Last());
			LastCollectedId = Waiting.Last().Id;
		}
	}
	return Customers;
}

// 베이스 → 과일(None이면 건너뜀) → 찜기를 StartTime · +1 · +2에 탭하고, 감싸기가 끝난 뒤인 +3에 ProcessDueEvents를 부른다. 그 변화 플래그를 돌려준다
// 1초 간격은 잠금(PlaceTime 0.2 · WrapTime 0.6)보다 길다. 찜기 게이지를 1 쓰므로 여러 개를 연달아 만드는 테스트는 MaxGauge를 늘린다
static EBusinessChange MakeSimProduct(FBusinessSim& Sim, EIngredient Base, EIngredient Fruit, double StartTime)
{
	Sim.TapIngredient(Base, StartTime);
	if (Fruit != EIngredient::None)
	{
		Sim.TapIngredient(Fruit, StartTime + 1.0);
	}
	Sim.TapSteamer(StartTime + 2.0);
	return Sim.ProcessDueEvents(StartTime + 3.0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDayEndAtDurationTest, "AIGame.BusinessSim.DayEnd.AtDuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDayEndAtDurationTest::RunTest(const FString& Parameters)
{
	// R33 (GDD 개요 · 화면)
	FBusinessSim Sim;
	const EBusinessChange StartChanges = Sim.StartDay(MakeSimDayInput(), 10.0);
	TestFalse(TEXT("StartDay: no DayEnded"), EnumHasAnyFlags(StartChanges, EBusinessChange::DayEnded));
	TestEqual(TEXT("Day end time = Now + Duration"), Sim.GetDayEndTime(), 190.0);

	const EBusinessChange BeforeEndChanges = Sim.ProcessDueEvents(189.5);
	TestFalse(TEXT("Before end: not over"), Sim.IsDayOver());
	TestFalse(TEXT("Before end: no DayEnded"), EnumHasAnyFlags(BeforeEndChanges, EBusinessChange::DayEnded));

	// 경계: 영업 종료 시각과 같은 Now에서 처리된다(Now 이하 — architecture.md 3.6 ProcessDueEvents)
	// 기대 플래그: 영역 5개(빈 화면을 그리는 델리게이트 5종, architecture.md 4.1) + DayEnded
	const EBusinessChange EndChanges = Sim.ProcessDueEvents(190.0);
	const EBusinessChange ExpectedEndChanges = EBusinessChange::Customers | EBusinessChange::Workbench
		| EBusinessChange::Steamer | EBusinessChange::Shelf | EBusinessChange::Gold | EBusinessChange::DayEnded;
	TestTrue(TEXT("At end: over"), Sim.IsDayOver());
	TestTrue(TEXT("At end: all areas + DayEnded"), EndChanges == ExpectedEndChanges);
	TestFalse(TEXT("At end: no next event"), Sim.GetNextEventTime().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimRentChargedAtStartTest, "AIGame.BusinessSim.Rent.ChargedAtStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimRentChargedAtStartTest::RunTest(const FString& Parameters)
{
	// R45 (ADR-0016 · ADR-0017)
	// 입력: ADR-0017 — 기본 50, 요거트 해금 +200 → 250
	FBusinessDayInput Input = MakeSimDayInput();
	FIngredientRow Yogurt;
	Yogurt.RentIncrease = 200;
	Input.Ingredients.Add(EIngredient::Yogurt, Yogurt);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	TestEqual(TEXT("Rent = BaseRent + RentIncrease"), Sim.GetResult().Rent, 250);
	// 경계: 임대료가 시작 골드보다 커서 영업 시작부터 빚(D17 — 현재 골드에 바로 반영)
	TestEqual(TEXT("Current gold right after start"), Sim.GetCurrentGold(), -150);

	// 다음 하루: 이전 하루의 임대료에 더해지지 않는다
	Sim.StartDay(Input, 200.0);
	TestEqual(TEXT("Restart: rent not accumulated"), Sim.GetResult().Rent, 250);
	TestEqual(TEXT("Restart: current gold"), Sim.GetCurrentGold(), -150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimRandomSameSeedSameDayTest, "AIGame.BusinessSim.Random.SameSeedSameDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimRandomSameSeedSameDayTest::RunTest(const FString& Parameters)
{
	// T3 (GDD 기술 포인트 3 · D3)
	// 하루를 이벤트마다 진행하며 처리한 이벤트 시각을 모은다
	const auto CollectEventTimes = [](FBusinessSim& Sim, const FBusinessDayInput& Input)
	{
		TArray<double> Times;
		Sim.StartDay(Input, 0.0);
		for (TOptional<double> Next = Sim.GetNextEventTime(); Next.IsSet(); Next = Sim.GetNextEventTime())
		{
			Times.Add(Next.GetValue());
			Sim.ProcessDueEvents(Next.GetValue());
		}
		return Times;
	};

	FBusinessDayInput Input = MakeSimDayInput();
	Input.RandomSeed = 7;
	FBusinessSim First;
	const TArray<double> FirstTimes = CollectEventTimes(First, Input);
	// 무작위로 정한 첫 도착(5.5~8.5초)과 영업 종료(180초)가 모두 있어야 무작위 값을 비교한 것이 된다
	TestTrue(TEXT("Has arrival before day end"), FirstTimes.Num() >= 2);
	TestEqual(TEXT("Last event = day end"), FirstTimes.Last(), 180.0);

	const TArray<double> RestartTimes = CollectEventTimes(First, Input);
	TestTrue(TEXT("Same object restarted: same day"), RestartTimes == FirstTimes);

	FBusinessSim Second;
	const TArray<double> SecondTimes = CollectEventTimes(Second, Input);
	TestTrue(TEXT("Another object: same day"), SecondTimes == FirstTimes);

	// 시드가 실제로 하루를 정하는지(시드를 무시하고 고정 초기값을 쓰는 구현을 걸러 낸다)
	FBusinessDayInput OtherSeedInput = Input;
	OtherSeedInput.RandomSeed = 8;
	FBusinessSim Third;
	const TArray<double> OtherSeedTimes = CollectEventTimes(Third, OtherSeedInput);
	TestTrue(TEXT("Different seed: different first arrival"), OtherSeedTimes[0] != FirstTimes[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimEventsSameTimeOrderTest, "AIGame.BusinessSim.Events.SameTimeOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimEventsSameTimeOrderTest::RunTest(const FString& Parameters)
{
	// T1 (GDD 기술 포인트 1 · D1). 같은 시각이면 넣은 순서대로(architecture.md 1.5)
	// 도착 간격은 FRandRange(a, a) = a로 고정한다(RandomStream.h FRandRange)

	// (A) 영업 종료와 같은 시각의 만료: DayEnd가 먼저 처리되어 힙을 비우므로 만료는 처리되지 않는다(architecture.md 4.2)
	// 입력: 도착 간격 60초, 인내심 120 → 손님1 도착 60 · 만료 180(= 영업 종료), 손님2 도착 120
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 60.f;
	Input.Day.ArrivalMax = 60.f;
	Input.Day.Patience = 120.f;

	FBusinessSim DayEndSim;
	DayEndSim.StartDay(Input, 0.0);
	DayEndSim.ProcessDueEvents(179.5);
	TestEqual(TEXT("A: two waiting before day end"), DayEndSim.GetWaitingCustomers().Num(), 2);

	DayEndSim.ProcessDueEvents(180.0);
	TestTrue(TEXT("A: day over"), DayEndSim.IsDayOver());
	TestEqual(TEXT("A: expire at day end time not processed"), DayEndSim.GetResult().MissedCustomers, 0);
	TestEqual(TEXT("A: queue cleared"), DayEndSim.GetWaitingCustomers().Num(), 0);

	// (B) 같은 시각의 도착과 만료: 다음 도착을 만료보다 먼저 넣으므로(architecture.md 3.6 ① → ⑤) 도착이 먼저 처리된다
	// 입력: 도착 간격 60초, 인내심 60, 대기열 1 → 120에 손님2 도착(손님1로 꽉 참 → 오지 않음) 뒤 손님1 만료
	Input.Day.Patience = 60.f;
	Input.Day.MaxQueue = 1;

	FBusinessSim ArriveSim;
	ArriveSim.StartDay(Input, 0.0);
	ArriveSim.ProcessDueEvents(120.0);
	TestEqual(TEXT("B: arrival first (no-show), then expire"), ArriveSim.GetWaitingCustomers().Num(), 0);
	TestEqual(TEXT("B: missed"), ArriveSim.GetResult().MissedCustomers, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerArrivalIntervalTest, "AIGame.BusinessSim.Customer.ArrivalInterval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerArrivalIntervalTest::RunTest(const FString& Parameters)
{
	// R11 (GDD 손님)
	// 인내심을 영업 시간보다 길게 둬 영업 종료 전 이벤트가 모두 도착이 되게 한다. 대기열(3명)이 찬 뒤에도 도착은 이어져야 한다(architecture.md 3.6 ① → ④)
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.Patience = 1000.f;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	TArray<double> ArrivalTimes;
	for (TOptional<double> Next = Sim.GetNextEventTime(); Next.IsSet() && Next.GetValue() < 180.0; Next = Sim.GetNextEventTime())
	{
		ArrivalTimes.Add(Next.GetValue());
		Sim.ProcessDueEvents(Next.GetValue());
	}
	TestEqual(TEXT("Queue capped at MaxQueue"), Sim.GetWaitingCustomers().Num(), 3);
	TestTrue(TEXT("Arrivals continue after queue full"), ArrivalTimes.Num() > 3);

	// 간격 기준: Days.csv 1일차 ArrivalMin 5.5 · ArrivalMax 8.5. 첫 간격은 StartDay 시각(0)부터
	double PrevTime = 0.0;
	for (const double& Time : ArrivalTimes)
	{
		const double Interval = Time - PrevTime;
		TestTrue(TEXT("Interval in [ArrivalMin, ArrivalMax]"),
			Interval >= 5.5 - UE_DOUBLE_KINDA_SMALL_NUMBER && Interval <= 8.5 + UE_DOUBLE_KINDA_SMALL_NUMBER);
		PrevTime = Time;
	}
	// 마지막 도착이 영업 종료(180) − ArrivalMax(8.5) 이후 = 영업 끝까지 도착이 끊기지 않았다
	TestTrue(TEXT("Last arrival within ArrivalMax of day end"), PrevTime >= 171.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerQueueFullNoShowTest, "AIGame.BusinessSim.Customer.QueueFullNoShow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerQueueFullNoShowTest::RunTest(const FString& Parameters)
{
	// R12 · D13 · D14 (GDD 손님, ADR-0015)
	// 입력: 도착 간격 10초 고정(FRandRange(a, a) = a), 대기열 2, 인내심 25 → 도착 10 · 20 · 30 · 40, 손님1 만료 35
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 10.f;
	Input.Day.ArrivalMax = 10.f;
	Input.Day.MaxQueue = 2;
	Input.Day.Patience = 25.f;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FCustomer>& Waiting = Sim.GetWaitingCustomers();

	// 한 번의 ProcessDueEvents가 10 · 20 · 30 도착을 이벤트 시각 순으로 처리한다
	Sim.ProcessDueEvents(30.0);
	if (TestEqual(TEXT("t=30: queue stays at MaxQueue"), Waiting.Num(), 2))
	{
		TestEqual(TEXT("t=30: slot 0"), Waiting[0].Id, 1);
		TestEqual(TEXT("t=30: slot 1"), Waiting[1].Id, 2);
	}
	TestEqual(TEXT("t=30: no-show is not a missed customer"), Sim.GetResult().MissedCustomers, 0);

	// 손님1이 35에 떠나 자리가 나면 40 도착은 들어온다. 오지 않은 손님은 ID를 받지 않는다(architecture.md 3.6 ⑤)
	Sim.ProcessDueEvents(40.0);
	if (TestEqual(TEXT("t=40: queue"), Waiting.Num(), 2))
	{
		TestEqual(TEXT("t=40: slot 0"), Waiting[0].Id, 2);
		TestEqual(TEXT("t=40: slot 1"), Waiting[1].Id, 3);
	}
	TestEqual(TEXT("t=40: only the expired customer is missed"), Sim.GetResult().MissedCustomers, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerOrderOnlyUnlockedTest, "AIGame.BusinessSim.Customer.OrderOnlyUnlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerOrderOnlyUnlockedTest::RunTest(const FString& Parameters)
{
	// R13 (GDD 손님)
	// 입력: 해금 = 팥 · 요거트(베이스) · 딸기 · 블루베리(과일). 블루베리는 해금됐지만 가중치 0(data-schema.md: OrderWeight ≥ 0)
	// 인내심 1초 — 다음 도착(5.5초 이상) 전에 떠나 모든 손님이 대기열에 들어온다. "아무거나" 0
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.Patience = 1.f;
	Input.Day.AnyOrderRate = 0.f;

	FIngredientRow Yogurt;
	Yogurt.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Yogurt, Yogurt);
	FIngredientRow Strawberry;
	Strawberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	Input.Ingredients.Add(EIngredient::Blueberry, FIngredientRow());

	TArray<FCustomer> Customers;
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		Input.RandomSeed = Seed;
		Customers.Append(CollectSimCustomers(Input));
	}

	int32 YogurtCount = 0;
	int32 StrawberryCount = 0;
	for (const FCustomer& Customer : Customers)
	{
		TestFalse(TEXT("Combo order only"), Customer.bAnyOrder);
		const bool bUnlockedBase = Customer.Order.Base == EIngredient::RedBean || Customer.Order.Base == EIngredient::Yogurt;
		TestTrue(TEXT("Base is an unlocked base"), bUnlockedBase);
		const bool bWeightedFruit = Customer.Order.Fruit == EIngredient::None || Customer.Order.Fruit == EIngredient::Strawberry;
		TestTrue(TEXT("Fruit is none or an unlocked fruit with weight > 0"), bWeightedFruit);
		if (Customer.Order.Base == EIngredient::Yogurt)
		{
			++YogurtCount;
		}
		if (Customer.Order.Fruit == EIngredient::Strawberry)
		{
			++StrawberryCount;
		}
	}
	// 위 검사로 베이스는 둘 중 하나이므로, 0 < 요거트 수 < 전체 = 두 베이스가 모두 나왔다. 과일도 같다
	TestTrue(TEXT("Both bases ordered"), YogurtCount > 0 && YogurtCount < Customers.Num());
	TestTrue(TEXT("Both no-fruit and strawberry ordered"), StrawberryCount > 0 && StrawberryCount < Customers.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerAnyOrderRateTest, "AIGame.BusinessSim.Customer.AnyOrderRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerAnyOrderRateTest::RunTest(const FString& Parameters)
{
	// R14 (GDD 손님), 극단값 0 · 1. FRand()는 [0, 1)이므로 둘 다 정확히 0% · 100%다(RandomStream.h RandHelper 주석)
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.Patience = 1.f;

	Input.Day.AnyOrderRate = 1.f;
	const TArray<FCustomer> RateOneCustomers = CollectSimCustomers(Input);
	TestTrue(TEXT("Rate 1: has customers"), RateOneCustomers.Num() > 0);
	for (const FCustomer& Customer : RateOneCustomers)
	{
		TestTrue(TEXT("Rate 1: any order"), Customer.bAnyOrder);
	}

	Input.Day.AnyOrderRate = 0.f;
	const TArray<FCustomer> RateZeroCustomers = CollectSimCustomers(Input);
	TestTrue(TEXT("Rate 0: has customers"), RateZeroCustomers.Num() > 0);
	for (const FCustomer& Customer : RateZeroCustomers)
	{
		TestFalse(TEXT("Rate 0: combo order"), Customer.bAnyOrder);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerLeavesAfterExpireTest, "AIGame.BusinessSim.Customer.LeavesAfterExpire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerLeavesAfterExpireTest::RunTest(const FString& Parameters)
{
	// R17 (GDD 손님)
	// 입력: 도착 간격 50초 고정, 인내심 20 → 손님1 도착 50 · 만료 70
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 50.f;
	Input.Day.ArrivalMax = 50.f;
	Input.Day.Patience = 20.f;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FCustomer>& Waiting = Sim.GetWaitingCustomers();

	Sim.ProcessDueEvents(50.0);
	if (TestEqual(TEXT("Arrived"), Waiting.Num(), 1))
	{
		// 위젯 인내심 게이지의 원본 시각(architecture.md 1.5)
		TestEqual(TEXT("ExpireTime = arrival + Patience"), Waiting[0].ExpireTime, 70.0);
	}

	Sim.ProcessDueEvents(69.5);
	TestEqual(TEXT("Before expire: still waiting"), Waiting.Num(), 1);
	TestEqual(TEXT("Before expire: not missed"), Sim.GetResult().MissedCustomers, 0);

	// 경계: 만료 시각과 같은 Now에서 떠난다(Now 이하 — architecture.md 3.6 ProcessDueEvents)
	const EBusinessChange ExpireChanges = Sim.ProcessDueEvents(70.0);
	TestTrue(TEXT("At expire: Customers changed"), EnumHasAnyFlags(ExpireChanges, EBusinessChange::Customers));
	TestEqual(TEXT("At expire: left queue"), Waiting.Num(), 0);
	TestEqual(TEXT("At expire: missed"), Sim.GetResult().MissedCustomers, 1);
	TestEqual(TEXT("At expire: no sales"), Sim.GetResult().Sales, 0);
	// 시작 골드 100 − 임대료 50, 떠난 손님은 골드를 바꾸지 않는다
	TestEqual(TEXT("At expire: gold unchanged"), Sim.GetCurrentGold(), 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerLookIndexInRangeTest, "AIGame.BusinessSim.Customer.LookIndexInRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerLookIndexInRangeTest::RunTest(const FString& Parameters)
{
	// R18 · D6 (GDD 손님, ADR-0009). 입력: 외형 5종(GDD 손님 — 실제 수는 BP 이미지 배열 길이, ADR-0014)
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.Patience = 1.f;

	TArray<FCustomer> Customers;
	for (int32 Seed = 1; Seed <= 5; ++Seed)
	{
		Input.RandomSeed = Seed;
		Customers.Append(CollectSimCustomers(Input));
	}

	TArray<int32> LookCounts;
	LookCounts.Init(0, 5);
	for (const FCustomer& Customer : Customers)
	{
		if (TestTrue(TEXT("Look index in [0, LookCount - 1]"), Customer.LookIndex >= 0 && Customer.LookIndex <= 4))
		{
			++LookCounts[Customer.LookIndex];
		}
	}
	for (const int32& Count : LookCounts)
	{
		TestTrue(TEXT("Every look appears"), Count > 0);
	}

	// 경계: 외형 1종(FBusinessDayInput 최소값)이면 항상 0
	Input.CustomerLookCount = 1;
	Input.RandomSeed = 1;
	const TArray<FCustomer> SingleLookCustomers = CollectSimCustomers(Input);
	TestTrue(TEXT("Single look: has customers"), SingleLookCustomers.Num() > 0);
	for (const FCustomer& Customer : SingleLookCustomers)
	{
		TestEqual(TEXT("Single look: always 0"), Customer.LookIndex, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimEventsZeroLockProcessedTest, "AIGame.BusinessSim.Events.ZeroLockProcessed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimEventsZeroLockProcessedTest::RunTest(const FString& Parameters)
{
	// T1 (GDD 기술 포인트 1 · ADR-0004). 잠금 시간이 0이면 WorkbenchDone 시각 = Now라 타이머 간격이 0 → 타이머로는 처리되지 않는다(TimerManager.cpp InternalSetTimer의 InRate > 0.f)
	// 입력 처리 뒤 같은 Now의 ProcessDueEvents가 처리해야 한다(architecture.md 3.6 ProcessDueEvents · 3.7 HandleChanges)
	// 입력: 잠금 시간 0. 첫 도착(5.5초 이상) 전인 t=1에 팥 → 팥(쓰레기 2번)
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Balance.PlaceTime = 0.f;
	Input.Balance.TrashTime = 0.f;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	Sim.TapIngredient(EIngredient::RedBean, 1.0);
	// 경계: 재료 올리기 잠금 0 — 같은 시각의 다음 입력을 받는다
	Sim.TapIngredient(EIngredient::RedBean, 1.0);
	TestTrue(TEXT("Trash"), Sim.GetWorkbenchState() == EWorkbenchState::Trash);
	const TOptional<double> NextEventTime = Sim.GetNextEventTime();
	TestTrue(TEXT("WorkbenchDone due at Now"), NextEventTime.IsSet() && NextEventTime.GetValue() == 1.0);

	const EBusinessChange DueChanges = Sim.ProcessDueEvents(1.0);
	TestTrue(TEXT("Same Now: Workbench changed"), EnumHasAnyFlags(DueChanges, EBusinessChange::Workbench));
	TestTrue(TEXT("Same Now: Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestTrue(TEXT("Same Now: recipe cleared"), Sim.GetWorkbenchRecipe() == FRecipe());

	// 비워진 작업대는 타이머를 기다리지 않고 바로 다음 입력을 받는다
	const EBusinessChange NextTapChanges = Sim.TapIngredient(EIngredient::RedBean, 1.0);
	TestTrue(TEXT("Next tap accepted"), NextTapChanges != EBusinessChange::None);
	TestTrue(TEXT("Next tap: Base"), Sim.GetWorkbenchState() == EWorkbenchState::Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimWorkbenchTransitionTableTest, "AIGame.BusinessSim.Workbench.TransitionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimWorkbenchTransitionTableTest::RunTest(const FString& Parameters)
{
	// R1 · R19 · R21 · T2 (GDD 재료와 조합 · 제작 · 기술 포인트 2)
	// 기대값: architecture.md 3.6 작업대 전이표, 괄호 = GDD 쓰레기 표 번호. 결과가 현재 상태와 같으면 무시된 입력이다
	TestTrue(TEXT("Empty + Base -> Base"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Empty, EWorkbenchInput::Base) == EWorkbenchState::Base);
	TestTrue(TEXT("Empty + Fruit -> Trash (1)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Empty, EWorkbenchInput::Fruit) == EWorkbenchState::Trash);
	TestTrue(TEXT("Empty + Steamer -> Trash (4)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Empty, EWorkbenchInput::Steamer) == EWorkbenchState::Trash);
	TestTrue(TEXT("Empty + Discard -> ignored (R21)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Empty, EWorkbenchInput::Discard) == EWorkbenchState::Empty);

	TestTrue(TEXT("Base + Base -> Trash (2)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Base, EWorkbenchInput::Base) == EWorkbenchState::Trash);
	TestTrue(TEXT("Base + Fruit -> BaseFruit"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Base, EWorkbenchInput::Fruit) == EWorkbenchState::BaseFruit);
	TestTrue(TEXT("Base + Steamer -> Wrapping"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Base, EWorkbenchInput::Steamer) == EWorkbenchState::Wrapping);
	TestTrue(TEXT("Base + Discard -> Trash"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Base, EWorkbenchInput::Discard) == EWorkbenchState::Trash);

	TestTrue(TEXT("BaseFruit + Base -> Trash (5)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::BaseFruit, EWorkbenchInput::Base) == EWorkbenchState::Trash);
	TestTrue(TEXT("BaseFruit + Fruit -> Trash (3)"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::BaseFruit, EWorkbenchInput::Fruit) == EWorkbenchState::Trash);
	TestTrue(TEXT("BaseFruit + Steamer -> Wrapping"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::BaseFruit, EWorkbenchInput::Steamer) == EWorkbenchState::Wrapping);
	TestTrue(TEXT("BaseFruit + Discard -> Trash"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::BaseFruit, EWorkbenchInput::Discard) == EWorkbenchState::Trash);

	TestTrue(TEXT("Wrapping + Base -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Wrapping, EWorkbenchInput::Base) == EWorkbenchState::Wrapping);
	TestTrue(TEXT("Wrapping + Fruit -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Wrapping, EWorkbenchInput::Fruit) == EWorkbenchState::Wrapping);
	TestTrue(TEXT("Wrapping + Steamer -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Wrapping, EWorkbenchInput::Steamer) == EWorkbenchState::Wrapping);
	TestTrue(TEXT("Wrapping + Discard -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Wrapping, EWorkbenchInput::Discard) == EWorkbenchState::Wrapping);

	TestTrue(TEXT("Trash + Base -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Trash, EWorkbenchInput::Base) == EWorkbenchState::Trash);
	TestTrue(TEXT("Trash + Fruit -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Trash, EWorkbenchInput::Fruit) == EWorkbenchState::Trash);
	TestTrue(TEXT("Trash + Steamer -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Trash, EWorkbenchInput::Steamer) == EWorkbenchState::Trash);
	TestTrue(TEXT("Trash + Discard -> ignored"),
		FBusinessSim::NextWorkbenchState(EWorkbenchState::Trash, EWorkbenchInput::Discard) == EWorkbenchState::Trash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimWorkbenchTrashCountTest, "AIGame.BusinessSim.Workbench.TrashCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimWorkbenchTrashCountTest::RunTest(const FString& Parameters)
{
	// R20 (GDD 제작). 쓰레기 수는 판정 순간에 센다(architecture.md 3.6 전이표 메모). 4번(빈 작업대 + 찜기)은 T12 Steamer.GaugeConsumed
	// 입력: 딸기 해금(Ingredients.csv 딸기 행 Cost 5). 탭 간격 1초 — 잠금(PlaceTime 0.2 · TrashTime 0.5)보다 길다
	FBusinessDayInput Input = MakeSimDayInput();
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);

	// (1) 빈 작업대 + 과일
	Sim.TapIngredient(EIngredient::Strawberry, 1.0);
	TestEqual(TEXT("(1) Empty + fruit: counted at judgment"), Sim.GetResult().TrashCount, 1);
	Sim.ProcessDueEvents(2.0);
	TestTrue(TEXT("(1) WorkbenchDone: Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestTrue(TEXT("(1) WorkbenchDone: recipe cleared"), Sim.GetWorkbenchRecipe() == FRecipe());

	// (2) 베이스 + 베이스
	Sim.TapIngredient(EIngredient::RedBean, 3.0);
	Sim.TapIngredient(EIngredient::RedBean, 4.0);
	TestEqual(TEXT("(2) Base + base"), Sim.GetResult().TrashCount, 2);
	Sim.ProcessDueEvents(5.0);

	// (3) 베이스+과일 + 과일
	Sim.TapIngredient(EIngredient::RedBean, 6.0);
	Sim.TapIngredient(EIngredient::Strawberry, 7.0);
	Sim.TapIngredient(EIngredient::Strawberry, 8.0);
	TestEqual(TEXT("(3) BaseFruit + fruit"), Sim.GetResult().TrashCount, 3);
	Sim.ProcessDueEvents(9.0);

	// (5) 베이스+과일 + 베이스
	Sim.TapIngredient(EIngredient::RedBean, 10.0);
	Sim.TapIngredient(EIngredient::Strawberry, 11.0);
	Sim.TapIngredient(EIngredient::RedBean, 12.0);
	TestEqual(TEXT("(5) BaseFruit + base"), Sim.GetResult().TrashCount, 4);
	Sim.ProcessDueEvents(13.0);

	// 버리기는 재료비를 더하지 않는다(GDD 버리기 버튼). 여기까지 재료비: 5 + 6 + 13 + 11 + 3 = 38
	Sim.TapIngredient(EIngredient::RedBean, 14.0);
	Sim.TapDiscard(15.0);
	TestEqual(TEXT("Discard on Base: counted"), Sim.GetResult().TrashCount, 5);
	TestEqual(TEXT("Discard on Base: no cost"), Sim.GetResult().IngredientCost, 38);
	Sim.ProcessDueEvents(16.0);

	// 38 + 3 + 5 = 46
	Sim.TapIngredient(EIngredient::RedBean, 17.0);
	Sim.TapIngredient(EIngredient::Strawberry, 18.0);
	Sim.TapDiscard(19.0);
	TestEqual(TEXT("Discard on BaseFruit: counted"), Sim.GetResult().TrashCount, 6);
	TestEqual(TEXT("Discard on BaseFruit: no cost"), Sim.GetResult().IngredientCost, 46);
	Sim.ProcessDueEvents(20.0);

	// 빈 작업대 버리기는 무시(R21)
	const EBusinessChange EmptyDiscardChanges = Sim.TapDiscard(21.0);
	TestTrue(TEXT("Discard on Empty: ignored"), EmptyDiscardChanges == EBusinessChange::None);
	TestEqual(TEXT("Discard on Empty: not counted"), Sim.GetResult().TrashCount, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimWorkbenchInputLockedTest, "AIGame.BusinessSim.Workbench.InputLocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimWorkbenchInputLockedTest::RunTest(const FString& Parameters)
{
	// R22 (GDD 제작 입력 잠금). 시나리오: architecture.md 4.4. 무시된 입력은 재료비도 내지 않는다
	// 끝에 영업 종료 뒤(architecture.md 4.2) · 미해금 재료 · None(architecture.md 3.6 TapIngredient) 무시도 본다(T11 사용자 결정 Q4)
	// 입력: 딸기 해금(Ingredients.csv Cost 5). 잠금 끝 경계는 TrashTime 0.5로 본다 — 이진수로 정확해 10.0 + 0.5 = 10.5가 그대로 나온다(PlaceTime 0.2f는 double로 바꾸면 오차)
	FBusinessDayInput Input = MakeSimDayInput();
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);

	// t=10 빈 작업대 + 딸기 → 쓰레기, 10.5까지 잠금. 시작 골드 100 − 임대료 50 − 딸기 5
	Sim.TapIngredient(EIngredient::Strawberry, 10.0);
	TestEqual(TEXT("t=10: gold after trash tap"), Sim.GetCurrentGold(), 45);

	const EBusinessChange TrashLockTapChanges = Sim.TapIngredient(EIngredient::RedBean, 10.2);
	TestTrue(TEXT("t=10.2 ingredient during trash lock: ignored"), TrashLockTapChanges == EBusinessChange::None);
	const EBusinessChange TrashLockDiscardChanges = Sim.TapDiscard(10.2);
	TestTrue(TEXT("t=10.2 discard during trash lock: ignored"), TrashLockDiscardChanges == EBusinessChange::None);
	TestTrue(TEXT("t=10.2: still Trash"), Sim.GetWorkbenchState() == EWorkbenchState::Trash);
	TestEqual(TEXT("t=10.2: no cost"), Sim.GetCurrentGold(), 45);

	// 경계: 잠금 끝(10.5)과 같은 시각. 잠금은 '이 시각 전'까지다(architecture.md 3.6 InputLockEndTime)
	Sim.ProcessDueEvents(10.5);
	const EBusinessChange BoundaryChanges = Sim.TapIngredient(EIngredient::RedBean, 10.5);
	TestTrue(TEXT("t=10.5 at lock end: accepted"), BoundaryChanges != EBusinessChange::None);
	TestTrue(TEXT("t=10.5: Base"), Sim.GetWorkbenchState() == EWorkbenchState::Base);

	// 재료 올리기 잠금(PlaceTime 0.2) 중. 45 − 팥 3
	const EBusinessChange PlaceLockDiscardChanges = Sim.TapDiscard(10.6);
	TestTrue(TEXT("t=10.6 discard during place lock: ignored"), PlaceLockDiscardChanges == EBusinessChange::None);
	const EBusinessChange PlaceLockTapChanges = Sim.TapIngredient(EIngredient::Strawberry, 10.6);
	TestTrue(TEXT("t=10.6 ingredient during place lock: ignored"), PlaceLockTapChanges == EBusinessChange::None);
	TestTrue(TEXT("t=10.6: still Base"), Sim.GetWorkbenchState() == EWorkbenchState::Base);
	TestEqual(TEXT("t=10.6: no cost"), Sim.GetCurrentGold(), 42);

	const EBusinessChange AfterLockDiscardChanges = Sim.TapDiscard(11.5);
	TestTrue(TEXT("t=11.5 discard after place lock: accepted"), AfterLockDiscardChanges != EBusinessChange::None);
	TestTrue(TEXT("t=11.5: Trash"), Sim.GetWorkbenchState() == EWorkbenchState::Trash);

	// 미해금 재료 · None: 무시. None이 IsBaseIngredient에 닿으면 checkNoEntry로 멈춘다
	Sim.ProcessDueEvents(12.0);
	const EBusinessChange LockedIngredientChanges = Sim.TapIngredient(EIngredient::Mango, 13.0);
	TestTrue(TEXT("Not unlocked: ignored"), LockedIngredientChanges == EBusinessChange::None);
	const EBusinessChange NoneIngredientChanges = Sim.TapIngredient(EIngredient::None, 13.0);
	TestTrue(TEXT("None: ignored"), NoneIngredientChanges == EBusinessChange::None);
	TestTrue(TEXT("Not unlocked · None: still Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestEqual(TEXT("Not unlocked · None: no cost"), Sim.GetCurrentGold(), 42);

	// 영업 종료 뒤: 무시
	Sim.ProcessDueEvents(180.0);
	TestTrue(TEXT("Day over"), Sim.IsDayOver());
	const EBusinessChange AfterDayTapChanges = Sim.TapIngredient(EIngredient::RedBean, 181.0);
	TestTrue(TEXT("After day end: ingredient ignored"), AfterDayTapChanges == EBusinessChange::None);
	const EBusinessChange AfterDayDiscardChanges = Sim.TapDiscard(181.0);
	TestTrue(TEXT("After day end: discard ignored"), AfterDayDiscardChanges == EBusinessChange::None);
	TestTrue(TEXT("After day end: Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestEqual(TEXT("After day end: no cost"), Sim.GetCurrentGold(), 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCostChargedOnTapTest, "AIGame.BusinessSim.Cost.ChargedOnTap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCostChargedOnTapTest::RunTest(const FString& Parameters)
{
	// R5 (GDD 경제). 쓰레기가 되는 탭도 낸다(architecture.md 3.6 전이표 메모). 떡 재료비는 T12 TapSteamer
	// 입력: 팥 Cost 3 · 딸기 Cost 5(Ingredients.csv), 시작 골드 100 · 임대료 50
	FBusinessDayInput Input = MakeSimDayInput();
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);

	const EBusinessChange BaseChanges = Sim.TapIngredient(EIngredient::RedBean, 1.0);
	TestTrue(TEXT("Base tap: Workbench changed"), EnumHasAnyFlags(BaseChanges, EBusinessChange::Workbench));
	TestTrue(TEXT("Base tap: Gold changed"), EnumHasAnyFlags(BaseChanges, EBusinessChange::Gold));
	TestEqual(TEXT("Base tap: cost"), Sim.GetResult().IngredientCost, 3);
	TestEqual(TEXT("Base tap: current gold"), Sim.GetCurrentGold(), 47);

	const EBusinessChange FruitChanges = Sim.TapIngredient(EIngredient::Strawberry, 2.0);
	TestTrue(TEXT("Fruit tap: Gold changed"), EnumHasAnyFlags(FruitChanges, EBusinessChange::Gold));
	TestEqual(TEXT("Fruit tap: cost"), Sim.GetResult().IngredientCost, 8);
	TestEqual(TEXT("Fruit tap: current gold"), Sim.GetCurrentGold(), 42);
	// 배분(T13)이 완성품 조합으로 쓴다
	TestTrue(TEXT("Recipe = RedBean + Strawberry"),
		Sim.GetWorkbenchRecipe().Base == EIngredient::RedBean && Sim.GetWorkbenchRecipe().Fruit == EIngredient::Strawberry);

	// 베이스+과일 + 과일 → 쓰레기(3번)여도 낸다
	const EBusinessChange TrashChanges = Sim.TapIngredient(EIngredient::Strawberry, 3.0);
	TestTrue(TEXT("Trash tap: Gold changed"), EnumHasAnyFlags(TrashChanges, EBusinessChange::Gold));
	TestEqual(TEXT("Trash tap: cost"), Sim.GetResult().IngredientCost, 13);

	// 버리기는 재료비가 없다. 13 + 팥 3 = 16
	Sim.ProcessDueEvents(4.0);
	Sim.TapIngredient(EIngredient::RedBean, 5.0);
	const EBusinessChange DiscardChanges = Sim.TapDiscard(6.0);
	TestTrue(TEXT("Discard: Workbench changed"), EnumHasAnyFlags(DiscardChanges, EBusinessChange::Workbench));
	TestFalse(TEXT("Discard: Gold not changed"), EnumHasAnyFlags(DiscardChanges, EBusinessChange::Gold));
	TestEqual(TEXT("Discard: cost unchanged"), Sim.GetResult().IngredientCost, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCostAllowsDebtTest, "AIGame.BusinessSim.Cost.AllowsDebt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCostAllowsDebtTest::RunTest(const FString& Parameters)
{
	// R6 (GDD 경제). 재료 탭은 골드를 검사하지 않는다
	// 입력: 시작 골드 52 → 임대료 50 뒤 2, 팥 Cost 3
	FBusinessDayInput Input = MakeSimDayInput();
	Input.GoldAtStart = 52;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const EBusinessChange DebtChanges = Sim.TapIngredient(EIngredient::RedBean, 1.0);
	TestTrue(TEXT("Gold < cost: accepted"), DebtChanges != EBusinessChange::None);
	TestTrue(TEXT("Gold < cost: Base"), Sim.GetWorkbenchState() == EWorkbenchState::Base);
	TestEqual(TEXT("Gold < cost: debt"), Sim.GetCurrentGold(), -1);

	// 빚인 채로도 받는다(베이스 + 베이스 → 쓰레기 2번)
	const EBusinessChange InDebtChanges = Sim.TapIngredient(EIngredient::RedBean, 2.0);
	TestTrue(TEXT("In debt: accepted"), InDebtChanges != EBusinessChange::None);
	TestEqual(TEXT("In debt: debt grows"), Sim.GetCurrentGold(), -4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimSteamerGaugeConsumedTest, "AIGame.BusinessSim.Steamer.GaugeConsumed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimSteamerGaugeConsumedTest::RunTest(const FString& Parameters)
{
	// R25 (GDD 찜기). 쓰레기 4번도 떡 1개를 쓰고 떡값을 낸다(architecture.md 3.6 전이표 메모)
	// 입력: Steamer.csv 1레벨 MaxGauge 3, Balance.csv RiceCakeCost 2 · WrapTime 0.6, 팥 Cost 3, 시작 골드 100 · 임대료 50
	FBusinessSim Sim;
	Sim.StartDay(MakeSimDayInput(), 0.0);
	// 하루는 가득 찬 게이지로 시작한다(T12 사용자 결정 Q1)
	TestEqual(TEXT("Start: full gauge"), Sim.GetSteamerGauge(), 3);
	TestEqual(TEXT("Start: not refilling"), Sim.GetSteamerRefillEndTime(), 0.0);

	// t=1 빈 작업대 + 찜기 → 쓰레기(4번)
	const EBusinessChange ExpectedTapChanges = EBusinessChange::Workbench | EBusinessChange::Steamer | EBusinessChange::Gold;
	const EBusinessChange TrashChanges = Sim.TapSteamer(1.0);
	TestTrue(TEXT("(4) Empty + steamer: Workbench | Steamer | Gold"), TrashChanges == ExpectedTapChanges);
	TestTrue(TEXT("(4) Trash"), Sim.GetWorkbenchState() == EWorkbenchState::Trash);
	TestEqual(TEXT("(4) gauge - 1"), Sim.GetSteamerGauge(), 2);
	TestEqual(TEXT("(4) counted as trash"), Sim.GetResult().TrashCount, 1);
	TestEqual(TEXT("(4) rice cake cost"), Sim.GetResult().IngredientCost, 2);
	TestEqual(TEXT("(4) current gold"), Sim.GetCurrentGold(), 48);
	Sim.ProcessDueEvents(2.0);

	// t=3 팥, t=4 찜기 → 감싸기. 재료비 2 + 3 + 2 = 7
	Sim.TapIngredient(EIngredient::RedBean, 3.0);
	const EBusinessChange WrapChanges = Sim.TapSteamer(4.0);
	TestTrue(TEXT("Base + steamer: Workbench | Steamer | Gold"), WrapChanges == ExpectedTapChanges);
	TestTrue(TEXT("Base + steamer: Wrapping"), Sim.GetWorkbenchState() == EWorkbenchState::Wrapping);
	TestEqual(TEXT("Base + steamer: gauge - 1"), Sim.GetSteamerGauge(), 1);
	TestEqual(TEXT("Base + steamer: cost"), Sim.GetResult().IngredientCost, 7);
	TestEqual(TEXT("Base + steamer: current gold"), Sim.GetCurrentGold(), 43);
	TestEqual(TEXT("Base + steamer: not trash"), Sim.GetResult().TrashCount, 1);

	// 감싸기 잠금(4.0 + WrapTime 0.6) 중의 찜기 탭은 무시되고 떡을 쓰지 않는다(R22)
	const EBusinessChange WrapLockChanges = Sim.TapSteamer(4.3);
	TestTrue(TEXT("t=4.3 during wrap lock: ignored"), WrapLockChanges == EBusinessChange::None);
	TestEqual(TEXT("t=4.3: gauge unchanged"), Sim.GetSteamerGauge(), 1);
	TestEqual(TEXT("t=4.3: cost unchanged"), Sim.GetResult().IngredientCost, 7);

	// 감싸기가 끝나면(WorkbenchDone) 작업대가 빈다
	Sim.ProcessDueEvents(5.0);
	TestTrue(TEXT("Wrap done: Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);

	// 영업 종료 뒤: 무시(architecture.md 4.2)
	Sim.ProcessDueEvents(180.0);
	const EBusinessChange AfterDayChanges = Sim.TapSteamer(181.0);
	TestTrue(TEXT("After day end: ignored"), AfterDayChanges == EBusinessChange::None);
	TestEqual(TEXT("After day end: gauge unchanged"), Sim.GetSteamerGauge(), 1);
	TestEqual(TEXT("After day end: cost unchanged"), Sim.GetResult().IngredientCost, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimSteamerRefillTest, "AIGame.BusinessSim.Steamer.Refill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimSteamerRefillTest::RunTest(const FString& Parameters)
{
	// R26 (GDD 찜기). 게이지 0이면 작업대에 베이스가 있어도 무시한다(architecture.md 4.4)
	// 입력: Steamer.csv 1레벨 MaxGauge 3 · RegenTime 6 — 이진수로 정확해 3.0 + 6.0 = 9.0 경계가 그대로 나온다
	// 게이지는 빈 작업대 + 찜기(쓰레기 4번)로 쓴다. 탭 간격 1초 — 쓰레기 잠금(TrashTime 0.5)보다 길다
	FBusinessSim Sim;
	Sim.StartDay(MakeSimDayInput(), 0.0);

	Sim.TapSteamer(1.0);
	Sim.ProcessDueEvents(2.0);
	Sim.TapSteamer(2.0);
	Sim.ProcessDueEvents(3.0);
	const EBusinessChange LastRiceCakeChanges = Sim.TapSteamer(3.0);
	TestTrue(TEXT("t=3 last rice cake: Steamer changed"), EnumHasAnyFlags(LastRiceCakeChanges, EBusinessChange::Steamer));
	TestEqual(TEXT("t=3: gauge 0"), Sim.GetSteamerGauge(), 0);
	TestEqual(TEXT("t=3: refill end = Now + RegenTime"), Sim.GetSteamerRefillEndTime(), 9.0);

	// 다시 찌는 중: 빈 작업대 탭도 무시한다 — 쓰레기 · 떡값 없음. 재료비 2 × 3 = 6
	Sim.ProcessDueEvents(4.0);
	const EBusinessChange RefillingEmptyChanges = Sim.TapSteamer(4.0);
	TestTrue(TEXT("t=4 refilling, Empty: ignored"), RefillingEmptyChanges == EBusinessChange::None);
	TestTrue(TEXT("t=4: still Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestEqual(TEXT("t=4: not counted as trash"), Sim.GetResult().TrashCount, 3);
	TestEqual(TEXT("t=4: no cost"), Sim.GetResult().IngredientCost, 6);

	// 다시 찌는 중: 베이스가 있어도 무시한다. 6 + 팥 3 = 9
	Sim.TapIngredient(EIngredient::RedBean, 5.0);
	const EBusinessChange RefillingBaseChanges = Sim.TapSteamer(6.0);
	TestTrue(TEXT("t=6 refilling, Base: ignored"), RefillingBaseChanges == EBusinessChange::None);
	TestTrue(TEXT("t=6: still Base"), Sim.GetWorkbenchState() == EWorkbenchState::Base);
	TestEqual(TEXT("t=6: no cost"), Sim.GetResult().IngredientCost, 9);

	Sim.ProcessDueEvents(8.5);
	TestEqual(TEXT("t=8.5 before refill end: gauge 0"), Sim.GetSteamerGauge(), 0);

	// 경계: 다시 찌기 끝 시각과 같은 Now에서 찬다(Now 이하 — architecture.md 3.6 ProcessDueEvents)
	const EBusinessChange RefillChanges = Sim.ProcessDueEvents(9.0);
	TestTrue(TEXT("t=9 refilled: Steamer changed"), EnumHasAnyFlags(RefillChanges, EBusinessChange::Steamer));
	TestEqual(TEXT("t=9: gauge = MaxGauge"), Sim.GetSteamerGauge(), 3);
	TestEqual(TEXT("t=9: not refilling"), Sim.GetSteamerRefillEndTime(), 0.0);

	const EBusinessChange AfterRefillChanges = Sim.TapSteamer(9.0);
	TestTrue(TEXT("t=9 after refill: accepted"), AfterRefillChanges != EBusinessChange::None);
	TestTrue(TEXT("t=9: Wrapping"), Sim.GetWorkbenchState() == EWorkbenchState::Wrapping);
	TestEqual(TEXT("t=9: gauge - 1"), Sim.GetSteamerGauge(), 2);

	// 다시 찌는 중에 영업이 끝나면 다시 찌기도 사라진다(T12 사용자 결정 Q2 — architecture.md 3.6 DayEnd)
	// 입력: t=175 · 176에 게이지 2 → 0, 다시 찌기 끝 182 > 영업 종료 180
	Sim.ProcessDueEvents(175.0);
	Sim.TapSteamer(175.0);
	Sim.ProcessDueEvents(176.0);
	Sim.TapSteamer(176.0);
	TestEqual(TEXT("t=176: refilling until 182"), Sim.GetSteamerRefillEndTime(), 182.0);
	Sim.ProcessDueEvents(180.0);
	TestTrue(TEXT("Day over"), Sim.IsDayOver());
	TestEqual(TEXT("Day end: not refilling"), Sim.GetSteamerRefillEndTime(), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerServedNotExpiredTest, "AIGame.BusinessSim.Customer.ServedNotExpired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerServedNotExpiredTest::RunTest(const FString& Parameters)
{
	// R17 · D2 (GDD 손님, ADR-0004). 판매된 손님의 만료 이벤트는 힙에 남았다가 꺼낼 때 버려진다(지연 삭제 — architecture.md 4.1 '알림 없음')
	// 입력: 도착 간격 50초 고정, 인내심 25(Days.csv 1일차), "아무거나" 0 → 손님1 도착 50 · 주문 팥 · 만료 75, 다음 도착 100
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 50.f;
	Input.Day.ArrivalMax = 50.f;
	Input.Day.AnyOrderRate = 0.f;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	Sim.ProcessDueEvents(50.0);
	TestEqual(TEXT("Arrived"), Sim.GetWaitingCustomers().Num(), 1);

	// 팥 완성(53.6) → 손님1에게 판매. 팥 판매가 10(Ingredients.csv)
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 51.0);
	TestEqual(TEXT("Served: left queue"), Sim.GetWaitingCustomers().Num(), 0);
	TestEqual(TEXT("Served: sales"), Sim.GetResult().Sales, 10);
	TestEqual(TEXT("Served: sold"), Sim.GetResult().SoldCount, 1);
	const TOptional<double> NextEventTime = Sim.GetNextEventTime();
	TestTrue(TEXT("Served: expire event still in heap"), NextEventTime.IsSet() && NextEventTime.GetValue() == 75.0);

	// 경계: 만료 시각과 같은 Now. 만료 이벤트를 꺼내지만 그 손님이 대기열에 없으므로 버린다
	const EBusinessChange ExpireChanges = Sim.ProcessDueEvents(75.0);
	TestTrue(TEXT("Expire of served customer: no change"), ExpireChanges == EBusinessChange::None);
	TestEqual(TEXT("Expire of served customer: not missed"), Sim.GetResult().MissedCustomers, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDistributeFirstMatchingTest, "AIGame.BusinessSim.Distribute.FirstMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDistributeFirstMatchingTest::RunTest(const FString& Parameters)
{
	// R23 · D3 (GDD 제작 완성품 배분 · 기술 포인트 3, ADR-0006). 도착 순 대기열을 앞에서부터 훑어 그 조합 또는 "아무거나"인 첫 손님에게 판다
	// 입력: 딸기 해금(Ingredients.csv Cost 5 · Price 12 · OrderWeight 10), 과일 없음 가중치 0 → 조합 주문은 모두 팥+딸기. "아무거나" 0.5, 도착 간격 10초 고정, 인내심 1000(영업 중 떠나지 않음)
	// 시드 10: 손님 1 · 2 · 3 = 아무거나 · 팥+딸기 · 아무거나. RandomStream.h 알고리즘을 재현해 고른 장면이라 먼저 확인한다
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 10.f;
	Input.Day.ArrivalMax = 10.f;
	Input.Day.Patience = 1000.f;
	Input.Day.AnyOrderRate = 0.5f;
	Input.Balance.NoFruitWeight = 0;
	Input.RandomSeed = 10;
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Strawberry.Price = 12;
	Strawberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FCustomer>& Waiting = Sim.GetWaitingCustomers();
	Sim.ProcessDueEvents(30.0);
	const bool bScene = Waiting.Num() == 3 && Waiting[0].bAnyOrder && !Waiting[1].bAnyOrder
		&& Waiting[1].Order.Fruit == EIngredient::Strawberry && Waiting[2].bAnyOrder;
	if (!TestTrue(TEXT("Scene: [any, RedBean+Strawberry, any]"), bScene))
	{
		return false;
	}

	// 팥+딸기 완성(32.6): 1번(아무거나) · 2번(팥+딸기) 모두 맞다 → 먼저 온 1번. 판매가 (10 + 12) × 1.25 = 27.5 → 28
	const EBusinessChange FirstChanges = MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 30.0);
	TestTrue(TEXT("1st: Customers changed"), EnumHasAnyFlags(FirstChanges, EBusinessChange::Customers));
	TestTrue(TEXT("1st: Gold changed"), EnumHasAnyFlags(FirstChanges, EBusinessChange::Gold));
	TestFalse(TEXT("1st: not shelved"), EnumHasAnyFlags(FirstChanges, EBusinessChange::Shelf));
	if (TestEqual(TEXT("1st: queue"), Waiting.Num(), 2))
	{
		TestEqual(TEXT("1st: slot 0"), Waiting[0].Id, 2);
		TestEqual(TEXT("1st: slot 1"), Waiting[1].Id, 3);
	}
	TestEqual(TEXT("1st: sales"), Sim.GetResult().Sales, 28);

	// 팥 완성(35.6): 2번(팥+딸기)은 맞지 않아 건너뛰고 3번(아무거나). 28 + 10 = 38
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 33.0);
	if (TestEqual(TEXT("2nd: queue"), Waiting.Num(), 1))
	{
		TestEqual(TEXT("2nd: skipped customer stays"), Waiting[0].Id, 2);
	}
	TestEqual(TEXT("2nd: sales"), Sim.GetResult().Sales, 38);

	// 팥+딸기 완성(38.6): 2번. 38 + 28 = 66. 다음 도착은 40이라 아직 없다
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 36.0);
	TestEqual(TEXT("3rd: queue empty"), Waiting.Num(), 0);
	TestEqual(TEXT("3rd: sales"), Sim.GetResult().Sales, 66);
	TestEqual(TEXT("Sold count"), Sim.GetResult().SoldCount, 3);
	TestEqual(TEXT("Nothing shelved"), Sim.GetShelf().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDistributeToShelfTest, "AIGame.BusinessSim.Distribute.ToShelf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDistributeToShelfTest::RunTest(const FString& Parameters)
{
	// R23 ② · D8 · D9 (GDD 제작 완성품 배분, ADR-0011 · ADR-0012). 맞는 손님이 기다리지 않으면 진열대로
	// 입력: 딸기(Cost 5 · Price 12, 가중치 0) · 블루베리(Cost 9 · Price 15, 가중치 10) 해금(Ingredients.csv), 과일 없음 0 → 손님은 모두 팥+블루베리
	// "아무거나" 0, 도착 간격 20초 고정, 인내심 1000. 베이스는 모두 팥이므로 과일로 칸을 구분한다
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 20.f;
	Input.Day.ArrivalMax = 20.f;
	Input.Day.Patience = 1000.f;
	Input.Day.AnyOrderRate = 0.f;
	Input.Balance.NoFruitWeight = 0;
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Strawberry.Price = 12;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	FIngredientRow Blueberry;
	Blueberry.Cost = 9;
	Blueberry.Price = 15;
	Blueberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Blueberry, Blueberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FShelfEntry>& Shelf = Sim.GetShelf();

	// 첫 도착(20) 전: 기다리는 손님 없음 → 진열대. 매출은 없어 작업대 · 진열대만 바뀐다
	const EBusinessChange FirstChanges = MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0);
	TestTrue(TEXT("No customer: Workbench | Shelf"), FirstChanges == (EBusinessChange::Workbench | EBusinessChange::Shelf));
	if (TestEqual(TEXT("No customer: one slot"), Shelf.Num(), 1))
	{
		TestTrue(TEXT("No customer: slot 0 = RedBean"), Shelf[0].Recipe.Fruit == EIngredient::None);
		TestEqual(TEXT("No customer: slot 0 count"), Shelf[0].Count, 1);
	}
	TestEqual(TEXT("No customer: no sales"), Sim.GetResult().Sales, 0);

	// 새 조합은 맨 뒤 칸에 붙고, 이미 있는 조합은 그 칸의 개수만 는다
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 4.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 7.0);
	if (TestEqual(TEXT("Same recipe: same slot"), Shelf.Num(), 2))
	{
		TestTrue(TEXT("Same recipe: slot 0 = RedBean"), Shelf[0].Recipe.Fruit == EIngredient::None);
		TestEqual(TEXT("Same recipe: slot 0 count"), Shelf[0].Count, 2);
		TestTrue(TEXT("New recipe: slot 1 = RedBean+Strawberry"), Shelf[1].Recipe.Fruit == EIngredient::Strawberry);
		TestEqual(TEXT("New recipe: slot 1 count"), Shelf[1].Count, 1);
	}

	// t=20 손님1(팥+블루베리): 진열대에 팥+블루베리가 없어 기다린다. 주문과 다른 팥은 손님에게 가지 않고 진열대로
	Sim.ProcessDueEvents(20.0);
	TestEqual(TEXT("Customer waiting"), Sim.GetWaitingCustomers().Num(), 1);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 20.0);
	TestEqual(TEXT("Mismatch: customer still waiting"), Sim.GetWaitingCustomers().Num(), 1);
	if (TestEqual(TEXT("Mismatch: slots"), Shelf.Num(), 2))
	{
		TestEqual(TEXT("Mismatch: shelved"), Shelf[0].Count, 3);
	}
	TestEqual(TEXT("Mismatch: no sales"), Sim.GetResult().Sales, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDistributeDiscardWhenFullTest, "AIGame.BusinessSim.Distribute.DiscardWhenFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDistributeDiscardWhenFullTest::RunTest(const FString& Parameters)
{
	// R23 ③ · D8 (GDD 제작 완성품 배분, ADR-0011). 그 조합 재고가 상한이면 폐기 — 재료비는 탭 때 이미 냈다
	// 입력: 상한 5(Balance.csv MaxStockPerRecipe), 딸기 해금(Cost 5 · Price 12). 첫 도착을 영업 종료(180) 뒤로 둬 손님이 오지 않고, 7개를 연달아 만들게 게이지 10
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 1000.f;
	Input.Day.ArrivalMax = 1000.f;
	Input.Steamer.MaxGauge = 10;
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Strawberry.Price = 12;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FShelfEntry>& Shelf = Sim.GetShelf();

	// 경계: 5번째는 개수가 상한과 같아지므로 진열된다
	for (int32 Index = 0; Index < 5; ++Index)
	{
		MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0 + 3.0 * Index);
	}
	if (!TestEqual(TEXT("5th: one slot"), Shelf.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("5th: count = cap"), Shelf[0].Count, 5);
	TestEqual(TEXT("5th: not discarded"), Sim.GetResult().DiscardCount, 0);

	// 6번째: 폐기. 화면의 진열대 · 골드는 그대로라 작업대만 바뀐다. 재료비 (팥 3 + 떡 2) × 6 = 30
	const EBusinessChange SixthChanges = MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 16.0);
	TestTrue(TEXT("6th: Workbench only"), SixthChanges == EBusinessChange::Workbench);
	TestEqual(TEXT("6th: discarded"), Sim.GetResult().DiscardCount, 1);
	TestEqual(TEXT("6th: count stays at cap"), Shelf[0].Count, 5);
	TestEqual(TEXT("6th: cost charged"), Sim.GetResult().IngredientCost, 30);

	// 상한은 조합마다다
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 19.0);
	if (TestEqual(TEXT("Other recipe: new slot"), Shelf.Num(), 2))
	{
		TestEqual(TEXT("Other recipe: count"), Shelf[1].Count, 1);
	}
	TestEqual(TEXT("Other recipe: not discarded"), Sim.GetResult().DiscardCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimShelfOrderAndRemoveTest, "AIGame.BusinessSim.Shelf.OrderAndRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimShelfOrderAndRemoveTest::RunTest(const FString& Parameters)
{
	// R36 · D9 (GDD 화면 구성 진열대, ADR-0012). 칸 순서 = 처음 진열된 순서. 개수가 0이 되면 칸을 빼고 뒤 칸이 당겨지며, 다시 만들면 맨 뒤에 붙는다
	// 입력: 딸기(Price 12, 가중치 10) · 블루베리(Price 15, 가중치 0) 해금, 과일 없음 0 → 손님은 모두 팥+딸기. "아무거나" 0, 도착 간격 30초 고정, 게이지 10
	// 베이스는 모두 팥이므로 과일로 칸을 구분한다
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 30.f;
	Input.Day.ArrivalMax = 30.f;
	Input.Day.AnyOrderRate = 0.f;
	Input.Balance.NoFruitWeight = 0;
	Input.Steamer.MaxGauge = 10;
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Strawberry.Price = 12;
	Strawberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	FIngredientRow Blueberry;
	Blueberry.Cost = 9;
	Blueberry.Price = 15;
	Input.Ingredients.Add(EIngredient::Blueberry, Blueberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FShelfEntry>& Shelf = Sim.GetShelf();

	// 첫 도착(30) 전에 팥 → 팥+딸기 → 팥+딸기 → 팥+블루베리
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 4.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 7.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Blueberry, 10.0);
	if (TestEqual(TEXT("Made: slots"), Shelf.Num(), 3))
	{
		TestTrue(TEXT("Made: slot 0 = RedBean"), Shelf[0].Recipe.Fruit == EIngredient::None);
		TestTrue(TEXT("Made: slot 1 = RedBean+Strawberry"), Shelf[1].Recipe.Fruit == EIngredient::Strawberry);
		TestEqual(TEXT("Made: slot 1 count"), Shelf[1].Count, 2);
		TestTrue(TEXT("Made: slot 2 = RedBean+Blueberry"), Shelf[2].Recipe.Fruit == EIngredient::Blueberry);
	}

	// t=30 손님1이 팥+딸기를 산다: 개수만 줄고 칸 위치는 그대로
	Sim.ProcessDueEvents(30.0);
	if (TestEqual(TEXT("Count down: slots"), Shelf.Num(), 3))
	{
		TestTrue(TEXT("Count down: slot 1 = RedBean+Strawberry"), Shelf[1].Recipe.Fruit == EIngredient::Strawberry);
		TestEqual(TEXT("Count down: slot 1 count"), Shelf[1].Count, 1);
		TestTrue(TEXT("Count down: slot 2 = RedBean+Blueberry"), Shelf[2].Recipe.Fruit == EIngredient::Blueberry);
	}

	// t=60 손님2가 마지막 팥+딸기를 산다: 칸을 빼고 뒤 칸이 당겨진다
	Sim.ProcessDueEvents(60.0);
	if (TestEqual(TEXT("Removed: slots"), Shelf.Num(), 2))
	{
		TestTrue(TEXT("Removed: slot 0 = RedBean"), Shelf[0].Recipe.Fruit == EIngredient::None);
		TestTrue(TEXT("Removed: slot 1 = RedBean+Blueberry (pulled forward)"), Shelf[1].Recipe.Fruit == EIngredient::Blueberry);
	}

	// 팥+딸기를 다시 만들면 맨 뒤에 붙는다. 다음 도착은 90
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 61.0);
	if (TestEqual(TEXT("Remade: slots"), Shelf.Num(), 3))
	{
		TestTrue(TEXT("Remade: slot 0 = RedBean"), Shelf[0].Recipe.Fruit == EIngredient::None);
		TestTrue(TEXT("Remade: slot 1 = RedBean+Blueberry"), Shelf[1].Recipe.Fruit == EIngredient::Blueberry);
		TestTrue(TEXT("Remade: slot 2 = RedBean+Strawberry"), Shelf[2].Recipe.Fruit == EIngredient::Strawberry);
		TestEqual(TEXT("Remade: slot 2 count"), Shelf[2].Count, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimPriceSingleAndMixTest, "AIGame.BusinessSim.Price.SingleAndMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimPriceSingleAndMixTest::RunTest(const FString& Parameters)
{
	// R4 (GDD 경제 판매가, data-schema.md Ingredients 절)
	// 입력: Ingredients.csv Price 팥 10 · 딸기 12 · 요거트 22 · 블루베리 15, Balance.csv MixMultiplier 1.25
	// 모든 손님을 "아무거나"(1 — 항상)로 두면 어떤 조합이든 기다리는 손님에게 팔린다. 도착 간격 10초 고정, 인내심 1000 → 30까지 3명
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 10.f;
	Input.Day.ArrivalMax = 10.f;
	Input.Day.Patience = 1000.f;
	Input.Day.AnyOrderRate = 1.f;
	FIngredientRow Strawberry;
	Strawberry.Price = 12;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	FIngredientRow Yogurt;
	Yogurt.Price = 22;
	Input.Ingredients.Add(EIngredient::Yogurt, Yogurt);
	FIngredientRow Blueberry;
	Blueberry.Price = 15;
	Input.Ingredients.Add(EIngredient::Blueberry, Blueberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	Sim.ProcessDueEvents(30.0);
	TestEqual(TEXT("Three waiting"), Sim.GetWaitingCustomers().Num(), 3);

	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 30.0);
	TestEqual(TEXT("Single: base price"), Sim.GetResult().Sales, 10);

	// (10 + 12) × 1.25 = 27.5 → 28. 경계: 소수부 0.5는 올린다(GenericPlatformMath.h RoundToInt32)
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 33.0);
	TestEqual(TEXT("Mix: 27.5 rounds up to 28"), Sim.GetResult().Sales, 38);

	// (22 + 15) × 1.25 = 46.25 → 46
	MakeSimProduct(Sim, EIngredient::Yogurt, EIngredient::Blueberry, 36.0);
	TestEqual(TEXT("Mix: 46.25 rounds down to 46"), Sim.GetResult().Sales, 84);
	TestEqual(TEXT("Sold"), Sim.GetResult().SoldCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerBuyFromShelfOnArriveTest, "AIGame.BusinessSim.Customer.BuyFromShelfOnArrive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerBuyFromShelfOnArriveTest::RunTest(const FString& Parameters)
{
	// R15 · D13 (GDD 손님, ADR-0015). 원하는 조합이 진열대에 있으면 대기열이 꽉 차 있어도 바로 사 가고, 대기열에는 들어가지 않는다
	// 입력: 대기열 1, 딸기(Price 12) · 블루베리(Price 15) 해금 · 주문 가중치 10씩, 과일 없음 0 → 주문은 팥+딸기 또는 팥+블루베리
	// "아무거나" 0, 도착 간격 10초 고정, 인내심 1000
	// 시드 1: 손님1 = 팥+딸기, 손님2 = 팥+블루베리. RandomStream.h 알고리즘을 재현해 고른 장면이라 손님1을 먼저 확인한다
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 10.f;
	Input.Day.ArrivalMax = 10.f;
	Input.Day.Patience = 1000.f;
	Input.Day.MaxQueue = 1;
	Input.Day.AnyOrderRate = 0.f;
	Input.Balance.NoFruitWeight = 0;
	Input.RandomSeed = 1;
	FIngredientRow Strawberry;
	Strawberry.Price = 12;
	Strawberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	FIngredientRow Blueberry;
	Blueberry.Price = 15;
	Blueberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Blueberry, Blueberry);

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	const TArray<FCustomer>& Waiting = Sim.GetWaitingCustomers();
	Sim.ProcessDueEvents(10.0);
	const bool bScene = Waiting.Num() == 1 && Waiting[0].Order.Fruit == EIngredient::Strawberry;
	if (!TestTrue(TEXT("Scene: customer 1 = RedBean+Strawberry, queue full"), bScene))
	{
		return false;
	}

	// 손님1 주문과 다른 팥+블루베리 → 진열대
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Blueberry, 11.0);
	TestEqual(TEXT("Shelved"), Sim.GetShelf().Num(), 1);

	// t=20 손님2(팥+블루베리): 대기열이 꽉 찼지만 재고가 있어 바로 산다. 손님 줄은 그대로라 진열대 · 골드만 바뀐다(architecture.md 4.5)
	const EBusinessChange ArriveChanges = Sim.ProcessDueEvents(20.0);
	TestTrue(TEXT("Arrive: Shelf | Gold"), ArriveChanges == (EBusinessChange::Shelf | EBusinessChange::Gold));
	if (TestEqual(TEXT("Arrive: queue unchanged"), Waiting.Num(), 1))
	{
		TestEqual(TEXT("Arrive: slot 0 still customer 1"), Waiting[0].Id, 1);
	}
	TestEqual(TEXT("Arrive: stock sold out"), Sim.GetShelf().Num(), 0);
	// (10 + 15) × 1.25 = 31.25 → 31
	TestEqual(TEXT("Arrive: sales"), Sim.GetResult().Sales, 31);
	TestEqual(TEXT("Arrive: sold"), Sim.GetResult().SoldCount, 1);
	TestEqual(TEXT("Arrive: not missed"), Sim.GetResult().MissedCustomers, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimCustomerAnyPicksByCountTest, "AIGame.BusinessSim.Customer.AnyPicksByCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimCustomerAnyPicksByCountTest::RunTest(const FString& Parameters)
{
	// R16 · D12 (GDD 손님, ADR-0015). "아무거나" 손님은 진열된 모찌 하나하나를 같은 확률로 고른다 — 개수 비례
	// 입력: 딸기 해금(Price 12), 모든 손님 "아무거나", 첫 도착 30. 도착 전에 진열대 [팥 4, 팥+딸기 1]을 만든다(게이지 10)
	// 시드 1~100에서 팥이 뽑힌 수: 기대 80. 칸마다 같은 확률이면 50, 맨 앞 칸 고정이면 100, 맨 뒤 칸 고정이면 0 → 70~90만 통과
	// 시드 고정이라 결과는 매번 같다(RandomStream.h 알고리즘 재현으로 79)
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 30.f;
	Input.Day.ArrivalMax = 30.f;
	Input.Day.AnyOrderRate = 1.f;
	Input.Steamer.MaxGauge = 10;
	FIngredientRow Strawberry;
	Strawberry.Price = 12;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);

	int32 RedBeanPicks = 0;
	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		Input.RandomSeed = Seed;
		FBusinessSim Sim;
		Sim.StartDay(Input, 0.0);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0 + 3.0 * Index);
		}
		MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::Strawberry, 13.0);
		Sim.ProcessDueEvents(30.0);

		// 팥을 사면 [팥 3, 팥+딸기 1], 팥+딸기를 사면 그 칸이 빠져 [팥 4]
		const TArray<FShelfEntry>& Shelf = Sim.GetShelf();
		const bool bRedBeanPicked = Shelf.Num() == 2 && Shelf[0].Count == 3 && Shelf[1].Count == 1;
		const bool bStrawberryPicked = Shelf.Num() == 1 && Shelf[0].Count == 4;
		TestTrue(TEXT("Exactly one bought from shelf"), bRedBeanPicked || bStrawberryPicked);
		if (bRedBeanPicked)
		{
			++RedBeanPicks;
		}
	}
	TestTrue(TEXT("Picks proportional to count: RedBean 70~90 of 100"), RedBeanPicks >= 70 && RedBeanPicks <= 90);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDayEndDiscardAndClearTest, "AIGame.BusinessSim.DayEnd.DiscardAndClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDayEndDiscardAndClearTest::RunTest(const FString& Parameters)
{
	// R29 · D14 · F1 (GDD 하루 진행 영업 종료, ADR-0015, architecture.md 3.6 DayEnd · 4.7 F1). 남은 재고와 작업대 위 재료는 폐기, 대기 손님은 돌아간다(놓친 손님 아님)
	// 쓰레기 연출 중이던 작업대는 판정 때 쓰레기 수에 들어갔으므로 폐기에 더하지 않는다(architecture.md 3.6 DayEnd 메모)
	// 입력: 딸기(Cost 5, 가중치 0) · 블루베리(가중치 10) 해금, 과일 없음 0 → 손님은 모두 팥+블루베리. "아무거나" 0, 도착 간격 100초 고정, 인내심 1000
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 100.f;
	Input.Day.ArrivalMax = 100.f;
	Input.Day.Patience = 1000.f;
	Input.Day.AnyOrderRate = 0.f;
	Input.Balance.NoFruitWeight = 0;
	FIngredientRow Strawberry;
	Strawberry.Cost = 5;
	Input.Ingredients.Add(EIngredient::Strawberry, Strawberry);
	FIngredientRow Blueberry;
	Blueberry.OrderWeight = 10;
	Input.Ingredients.Add(EIngredient::Blueberry, Blueberry);

	FBusinessSim Sim;

	// (A) 진열 [팥 2] + 감싸기 중 + 기다리는 손님 1명
	Sim.StartDay(Input, 0.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0);
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 4.0);
	Sim.ProcessDueEvents(100.0);
	TestEqual(TEXT("A: customer waiting"), Sim.GetWaitingCustomers().Num(), 1);
	// 179.5 + WrapTime 0.6 = 180.1 > 영업 종료 180 → 감싸기 중에 끝난다
	Sim.TapIngredient(EIngredient::RedBean, 178.0);
	Sim.TapIngredient(EIngredient::Strawberry, 179.0);
	Sim.TapSteamer(179.5);
	TestTrue(TEXT("A: wrapping before day end"), Sim.GetWorkbenchState() == EWorkbenchState::Wrapping);
	const EBusinessChange EndChanges = Sim.ProcessDueEvents(180.0);
	TestTrue(TEXT("A: DayEnded"), EnumHasAnyFlags(EndChanges, EBusinessChange::DayEnded));
	TestTrue(TEXT("A: Shelf changed"), EnumHasAnyFlags(EndChanges, EBusinessChange::Shelf));
	// 진열 2 + 감싸기 중 1
	TestEqual(TEXT("A: discard = stock + workbench"), Sim.GetResult().DiscardCount, 3);
	TestEqual(TEXT("A: shelf cleared"), Sim.GetShelf().Num(), 0);
	TestEqual(TEXT("A: queue cleared"), Sim.GetWaitingCustomers().Num(), 0);
	TestEqual(TEXT("A: waiting customer is not missed"), Sim.GetResult().MissedCustomers, 0);
	TestTrue(TEXT("A: workbench Empty"), Sim.GetWorkbenchState() == EWorkbenchState::Empty);
	TestTrue(TEXT("A: recipe cleared"), Sim.GetWorkbenchRecipe() == FRecipe());

	// (B) 쓰레기 연출 중(179.8 + TrashTime 0.5 = 180.3)에 영업 종료
	Sim.StartDay(Input, 0.0);
	Sim.TapIngredient(EIngredient::Strawberry, 179.8);
	Sim.ProcessDueEvents(180.0);
	TestEqual(TEXT("B: counted as trash"), Sim.GetResult().TrashCount, 1);
	TestEqual(TEXT("B: trash is not discard"), Sim.GetResult().DiscardCount, 0);

	// (C) 베이스만 올린 채 영업 종료
	Sim.StartDay(Input, 0.0);
	Sim.TapIngredient(EIngredient::RedBean, 179.9);
	Sim.ProcessDueEvents(180.0);
	TestEqual(TEXT("C: ingredient on workbench discarded"), Sim.GetResult().DiscardCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusinessSimDayEndResultCountsTest, "AIGame.BusinessSim.DayEnd.ResultCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBusinessSimDayEndResultCountsTest::RunTest(const FString& Parameters)
{
	// R37 (GDD 화면 구성 결산 팝업 · D14). 하루 집계의 항목마다 그 값을 만드는 경로를 한 번씩 지난다
	// 입력: 팥만 해금(Cost 3 · Price 10) → 주문은 모두 팥, "아무거나" 0. 도착 간격 40초 고정(40 · 80 · 120 · 160), 인내심 25(Days.csv 1일차)
	// 진열 상한 1 — 두 번째 팥이 바로 폐기되게 한다. 시작 골드 100 · 임대료 50
	FBusinessDayInput Input = MakeSimDayInput();
	Input.Day.ArrivalMin = 40.f;
	Input.Day.ArrivalMax = 40.f;
	Input.Day.AnyOrderRate = 0.f;
	Input.Balance.MaxStockPerRecipe = 1;

	FBusinessSim Sim;
	Sim.StartDay(Input, 0.0);
	// 팥 → 진열 [팥 1]. 재료비 팥 3 + 떡 2 = 5
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 1.0);
	// 팥 → 상한이라 폐기 1. 재료비 10
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 4.0);
	// 팥 + 팥 → 쓰레기(2번) 1. 재료비 16
	Sim.TapIngredient(EIngredient::RedBean, 7.0);
	Sim.TapIngredient(EIngredient::RedBean, 8.0);
	Sim.ProcessDueEvents(9.0);
	// t=40 손님1: 진열된 팥을 바로 산다(판매 1 · 매출 10). t=80 손님2: 재고가 없어 기다린다(만료 105)
	Sim.ProcessDueEvents(80.0);
	// 팥 완성(83.6) → 손님2에게(판매 2 · 매출 20). 재료비 21
	MakeSimProduct(Sim, EIngredient::RedBean, EIngredient::None, 81.0);
	// 손님2의 만료(105)는 버려진다. t=120 손님3은 145에 떠난다(놓친 손님 1). t=160 손님4는 만료(185) 전에 영업이 끝난다(놓친 손님 아님)
	Sim.ProcessDueEvents(160.0);
	// 베이스만 올린 채 영업 종료 → 폐기 2. 재료비 24
	Sim.TapIngredient(EIngredient::RedBean, 179.0);
	Sim.ProcessDueEvents(180.0);

	const FDayResult& Result = Sim.GetResult();
	TestTrue(TEXT("Day over"), Sim.IsDayOver());
	TestEqual(TEXT("Sales"), Result.Sales, 20);
	TestEqual(TEXT("IngredientCost"), Result.IngredientCost, 24);
	TestEqual(TEXT("SoldCount"), Result.SoldCount, 2);
	TestEqual(TEXT("MissedCustomers"), Result.MissedCustomers, 1);
	TestEqual(TEXT("TrashCount"), Result.TrashCount, 1);
	TestEqual(TEXT("DiscardCount"), Result.DiscardCount, 2);
	TestEqual(TEXT("Rent"), Result.Rent, 50);
	// 100 − 50 + 20 − 24
	TestEqual(TEXT("Current gold"), Sim.GetCurrentGold(), 46);
	return true;
}

#endif
