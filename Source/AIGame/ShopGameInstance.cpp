#include "ShopGameInstance.h"

#include "AIGame.h"
#include "BusinessSim.h"
#include "Engine/DataTable.h"

void UShopGameInstance::Init()
{
	Super::Init();

	// 데이터 검증(data-schema.md '데이터 검증'). 입력 경계에서 한 번만 검사하므로 이후 조회는 행을 다시 확인하지 않는다
	// 검증 · 초기화는 Init에서만 쓰므로 함수로 나누지 않는다(architecture.md 3.5)
	checkf(IngredientTable && DayTable && SteamerTable && BalanceTable,
		TEXT("UShopGameInstance: assign all four DataTables in BP_ShopGameInstance"));

	const FBalanceRow Balance = GetBalance();
	checkf(Balance.BaseRent < Balance.StartGold,
		TEXT("Balance: BaseRent (%d) must be less than StartGold (%d)"), Balance.BaseRent, Balance.StartGold);

	TArray<int32> UnlockOrders;
	for (const EIngredient& Ingredient : TEnumRange<EIngredient>())
	{
		const FIngredientRow* Row = FindIngredientRow(Ingredient);
		checkf(Row, TEXT("Ingredients: missing row for EIngredient value %d (see LogDataTable warning)"), static_cast<int32>(Ingredient));
		checkf(Row->RentIncrease >= 0,
			TEXT("Ingredients value %d: RentIncrease (%d) must be >= 0"), static_cast<int32>(Ingredient), Row->RentIncrease);
		// FShopProgress::TrySpend의 전제(Cost > 0, architecture.md 3.3)를 데이터 경계에서 지킨다. UnlockOrder 0은 사지 않는다
		checkf(Row->UnlockOrder == 0 || Row->UnlockCost > 0,
			TEXT("Ingredients value %d: UnlockCost (%d) must be > 0"), static_cast<int32>(Ingredient), Row->UnlockCost);
		UnlockOrders.Add(Row->UnlockOrder);
	}
	// 정렬한 뒤 [i] == i이면 0 ~ (재료 수 − 1)이 빠짐 · 중복 없이 하나씩 있다
	UnlockOrders.Sort();
	for (int32 Index = 0; Index < UnlockOrders.Num(); ++Index)
	{
		checkf(UnlockOrders[Index] == Index,
			TEXT("Ingredients: UnlockOrder must be 0..%d without gaps or duplicates"), UnlockOrders.Num() - 1);
	}

	// 행 이름이 "1" ~ 행 수로 빠짐없어야 FillDayInput의 min(Day, 행 수) 조회와 '최대 레벨 = 행 수'가 성립한다
	const int32 DayRowCount = DayTable->GetRowNames().Num();
	checkf(DayRowCount > 0, TEXT("Days: no rows"));
	for (int32 DayNumber = 1; DayNumber <= DayRowCount; ++DayNumber)
	{
		const FDayRow* Row = DayTable->FindRow<FDayRow>(FName(*FString::FromInt(DayNumber)), TEXT("UShopGameInstance::Init"));
		checkf(Row, TEXT("Days: rows must be named 1..%d without gaps (missing %d)"), DayRowCount, DayNumber);
		checkf(Row->MaxQueue >= 1 && Row->MaxQueue <= QueueSlotCount,
			TEXT("Days %d: MaxQueue (%d) must be 1..%d"), DayNumber, Row->MaxQueue, QueueSlotCount);
	}

	const int32 SteamerRowCount = SteamerTable->GetRowNames().Num();
	checkf(SteamerRowCount > 0, TEXT("Steamer: no rows"));
	for (int32 Level = 1; Level <= SteamerRowCount; ++Level)
	{
		const FSteamerRow* Row = FindSteamerRow(Level);
		checkf(Row, TEXT("Steamer: rows must be named 1..%d without gaps (missing %d)"), SteamerRowCount, Level);
		// TrySpend의 전제(Cost > 0). 레벨 1은 사지 않는다
		checkf(Level == 1 || Row->UpgradeCost > 0,
			TEXT("Steamer %d: UpgradeCost (%d) must be > 0"), Level, Row->UpgradeCost);
	}

	// TODO(T05): 슬롯 불러오기(없거나 SaveVersion이 다르면 새 게임) · 백그라운드 저장 구독
	Progress = FShopProgress::MakeNew(Balance.StartGold);
	UE_LOG(LogAIGame, Log, TEXT("Data validation passed. Day=%d Gold=%d UnlockStep=%d SteamerLevel=%d"),
		Progress.Day, Progress.Gold, Progress.UnlockStep, Progress.SteamerLevel);
}

void UShopGameInstance::GetUnlockedIngredients(TArray<EIngredient>& OutBases, TArray<EIngredient>& OutFruits) const
{
	OutBases.Reset();
	OutFruits.Reset();
	for (const EIngredient& Ingredient : TEnumRange<EIngredient>())
	{
		// 해금 판정: data-schema.md Ingredients (UnlockOrder ≤ UnlockStep)
		if (FindIngredientRow(Ingredient)->UnlockOrder > Progress.UnlockStep)
		{
			continue;
		}
		if (IsBaseIngredient(Ingredient))
		{
			OutBases.Add(Ingredient);
		}
		else
		{
			OutFruits.Add(Ingredient);
		}
	}

	// enum 순서와 해금 순서가 다르므로 UnlockOrder로 정렬한다. 원소는 재료 수 이하
	const auto ByUnlockOrder = [this](const EIngredient A, const EIngredient B)
	{
		return FindIngredientRow(A)->UnlockOrder < FindIngredientRow(B)->UnlockOrder;
	};
	OutBases.Sort(ByUnlockOrder);
	OutFruits.Sort(ByUnlockOrder);
}

