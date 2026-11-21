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
	/** 로비 구매로 진행 값이 바뀐 뒤 브로드캐스트. 로비 위젯이 다시 그린다 */
	UPROPERTY(BlueprintAssignable)
	FOnProgressChanged OnProgressChanged;

	/** 기획 데이터를 검증하고 새 게임 진행 값으로 시작한다. 검증에 실패하면 checkf로 멈춘다(data-schema.md '데이터 검증') */
	virtual void Init() override;

	UFUNCTION(BlueprintPure)
	FShopProgress GetProgress() const { return Progress; }

	/** 해금된 재료를 베이스 / 과일로 나눠 UnlockOrder 순으로 돌려준다. 재료통 배치와 하루 입력에 쓴다 */
	UFUNCTION(BlueprintPure)
	void GetUnlockedIngredients(TArray<EIngredient>& OutBases, TArray<EIngredient>& OutFruits) const;

	/** 다음에 살 해금 재료. 모두 해금했으면 false(목표 달성) */
	UFUNCTION(BlueprintPure)
	bool GetNextUnlock(EIngredient& OutIngredient, FIngredientRow& OutRow) const;

	/** 다음 해금을 산다. 실패하면 아무것도 바꾸지 않고 false(구매 조건: GDD 빚과 파산). 성공하면 OnProgressChanged */
	UFUNCTION(BlueprintCallable)
	bool TryUnlockNext();

	/** 다음 찜기 레벨의 수치 · 비용. 최대 레벨이면 false */
	UFUNCTION(BlueprintPure)
	bool GetNextSteamerLevel(FSteamerRow& OutRow) const;

	/** 찜기를 한 단계 올린다. 실패 조건 · 성공 처리는 TryUnlockNext와 같다 */
	UFUNCTION(BlueprintCallable)
	bool TryUpgradeSteamer();

	/** 전역 상수. 위젯 연출 길이는 PlaceTime · WrapTime · TrashTime에 맞춘다 */
	UFUNCTION(BlueprintPure)
	FBalanceRow GetBalance() const;

	/** 오늘 영업에 필요한 행과 GoldAtStart를 채운다. 외형 수 · 시드는 호출 쪽이 채운다. 일차 행 선택: GDD 하루 진행 · data-schema.md Days */
	void FillDayInput(FBusinessDayInput& OutInput) const;

	/**
	 * 하루 결과를 진행 값에 반영하고 판정을 돌려준다. 결산 때 한 번만 부른다.
	 * GameOver면 진행 값을 새 게임으로 바꾼다(GDD 저장: 게임 오버 확정 즉시 초기화).
	 */
	EDayOutcome ApplyDayResult(const FDayResult& Result);

private:
	/** 손님 줄 화면 슬롯 수(ADR-0009). Days.MaxQueue의 상한 */
	static constexpr int32 QueueSlotCount = 5;

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

	/** 행 이름 = enum 값 이름. 없으면 nullptr(Init 검증 후에는 모든 재료에 있다) */
	const FIngredientRow* FindIngredientRow(EIngredient Ingredient) const;

	/** 행 이름 = 레벨 숫자. 최대 레벨을 넘으면 nullptr */
	const FSteamerRow* FindSteamerRow(int32 Level) const;
};
