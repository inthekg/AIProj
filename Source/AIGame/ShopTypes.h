#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Misc/EnumRange.h"
#include "ShopTypes.generated.h"

/** 재료 종류. 고정 집합이라 종류별 분기는 switch로 쓴다. DataTable 행 이름 = 값 이름 */
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

/** 베이스면 true, 과일이면 false. None은 checkNoEntry() */
bool IsBaseIngredient(EIngredient Ingredient);

/** 작업대 상태 (GDD 제작 상태도) */
UENUM(BlueprintType)
enum class EWorkbenchState : uint8
{
	Empty,
	Base,
	BaseFruit,
	Wrapping,
	Trash
};

/** 하루 결산 판정 */
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

	/** 베이스 재료. 조합마다 1개 */
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

	/** "아무거나" 주문이면 true */
	UPROPERTY(BlueprintReadOnly)
	bool bAnyOrder = false;

	/** 사람 외형 이미지 배열(ABusinessGameMode의 CustomerLookImages) 인덱스 */
	UPROPERTY(BlueprintReadOnly)
	int32 LookIndex = 0;

	/** 인내심 만료 시각(월드 시간). 게이지 = (ExpireTime − 현재 월드 시각) / Patience */
	UPROPERTY(BlueprintReadOnly)
	double ExpireTime = 0.0;
};

/** 진열대 칸 하나 = 조합 하나 */
USTRUCT(BlueprintType)
struct FShelfEntry
{
	GENERATED_BODY()

	/** 이 칸의 조합 */
	UPROPERTY(BlueprintReadOnly)
	FRecipe Recipe;

	/** 1 ~ MaxStockPerRecipe */
	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;
};

/** 하루 집계. 영업 중에는 누적값, 영업 종료 때 결산 결과. 순이익 = Sales − IngredientCost − Rent(따로 두지 않는다) */
USTRUCT(BlueprintType)
struct FDayResult
{
	GENERATED_BODY()

	/** 매출 */
	UPROPERTY(BlueprintReadOnly)
	int32 Sales = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 IngredientCost = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SoldCount = 0;

	/** 인내심이 다해 떠난 손님만(D14) */
	UPROPERTY(BlueprintReadOnly)
	int32 MissedCustomers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TrashCount = 0;

	/** 진열 상한 초과(ADR-0011) + 영업 종료 때 남은 진열 개수 · 작업대 */
	UPROPERTY(BlueprintReadOnly)
	int32 DiscardCount = 0;

	/** 영업 시작 때 낸 임대료 = BaseRent + 해금된 재료의 RentIncrease 합(D17 · D18, ADR-0016 · 0017) */
	UPROPERTY(BlueprintReadOnly)
	int32 Rent = 0;
};

// 데이터 행: 멤버 순서 = CSV 컬럼 순서(CLAUDE.md 코딩 컨벤션). 멤버 주석은 data-schema.md의 '뜻' 열, 제약은 data-schema.md

/** Ingredients.csv 한 줄 */
USTRUCT(BlueprintType)
struct FIngredientRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면 이름(로비의 다음 해금 표시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FText DisplayName;

	/** 해금 순서. 0 = 처음부터 해금 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UnlockOrder = 0;

	/** 해금 비용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UnlockCost = 0;

	/** 작업대에 올리는 순간 차감되는 재료비 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 Cost = 0;

	/** 판매가 계산에 쓰는 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 Price = 0;

	/** 주문 가중치. 베이스는 베이스끼리, 과일은 과일끼리(+ Balance.NoFruitWeight) 비교 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 OrderWeight = 0;

	/** 이 재료를 해금하면 하루 임대료가 이만큼 오른다(ADR-0017) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 RentIncrease = 0;

};

/** Days.csv 한 줄 */
USTRUCT(BlueprintType)
struct FDayRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 영업 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float Duration = 0.f;

	/** 손님 도착 간격 최소(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float ArrivalMin = 0.f;

	/** 손님 도착 간격 최대(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float ArrivalMax = 0.f;

	/** 손님 인내심(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float Patience = 0.f;

	/** 최대 대기 손님 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxQueue = 0;

	/** "아무거나" 주문 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float AnyOrderRate = 0.f;

};

/** Steamer.csv 한 줄 */
USTRUCT(BlueprintType)
struct FSteamerRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 게이지 최대치(떡 개수) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxGauge = 0;

	/** 게이지가 0이 된 뒤 다시 가득 채우는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float RegenTime = 0.f;

	/** 이 레벨로 올리는 비용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 UpgradeCost = 0;

};

/** Balance.csv의 Default 행 */
USTRUCT(BlueprintType)
struct FBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 시작 골드(1일차, 게임 오버 후 초기화) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 StartGold = 0;

	/** 떡 재료비 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 RiceCakeCost = 0;

	/** '과일 없음' 주문 가중치(과일 OrderWeight와 함께 비교) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 NoFruitWeight = 0;

	/** 베이스 + 과일 판매가 혼합 배수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float MixMultiplier = 0.f;

	/** 재료 올리기 연출 = 입력 잠금 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float PlaceTime = 0.f;

	/** 감싸기 연출 = 입력 잠금 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float WrapTime = 0.f;

	/** 쓰레기 연출(버리기 포함) = 입력 잠금 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	float TrashTime = 0.f;

	/** 조합당 진열 상한(ADR-0011) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	int32 MaxStockPerRecipe = 0;

	/** 기본 임대료(팥만 해금한 상태). 하루 임대료 = BaseRent + 해금된 재료의 RentIncrease 합 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseRent = 0;

};