bool UShopGameInstance::GetNextUnlock(EIngredient& OutIngredient, FIngredientRow& OutRow) const
{
	// 다음 해금은 하나뿐이다(GDD 재료와 조합)
	for (const EIngredient& Ingredient : TEnumRange<EIngredient>())
	{
		const FIngredientRow* Row = FindIngredientRow(Ingredient);
		if (Row->UnlockOrder == Progress.UnlockStep + 1)
		{
			OutIngredient = Ingredient;
			OutRow = *Row;
			return true;
		}
	}
	return false;
}

bool UShopGameInstance::TryUnlockNext()
{
	EIngredient NextIngredient = EIngredient::None;
	FIngredientRow NextRow;
	if (!GetNextUnlock(NextIngredient, NextRow))
	{
		return false;
	}
	if (!Progress.TrySpend(NextRow.UnlockCost))
	{
		return false;
	}
	++Progress.UnlockStep;
	// TODO(T05): SaveProgress — 저장한 뒤 알린다(architecture.md 3.5)
	OnProgressChanged.Broadcast();
	return true;
}

bool UShopGameInstance::GetNextSteamerLevel(FSteamerRow& OutRow) const
{
	const FSteamerRow* Row = FindSteamerRow(Progress.SteamerLevel + 1);
	if (!Row)
	{
		return false;
	}
	OutRow = *Row;
	return true;
}

bool UShopGameInstance::TryUpgradeSteamer()
{
	FSteamerRow NextRow;
	if (!GetNextSteamerLevel(NextRow))
	{
		return false;
	}
	if (!Progress.TrySpend(NextRow.UpgradeCost))
	{
		return false;
	}
	++Progress.SteamerLevel;
	// TODO(T05): SaveProgress — 저장한 뒤 알린다(architecture.md 3.5)
	OnProgressChanged.Broadcast();
	return true;
}

FBalanceRow UShopGameInstance::GetBalance() const
{
	const FBalanceRow* Row = BalanceTable->FindRow<FBalanceRow>(FName(TEXT("Default")), TEXT("UShopGameInstance::GetBalance"));
	checkf(Row, TEXT("Balance: missing row 'Default'"));
	return *Row;
}

void UShopGameInstance::FillDayInput(FBusinessDayInput& OutInput) const
{
	// 마지막 행 이후 일차는 마지막 행을 쓴다(GDD 하루 진행). 행이 "1" ~ 행 수로 빠짐없는지는 Init에서 검증했다
	const int32 DayNumber = FMath::Min(Progress.Day, DayTable->GetRowNames().Num());
	OutInput.Day = *DayTable->FindRow<FDayRow>(FName(*FString::FromInt(DayNumber)), TEXT("UShopGameInstance::FillDayInput"));
	OutInput.Steamer = *FindSteamerRow(Progress.SteamerLevel);
	OutInput.Balance = GetBalance();

	TArray<EIngredient> Bases;
	TArray<EIngredient> Fruits;
	GetUnlockedIngredients(Bases, Fruits);
	OutInput.Ingredients.Reset();
	for (const EIngredient& Base : Bases)
	{
		OutInput.Ingredients.Add(Base, *FindIngredientRow(Base));
	}
	for (const EIngredient& Fruit : Fruits)
	{
		OutInput.Ingredients.Add(Fruit, *FindIngredientRow(Fruit));
	}

	OutInput.GoldAtStart = Progress.Gold;
}

EDayOutcome UShopGameInstance::ApplyDayResult(const FDayResult& Result)
{
	const EDayOutcome Outcome = Progress.ApplyDayResult(Result);
	if (Outcome == EDayOutcome::GameOver)
	{
		Progress = FShopProgress::MakeNew(GetBalance().StartGold);
	}
	// TODO(T05): SaveProgress — 결산 확정 직후 저장, GameOver면 초기화한 값을 저장한다(GDD 저장)
	return Outcome;
}

const FIngredientRow* UShopGameInstance::FindIngredientRow(EIngredient Ingredient) const
{
	// enum class는 "EIngredient::" 접두사를 뗀 값 이름을 돌려준다(Enum.cpp UEnum::GetNameStringByIndex)
	const FName RowName(*StaticEnum<EIngredient>()->GetNameStringByValue(static_cast<int64>(Ingredient)));
	return IngredientTable->FindRow<FIngredientRow>(RowName, TEXT("UShopGameInstance::FindIngredientRow"));
}

const FSteamerRow* UShopGameInstance::FindSteamerRow(int32 Level) const
{
	// 최대 레벨을 넘는 조회(GetNextSteamerLevel)는 정상 경우라 행이 없다는 경고를 끈다(DataTable.h FindRow의 bWarnIfRowMissing)
	return SteamerTable->FindRow<FSteamerRow>(FName(*FString::FromInt(Level)), TEXT("UShopGameInstance::FindSteamerRow"), false);
}
