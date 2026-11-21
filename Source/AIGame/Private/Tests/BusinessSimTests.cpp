#include "Misc/AutomationTest.h"
#include "BusinessSim.h"

// WITH_DEV_AUTOMATION_TESTS는 UBT가 정의하고 Shipping에서는 0이다 (UEBuildTarget.cs)
#if WITH_DEV_AUTOMATION_TESTS

// 입력값은 리터럴로 쓰고 설계 문서 · 데이터에서 가져온 값에는 출처를 단다. 기대값도 리터럴 — 계산식으로 쓰면 구현과 같은 식을 되풀이한다

// 하루 입력의 공통 부분. 입력: Data/Days.csv 1일차 · Balance.csv Default, 팥만 해금(GDD 시작 조건)
// 이름을 모듈 안에서 겹치지 않게 둔다 — unity build에서 다른 테스트 파일과 한 번역 단위로 합쳐질 수 있다
static FBusinessDayInput MakeSimDayInput()
{
	FBusinessDayInput Input;
	Input.Day.Duration = 180.f;
	Input.Day.ArrivalMin = 5.5f;
	Input.Day.ArrivalMax = 8.5f;
	Input.Balance.BaseRent = 50;
	Input.Ingredients.Add(EIngredient::RedBean, FIngredientRow());
	Input.GoldAtStart = 100;
	Input.RandomSeed = 1;
	return Input;
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

	const EBusinessChange BeforeEndChanges = Sim.Advance(189.5);
	TestFalse(TEXT("Before end: not over"), Sim.IsDayOver());
	TestFalse(TEXT("Before end: no DayEnded"), EnumHasAnyFlags(BeforeEndChanges, EBusinessChange::DayEnded));

	// 경계: 영업 종료 시각과 같은 Now에서 처리된다(Now 이하 — architecture.md 3.6 Advance)
	// 기대 플래그: 영역 5개(빈 화면을 그리는 델리게이트 5종, architecture.md 4.1) + DayEnded
	const EBusinessChange EndChanges = Sim.Advance(190.0);
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
			Sim.Advance(Next.GetValue());
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

#endif
