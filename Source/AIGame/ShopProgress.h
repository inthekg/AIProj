#pragma once

#include "CoreMinimal.h"
#include "ShopTypes.h"
#include "ShopProgress.generated.h"

/** 저장되는 진행 값(Day · Gold · UnlockStep · SteamerLevel)과 구매 · 결산 · 파산 규칙 */
USTRUCT(BlueprintType)
struct FShopProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Day = 1;

	/** 음수 = 빚 */
	UPROPERTY(BlueprintReadOnly)
	int32 Gold = 0;

	/** 해금된 마지막 UnlockOrder. 0 = 기본 재료만 */
	UPROPERTY(BlueprintReadOnly)
	int32 UnlockStep = 0;

	/** Steamer DataTable 행 이름(레벨 숫자). 1부터 */
	UPROPERTY(BlueprintReadOnly)
	int32 SteamerLevel = 1;

	/** 새 게임의 진행 값을 만든다. 앱 첫 실행과 게임 오버 때 쓴다. 시작 조건: GDD 하루 진행 */
	static FShopProgress MakeNew(int32 StartGold);

	/**
	 * 로비 구매 비용을 차감한다. 성공하면 true, 실패하면 아무것도 바꾸지 않고 false. 로비 구매에서만 부른다.
	 * 전제: Cost > 0 (check). 구매 조건: GDD 빚과 파산
	 */
	bool TrySpend(int32 Cost);

	/**
	 * 하루 결과를 진행 값(Gold · Day)에 반영하고 판정을 반환한다. 결산 때 한 번만 부른다.
	 * GameOver면 Day를 올리지 않는다 — 호출 쪽이 MakeNew로 바꾼다. 판정 규칙: GDD 빚과 파산 · ADR-0016
	 */
	EDayOutcome ApplyDayResult(const FDayResult& Result);
};
