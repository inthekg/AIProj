#include "Misc/AutomationTest.h"
#include "Business/BusinessSim.h"

// WITH_DEV_AUTOMATION_TESTS는 UBT가 정의하고 Shipping에서는 0이다 (UEBuildTarget.cs)
#if WITH_DEV_AUTOMATION_TESTS

// 입력값은 리터럴로 쓰고 설계 문서 · 데이터에서 가져온 값에는 출처를 단다. 기대값도 리터럴 — 계산식으로 쓰면 구현과 같은 식을 되풀이한다

// 하루 입력의 공통 부분. 입력: Data/Days.csv 1일차 · Balance.csv Default · Ingredients.csv 팥 행, 팥만 해금(GDD 시작 조건), 외형 5종(GDD 손님)
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
	Input.Balance.NoFruitWeight = 8;
	Input.Balance.BaseRent = 50;

	FIngredientRow RedBean;
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

#endif
