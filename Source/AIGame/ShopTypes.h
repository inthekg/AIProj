#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Misc/EnumRange.h"
#include "ShopTypes.generated.h"

/** 값 이름 = Ingredients DataTable 행 이름. None은 재료 없음(행 없음) */
UENUM(BlueprintType)
enum class EIngredient : uint8
{
	None,
	RedBean,
	Yogurt,
	DubaiChoco,
	Strawberry,
	Blueberry,
	Pineapple,
	Mango
};
ENUM_RANGE_BY_FIRST_AND_LAST(EIngredient, EIngredient::RedBean, EIngredient::Mango);

/** 전제: Ingredient != None (None이면 checkNoEntry) */
bool IsBaseIngredient(EIngredient Ingredient);

/** 상태 정의: GDD 제작 상태도 */
UENUM(BlueprintType)
enum class EWorkbenchState : uint8
{
	Empty,
	Base,
	BaseFruit,
	Wrapping,
	Trash
};

/** 판정 규칙: GDD 빚과 파산 */
UENUM(BlueprintType)
enum class EDayOutcome : uint8
{
	Normal,
	DebtWarning,
	GameOver
};

/** 모찌 조합 = 베이스 1 + 과일 0~1. 주문과 완성품이 같은 형태다 */
USTRUCT(BlueprintType)
struct FRecipe
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EIngredient Base = EIngredient::None;

	/** None이면 과일 없음 */
	UPROPERTY(BlueprintReadOnly)
	EIngredient Fruit = EIngredient::None;

	bool operator==(const FRecipe& Other) const { return Base == Other.Base && Fruit == Other.Fruit; }
};

/** 대기 중인 손님 한 명 */
USTRUCT(BlueprintType)
struct FCustomer
{
	GENERATED_BODY()

	/** 1부터 하루 동안 증가. 힙 이벤트가 이 값으로 손님을 찾는다(ADR-0009) */
	UPROPERTY(BlueprintReadOnly)
	int32 Id = 0;

	/** bAnyOrder == true 면 쓰지 않는다 */
	UPROPERTY(BlueprintReadOnly)
	FRecipe Order;

	UPROPERTY(BlueprintReadOnly)
	bool bAnyOrder = false;

	/** 사람 외형 이미지 배열(ABusinessGameMode의 CustomerLookImages) 인덱스 */
	UPROPERTY(BlueprintReadOnly)
	int32 LookIndex = 0;

	/** 인내심 만료 시각. 월드 시간(초, GetTimeSeconds 기준) */
	UPROPERTY(BlueprintReadOnly)
	double ExpireTime = 0.0;
};

/** 진열대 칸 하나 = 조합 하나 */
USTRUCT(BlueprintType)
struct FShelfEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FRecipe Recipe;

	/** 1 ~ MaxStockPerRecipe */
	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;
};

/** 하루 집계. 영업 중에는 누적값, 영업 종료 때 결산 결과 */
USTRUCT(BlueprintType)
struct FDayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Sales = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 IngredientCost = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SoldCount = 0;

	/** 집계 기준: ADR-0015 D14 */
	UPROPERTY(BlueprintReadOnly)
	int32 MissedCustomers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TrashCount = 0;

	/** 집계 기준: ADR-0011, GDD 하루 진행 */
	UPROPERTY(BlueprintReadOnly)
	int32 DiscardCount = 0;

	/** 계산: ADR-0016 · ADR-0017 */
	UPROPERTY(BlueprintReadOnly)
	int32 Rent = 0;
};

// 데이터 행: 멤버 이름 · 순서 = Data/*.csv 열. 열의 뜻 · 제약은 data-schema.md

/** Ingredients.csv 한 줄 */
USTRUCT(BlueprintType)
struct FIngredientRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FText DisplayName;

	/** 0 = 처음부터 해금 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UnlockOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UnlockCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 Cost = 0;

	/** 조합 판매가 계산의 입력값(공식: GDD 경제) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 Price = 0;

	/** 과일 행은 FBalanceRow::NoFruitWeight와 함께 비교한다(GDD 손님) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 OrderWeight = 0;

	/** 골드/일. 계산: ADR-0017 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 RentIncrease = 0;

};

/** Days.csv 한 줄 */
USTRUCT(BlueprintType)
struct FDayRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float Duration = 0.f;

	/** 초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float ArrivalMin = 0.f;

	/** 초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float ArrivalMax = 0.f;

	/** 초 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float Patience = 0.f;

	/** 1~5 (화면 슬롯 수, ADR-0009) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxQueue = 0;

	/** 0~1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float AnyOrderRate = 0.f;

};

/** Steamer.csv 한 줄 */
USTRUCT(BlueprintType)
struct FSteamerRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 떡 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxGauge = 0;

	/** 초 (GDD 찜기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float RegenTime = 0.f;

	/** 이 행의 레벨로 올리는 비용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UpgradeCost = 0;

};

/** Balance.csv의 Default 행 */
USTRUCT(BlueprintType)
struct FBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 StartGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 RiceCakeCost = 0;

	/** '과일 없음' 가중치. 과일 행의 FIngredientRow::OrderWeight와 함께 비교한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 NoFruitWeight = 0;

	/** 조합 판매가 배수(공식: GDD 경제) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float MixMultiplier = 0.f;

	/** 초. 재료 올리기 연출 길이 = 입력 잠금 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float PlaceTime = 0.f;

	/** 초. 감싸기 연출 길이 = 입력 잠금 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float WrapTime = 0.f;

	/** 초. 쓰레기 · 버리기 연출 길이 = 입력 잠금 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float TrashTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxStockPerRecipe = 0;

	/** 계산: ADR-0016 · ADR-0017 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseRent = 0;

};
