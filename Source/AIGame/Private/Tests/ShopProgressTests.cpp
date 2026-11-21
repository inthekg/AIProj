#include "Misc/AutomationTest.h"
#include "ShopProgress.h"

// WITH_DEV_AUTOMATION_TESTS는 UBT가 정의하고 Shipping에서는 0이다 (UEBuildTarget.cs)
#if WITH_DEV_AUTOMATION_TESTS

// RunTest가 true를 돌려줘도 TestEqual · TestTrue가 남긴 오류가 있으면 실패로 판정된다 (AutomationTest.cpp 1092행)
// 입력값은 리터럴로 쓰고 설계 문서에서 가져온 값에는 출처를 단다. 기대값도 리터럴 — 계산식으로 쓰면 구현과 같은 식을 되풀이한다

// 결산에 쓰는 세 값만 채운다. 나머지 집계는 ApplyDayResult가 읽지 않는다
// 이름을 모듈 안에서 겹치지 않게 둔다 — unity build에서 다른 테스트 파일과 한 번역 단위로 합쳐질 수 있다
static FDayResult MakeSettlementResult(int32 Sales, int32 IngredientCost, int32 Rent)
{
	FDayResult Result;
	Result.Sales = Sales;
	Result.IngredientCost = IngredientCost;
	Result.Rent = Rent;
	return Result;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopProgressMakeNewTest, "AIGame.ShopProgress.MakeNew",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FShopProgressMakeNewTest::RunTest(const FString& Parameters)
{
	// R28 (GDD 하루 진행)
	const FShopProgress Progress = FShopProgress::MakeNew(100);
	TestEqual(TEXT("Day"), Progress.Day, 1);
	TestEqual(TEXT("Gold = StartGold"), Progress.Gold, 100);
	TestEqual(TEXT("UnlockStep"), Progress.UnlockStep, 0);
	TestEqual(TEXT("SteamerLevel"), Progress.SteamerLevel, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopProgressTrySpendTest, "AIGame.ShopProgress.TrySpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FShopProgressTrySpendTest::RunTest(const FString& Parameters)
{
	// R10 (GDD 빚과 파산)
	FShopProgress Enough;
	Enough.Gold = 100;
	const bool bEnoughSpent = Enough.TrySpend(40);
	TestTrue(TEXT("Gold > Cost: succeeds"), bEnoughSpent);
	TestEqual(TEXT("Gold > Cost: gold"), Enough.Gold, 60);

	// 경계: Gold == Cost
	FShopProgress Exact;
	Exact.Gold = 40;
	const bool bExactSpent = Exact.TrySpend(40);
	TestTrue(TEXT("Gold == Cost: succeeds"), bExactSpent);
	TestEqual(TEXT("Gold == Cost: gold"), Exact.Gold, 0);

	FShopProgress Short;
	Short.Gold = 30;
	const bool bShortSpent = Short.TrySpend(40);
	TestFalse(TEXT("Gold < Cost: fails"), bShortSpent);
	TestEqual(TEXT("Gold < Cost: gold unchanged"), Short.Gold, 30);

	// 빚인 상태의 구매 — 입력: architecture.md 4.6
	FShopProgress InDebt;
	InDebt.Gold = -30;
	const bool bInDebtSpent = InDebt.TrySpend(40);
	TestFalse(TEXT("In debt: fails"), bInDebtSpent);
	TestEqual(TEXT("In debt: gold unchanged"), InDebt.Gold, -30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopProgressDebtWarningTest, "AIGame.ShopProgress.DebtWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FShopProgressDebtWarningTest::RunTest(const FString& Parameters)
{
	// R8 (GDD 빚과 파산)
	// 입력: architecture.md 4.6 3일차
	FShopProgress Day3;
	Day3.Day = 3;
	Day3.Gold = 20;
	const EDayOutcome Day3Outcome = Day3.ApplyDayResult(MakeSettlementResult(50, 100, 0));
	TestEqual(TEXT("4.6 day 3: outcome"), Day3Outcome, EDayOutcome::DebtWarning);
	TestEqual(TEXT("4.6 day 3: gold"), Day3.Gold, -30);
	TestEqual(TEXT("4.6 day 3: day"), Day3.Day, 4);

	// 경계: 시작 Gold 0, 임대료만으로 빚이 되는 하루
	FShopProgress ZeroStart;
	ZeroStart.Gold = 0;
	const EDayOutcome ZeroStartOutcome = ZeroStart.ApplyDayResult(MakeSettlementResult(0, 0, 50));
	TestEqual(TEXT("Start gold 0: outcome"), ZeroStartOutcome, EDayOutcome::DebtWarning);
	TestEqual(TEXT("Start gold 0: gold"), ZeroStart.Gold, -50);

	// 경계: 결산 결과 Gold 0
	FShopProgress ZeroEnd;
	ZeroEnd.Gold = 20;
	const EDayOutcome ZeroEndOutcome = ZeroEnd.ApplyDayResult(MakeSettlementResult(50, 30, 40));
	TestEqual(TEXT("End gold 0: outcome"), ZeroEndOutcome, EDayOutcome::Normal);
	TestEqual(TEXT("End gold 0: gold"), ZeroEnd.Gold, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopProgressBankruptcyTest, "AIGame.ShopProgress.Bankruptcy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FShopProgressBankruptcyTest::RunTest(const FString& Parameters)
{
	// R9 · R31 · R43 (GDD 빚과 파산)
	// 입력: architecture.md 4.6 4일차
	// GameOver 뒤 Gold · Day는 확인하지 않는다 — 호출 쪽(UShopGameInstance)이 곧바로 MakeNew로 바꾼다(3.5)
	FShopProgress StillInDebt;
	StillInDebt.Day = 4;
	StillInDebt.Gold = -30;
	const EDayOutcome StillInDebtOutcome = StillInDebt.ApplyDayResult(MakeSettlementResult(40, 50, 0));
	TestEqual(TEXT("4.6 day 4 still in debt: outcome"), StillInDebtOutcome, EDayOutcome::GameOver);

	// 경계: 빚을 딱 갚아 결산 결과 Gold 0 — 입력: architecture.md 4.6 메모
	FShopProgress Repaid;
	Repaid.Day = 4;
	Repaid.Gold = -30;
	const EDayOutcome RepaidOutcome = Repaid.ApplyDayResult(MakeSettlementResult(80, 50, 0));
	TestEqual(TEXT("Debt repaid: outcome"), RepaidOutcome, EDayOutcome::Normal);
	TestEqual(TEXT("Debt repaid: gold"), Repaid.Gold, 0);
	TestEqual(TEXT("Debt repaid: day"), Repaid.Day, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopProgressRentInSettlementTest, "AIGame.ShopProgress.RentInSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FShopProgressRentInSettlementTest::RunTest(const FString& Parameters)
{
	// R45 (ADR-0016)
	FShopProgress Profit;
	Profit.Gold = 100;
	const EDayOutcome ProfitOutcome = Profit.ApplyDayResult(MakeSettlementResult(150, 60, 50));
	TestEqual(TEXT("Profit: outcome"), ProfitOutcome, EDayOutcome::Normal);
	TestEqual(TEXT("Profit: gold"), Profit.Gold, 140);
	TestEqual(TEXT("Profit: day"), Profit.Day, 2);

	// 임대료가 없었다면 Normal이었을 하루
	FShopProgress RentDebt;
	RentDebt.Gold = 10;
	const EDayOutcome RentDebtOutcome = RentDebt.ApplyDayResult(MakeSettlementResult(100, 60, 60));
	TestEqual(TEXT("Rent causes debt: outcome"), RentDebtOutcome, EDayOutcome::DebtWarning);
	TestEqual(TEXT("Rent causes debt: gold"), RentDebt.Gold, -10);
	return true;
}

#endif
