#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ShopProgress.h"
#include "ShopGameInstance.generated.h"

class UDataTable;
struct FBusinessDayInput;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProgressChanged);

/** 앱 전체 진행 상태를 소유하고 저장 · 불러오기, 로비 구매, 하루 결과 적용을 한다 */
UCLASS()
class UShopGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** 로비 구매로 진행 값이 바뀐 뒤(저장까지 끝난 뒤) 브로드캐스트. 로비 위젯이 다시 그린다 */
	UPROPERTY(BlueprintAssignable)
	FOnProgressChanged OnProgressChanged;

	/** 데이터 검증 → 슬롯 불러오기 → 백그라운드 저장 구독 */
	virtual void Init() override;
	/** 백그라운드 구독 해제 */
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure)
	FShopProgress GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure)
	void GetUnlockedIngredients(TArray<EIngredient>& OutBases, TArray<EIngredient>& OutFruits) const;

	UFUNCTION(BlueprintPure)
	bool GetNextUnlock(EIngredient& OutIngredient, FIngredientRow& OutRow) const;

	/** 다음 해금을 산다. 남은 해금이 없거나 Gold < 비용이면 false. 성공하면 저장 후 OnProgressChanged */
	UFUNCTION(BlueprintCallable)
	bool TryUnlockNext();

	UFUNCTION(BlueprintPure)
	bool GetNextSteamerLevel(FSteamerRow& OutRow) const;

	/** 찜기를 한 단계 올린다. 실패 조건 · 성공 처리는 TryUnlockNext와 같다 */
	UFUNCTION(BlueprintCallable)
	bool TryUpgradeSteamer();

	UFUNCTION(BlueprintPure)
	FBalanceRow GetBalance() const;

	/** 오늘 영업에 필요한 데이터를 채운다.*/
	void FillDayInput(FBusinessDayInput& OutInput) const;

	/**
	 * 하루 결과를 진행 값에 반영하고 바로 저장한다(GDD: 결산 확정 직후 저장). 결산 때 한 번만 부른다.
	 * GameOver면 진행 값을 새 게임으로 바꿔 저장한다(GDD: 게임 오버 확정 즉시 저장 초기화).
	 */
	EDayOutcome ApplyDayResult(const FDayResult& Result);

private:
	static constexpr const TCHAR* SaveSlotName = TEXT("Progress");
	static constexpr int32 SaveUserIndex = 0;

	/** 손님 줄 화면 슬롯 수(ADR-0009). Days.MaxQueue의 상한 */
	static constexpr int32 QueueSlotCount = 5;
	/** Balance DataTable의 유일한 행 이름(ADR-0013) */
	static constexpr const TCHAR* BalanceRowName = TEXT("Default");

	/** Data/Ingredients.csv를 가져온 DataTable (행: FIngredientRow) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UDataTable> IngredientTable;

	/** Data/Days.csv를 가져온 DataTable (행: FDayRow) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UDataTable> DayTable;

	/** Data/Steamer.csv를 가져온 DataTable (행: FSteamerRow) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UDataTable> SteamerTable;

	/** Data/Balance.csv를 가져온 DataTable (행: FBalanceRow) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UDataTable> BalanceTable;

	UPROPERTY()
	FShopProgress Progress;

	const FIngredientRow* FindIngredientRow(EIngredient Ingredient) const;
	const FSteamerRow* FindSteamerRow(int32 Level) const;

	/** 진행 값을 슬롯에 쓰고 결과를 LogAIGame에 남긴다(실패는 Error). 앱 백그라운드 진입 때도 불린다 */
	void SaveProgress() const;
};
