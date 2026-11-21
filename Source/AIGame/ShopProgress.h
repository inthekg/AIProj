#pragma once

#include "CoreMinimal.h"
#include "ShopTypes.h"
#include "ShopProgress.generated.h"

/** 저장되는 진행 값(Day · Gold · UnlockStep · SteamerLevel)과 구매 · 결산 · 파산 규칙 */
USTRUCT(BlueprintType)
struct FShopProgress
{
	GENERATED_BODY()

	/** 현재 일차. 1부터 시작하고 결산이 GameOver가 아니면 1 오른다 */
	UPROPERTY(BlueprintReadOnly)
	int32 Day = 1;

	/** 0보다 작으면 빚("파산위기!") */
	UPROPERTY(BlueprintReadOnly)
	int32 Gold = 0;

	/** 해금된 마지막 UnlockOrder. 0 = 기본 재료만 */
	UPROPERTY(BlueprintReadOnly)
	int32 UnlockStep = 0;

	/** 찜기 레벨 = Steamer.csv 행 이름(레벨 숫자). 1부터 */
	UPROPERTY(BlueprintReadOnly)
	int32 SteamerLevel = 1;

	/** 새 게임(1일차 · 기본 재료 · 찜기 1레벨). 앱 첫 실행과 게임 오버 때 쓴다 */
	static FShopProgress MakeNew(int32 StartGold);

	/**
	 * Gold ≥ Cost면 차감하고 true, 아니면 아무것도 바꾸지 않고 false. 로비 구매 전용 — 빚은 재료비 · 임대료로만 생긴다(GDD · ADR-0016).
	 * 전제: Cost > 0 (check). 구매 비용이 양수인지는 UShopGameInstance의 데이터 검증이 먼저 확인한다.
	 */
	bool TrySpend(int32 Cost);

	/**
	 * 영업 하루의 결과를 반영한다. 결산 때 한 번 부른다.
	 * Gold에 순이익(Sales − IngredientCost − Rent)을 더한다.
	 * 전날 결산이 빚(이 함수를 부르기 전 Gold < 0)이었고 지금도 빚이면 GameOver — 호출 쪽이 MakeNew로 바꾼다.
	 * 임대료 때문에 영업 중에 빚이 된 것은 '전날 빚'이 아니다(판정은 결산 때만, GDD).
	 * 아니면 Day를 1 올리고, Gold < 0이면 DebtWarning, 아니면 Normal.
	 */
	EDayOutcome ApplyDayResult(const FDayResult& Result);
};
