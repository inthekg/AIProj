#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BusinessSim.h"
#include "BusinessGameMode.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBusinessChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkbenchChanged, EWorkbenchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChanged, int32, CurrentGold);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDayEnded, const FDayResult&, Result, EDayOutcome, Outcome);

/** 영업 레벨에서 FBusinessSim을 월드 시간과 타이머 1개로 구동하고 화면 입력 · 알림을 중계한다 */
UCLASS()
class ABusinessGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnBusinessChanged OnCustomersChanged;

	/** 작업대 상태가 바뀜. Base · BaseFruit면 재료 올리기, Wrapping이면 감싸기, Trash면 쓰레기 연출 시작 — 길이는 GetBalance()의 PlaceTime · WrapTime · TrashTime */
	UPROPERTY(BlueprintAssignable)
	FOnWorkbenchChanged OnWorkbenchChanged;

	UPROPERTY(BlueprintAssignable)
	FOnBusinessChanged OnSteamerChanged;

	/** 진열대가 바뀜. 칸 i ← GetShelf()[i] */
	UPROPERTY(BlueprintAssignable)
	FOnBusinessChanged OnShelfChanged;

	UPROPERTY(BlueprintAssignable)
	FOnGoldChanged OnGoldChanged;

	/** 영업 종료*/
	UPROPERTY(BlueprintAssignable)
	FOnDayEnded OnDayEnded;

	/** 앱이 백그라운드에서 돌아옴(영업 중일 때만). 게임은 일시정지*/
	UPROPERTY(BlueprintAssignable)
	FOnBusinessChanged OnResumeRequired;

	/** 재료통 탭 */
	UFUNCTION(BlueprintCallable)
	void TapIngredient(EIngredient Ingredient);

	/** 찜기 탭 */
	UFUNCTION(BlueprintCallable)
	void TapSteamer();

	/** 작업대 버리기 버튼 */
	UFUNCTION(BlueprintCallable)
	void TapDiscard();

	UFUNCTION(BlueprintPure)
	TArray<FCustomer> GetWaitingCustomers() const { return Sim.GetWaitingCustomers(); }

	UFUNCTION(BlueprintPure)
	TArray<FShelfEntry> GetShelf() const { return Sim.GetShelf(); }

	UFUNCTION(BlueprintPure)
	FRecipe GetWorkbenchRecipe() const { return Sim.GetWorkbenchRecipe(); }

	UFUNCTION(BlueprintPure)
	int32 GetSteamerGauge() const { return Sim.GetSteamerGauge(); }

	UFUNCTION(BlueprintPure)
	int32 GetSteamerMaxGauge() const { return Sim.GetInput().Steamer.MaxGauge; }

	/** 다시 찌는 중이 아니면 0 */
	UFUNCTION(BlueprintPure)
	double GetSteamerRefillEndTime() const { return Sim.GetSteamerRefillEndTime(); }

	/** 남은 시간 = 이 값 − 현재 월드 시각(Get Game Time in Seconds) */
	UFUNCTION(BlueprintPure)
	double GetDayEndTime() const { return Sim.GetDayEndTime(); }

	/** 인내심 게이지의 분모 */
	UFUNCTION(BlueprintPure)
	float GetPatience() const { return Sim.GetInput().Day.Patience; }

	/** 영업 중 누적 집계(좌상단 매출) */
	UFUNCTION(BlueprintPure)
	FDayResult GetDayResult() const { return Sim.GetResult(); }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentGold() const { return Sim.GetCurrentGold(); }

protected:
	/** BP BeginPlay(위젯 생성 · 구독)를 먼저 실행한 뒤 하루를 시작한다. 그래서 첫 알림을 위젯이 받는다 */
	virtual void BeginPlay() override;

	/** 타이머와 백그라운드 · 복귀 구독을 해제한다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 사람 외형 이미지. 개수 = 외형 수, 1개 이상(ADR-0014) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UTexture2D>> CustomerLookImages;

	FBusinessSim Sim;
	FTimerHandle SimTimerHandle;

	/**
	 * 모든 Sim 호출 뒤에 부른다. ProcessDueEvents(Now)로 Now 이하 이벤트를 마저 처리하고 바뀐 부분의 델리게이트를 보낸 뒤,
	 * 영업 종료면 결산(UShopGameInstance::ApplyDayResult → OnDayEnded), 아니면 다음 이벤트 시각으로 타이머를 다시 건다.
	 * 영업이 이미 끝난 뒤의 입력(architecture.md 4.2)이면 로그만 남긴다.
	 * 타이머 콜백으로도 쓴다(Changes = None을 payload로 바인딩).
	 */
	void HandleChanges(EBusinessChange Changes);
};
