#include "ShopGameInstance.h"

#include "AIGame.h"
#include "Business/BusinessSim.h"
#include "Engine/DataTable.h"

void UShopGameInstance::Init()
{
	Super::Init();

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
		checkf(Row->UnlockOrder == 0 || Row->UnlockCost > 0,
			TEXT("Ingredients value %d: UnlockCost (%d) must be > 0"), static_cast<int32>(Ingredient), Row->UnlockCost);
		UnlockOrders.Add(Row->UnlockOrder);
	}

	UnlockOrders.Sort();
	for (int32 Index = 0; Index < UnlockOrders.Num(); ++Index)
	{
		checkf(UnlockOrders[Index] == Index,
			TEXT("Ingredients: UnlockOrder must be 0..%d without gaps or duplicates"), UnlockOrders.Num() - 1);
	}

	//Day별 영업 데이터 확인
	const int32 DayRowCount = DayTable->GetRowNames().Num();
	checkf(DayRowCount > 0, TEXT("Days: no rows"));
	for (int32 DayNumber = 1; DayNumber <= DayRowCount; ++DayNumber)
	{
		const FDayRow* Row = DayTable->FindRow<FDayRow>(FName(*FString::FromInt(DayNumber)), TEXT("UShopGameInstance::Init"));
		checkf(Row, TEXT("Days: rows must be named 1..%d without gaps (missing %d)"), DayRowCount, DayNumber);
		checkf(Row->MaxQueue >= 1 && Row->MaxQueue <= QueueSlotCount,
			TEXT("Days %d: MaxQueue (%d) must be 1..%d"), DayNumber, Row->MaxQueue, QueueSlotCount);
	}

	//찜기 데이터 확인
	const int32 SteamerRowCount = SteamerTable->GetRowNames().Num();
	checkf(SteamerRowCount > 0, TEXT("Steamer: no rows"));
	for (int32 Level = 1; Level <= SteamerRowCount; ++Level)
	{
		const FSteamerRow* Row = FindSteamerRow(Level);
		checkf(Row, TEXT("Steamer: rows must be named 1..%d without gaps (missing %d)"), SteamerRowCount, Level);
		checkf(Level == 1 || Row->UpgradeCost > 0,
			TEXT("Steamer %d: UpgradeCost (%d) must be > 0"), Level, Row->UpgradeCost);
	}

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
	OnProgressChanged.Broadcast();
	return true;
}

FBalanceRow UShopGameInstance::GetBalance() const
{
	const FBalanceRow* Row = BalanceTable->FindRow<FBalanceRow>(FName(BalanceRowName), TEXT("UShopGameInstance::GetBalance"));
	checkf(Row, TEXT("Balance: missing row '%s'"), BalanceRowName);
	return *Row;
}

void UShopGameInstance::FillDayInput(FBusinessDayInput& OutInput) const
{
	// 마지막 행 이후 일차는 마지막 행을 쓴다
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
	return Outcome;
}

const FIngredientRow* UShopGameInstance::FindIngredientRow(EIngredient Ingredient) const
{
	const FName RowName(*StaticEnum<EIngredient>()->GetNameStringByValue(static_cast<int64>(Ingredient)));
	return IngredientTable->FindRow<FIngredientRow>(RowName, TEXT("UShopGameInstance::FindIngredientRow"));
}

const FSteamerRow* UShopGameInstance::FindSteamerRow(int32 Level) const
{
	return SteamerTable->FindRow<FSteamerRow>(FName(*FString::FromInt(Level)), TEXT("UShopGameInstance::FindSteamerRow"), false);
}
