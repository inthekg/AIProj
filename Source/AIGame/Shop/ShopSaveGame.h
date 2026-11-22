#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ShopProgress.h"
#include "ShopSaveGame.generated.h"

/** 저장 슬롯에 쓰는 SaveVersion과 진행 값을 담는다 */
UCLASS()
class UShopSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 저장 필드를 바꾸면 올린다 */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 SaveVersion = CurrentVersion;

	UPROPERTY()
	FShopProgress Progress;
};
