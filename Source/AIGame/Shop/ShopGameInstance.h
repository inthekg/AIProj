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
	/** 로비 위젯(BP) 연동 */
	UPROPERTY(BlueprintAssignable)
	FOnProgressChanged OnProgressChanged;

	virtual void Init() override;

	UFUNCTION(BlueprintPure)
	FShopProgress GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure)
	void GetUnlockedIngredients(TArray<EIngredient>& OutBases, TArray<EIngredient>& OutFruits) const;

	UFUNCTION(BlueprintPure)
	bool GetNextUnlock(EIngredient& OutIngredient, FIngredientRow& OutRow) const;

	UFUNCTION(BlueprintCallable)
	bool TryUnlockNext();

	UFUNCTION(BlueprintPure)
	bool GetNextSteamerLevel(FSteamerRow& OutRow) const;

	UFUNCTION(BlueprintCallable)
	bool TryUpgradeSteamer();

	UFUNCTION(BlueprintPure)
	FBalanceRow GetBalance() const;

	/** 오늘 영업에 필요한 데이터를 채운다.*/
	void FillDayInput(FBusinessDayInput& OutInput) const;

	/**
	 * 하루 결과를 진행 값에 반영하고 판정을 돌려준다. 결산 때 한 번만 부른다.
	 */
	EDayOutcome ApplyDayResult(const FDayResult& Result);

private:
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
};
