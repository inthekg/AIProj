#include "ShopProgress.h"

FShopProgress FShopProgress::MakeNew(int32 StartGold)
{
	// 시작 조건(GDD 하루 진행)의 원본은 멤버 기본값이다. 데이터에서 오는 값은 StartGold뿐이다
	FShopProgress Progress;
	Progress.Gold = StartGold;
	return Progress;
}

bool FShopProgress::TrySpend(int32 Cost)
{
	// 음수 비용은 빚인 상태에서도 구매를 성공시키고 Gold를 늘린다. 데이터 검증(입력 경계)과 별도로 여기서도 막는다(T02 사용자 결정)
	check(Cost > 0);
	if (Gold < Cost)
	{
		return false;
	}
	Gold -= Cost;
	return true;
}

EDayOutcome FShopProgress::ApplyDayResult(const FDayResult& Result)
{
	// 순서 중요: '전날 빚'은 이번 순이익을 더하기 전 Gold로 판정한다(ADR-0016)
	const bool bWasInDebt = Gold < 0;
	Gold += Result.Sales - Result.IngredientCost - Result.Rent;
	if (bWasInDebt && Gold < 0)
	{
		// 호출 쪽이 곧바로 MakeNew로 바꾸므로 Day는 올리지 않는다(3.3 계약)
		return EDayOutcome::GameOver;
	}
	++Day;
	if (Gold < 0)
	{
		return EDayOutcome::DebtWarning;
	}
	return EDayOutcome::Normal;
}
