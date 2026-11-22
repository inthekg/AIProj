#include "BusinessGameMode.h"

#include "AIGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Shop/ShopGameInstance.h"
#include "TimerManager.h"

void ABusinessGameMode::BeginPlay()
{
	// 순서 중요: Super가 BP BeginPlay(위젯 생성 · 델리게이트 구독)를 실행해야 아래 StartDay 뒤의 첫 알림을 위젯이 받는다(Actor.cpp BeginPlay → ReceiveBeginPlay)
	Super::BeginPlay();

	checkf(CustomerLookImages.Num() > 0, TEXT("ABusinessGameMode: assign CustomerLookImages in BP_BusinessGameMode"));
	const UShopGameInstance* ShopGameInstance = GetGameInstance<UShopGameInstance>();
	checkf(ShopGameInstance, TEXT("ABusinessGameMode: Game Instance Class must be a UShopGameInstance (BP_ShopGameInstance)"));

	FBusinessDayInput Input;
	ShopGameInstance->FillDayInput(Input);
	// ADR-0014: 외형 수는 데이터가 아니라 BP 이미지 배열 길이로 정한다
	Input.CustomerLookCount = CustomerLookImages.Num();
	// 게임은 매번 다른 시드(architecture.md 1.6)
	Input.RandomSeed = static_cast<int32>(FDateTime::Now().GetTicks());

	const EBusinessChange StartChanges = Sim.StartDay(Input, GetWorld()->GetTimeSeconds());
	HandleChanges(StartChanges);

	// 내려갈 때 · 돌아올 때 각각 두 델리게이트가 연달아 오므로 한 쌍만 구독한다
	// 둘 다 영업 중일 때만 처리한다
	// TODO(T06): 실기기에서 게임 스레드 호출과, 복귀 첫 프레임의 DeltaTime이 월드 시간에 더해지지 않는지 확인(architecture.md 3.7 · 4.7)
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddWeakLambda(this, [this]()
	{
		UE_LOG(LogAIGame, Log, TEXT("Entering background. DayOver=%s"), Sim.IsDayOver() ? TEXT("true") : TEXT("false"));
		if (!Sim.IsDayOver())
		{
			// 일시정지 중에는 월드 시간과 타이머가 함께 멈춘다
			UGameplayStatics::SetGamePaused(this, true);
		}
	});
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddWeakLambda(this, [this]()
	{
		UE_LOG(LogAIGame, Log, TEXT("Entered foreground. DayOver=%s"), Sim.IsDayOver() ? TEXT("true") : TEXT("false"));
		if (!Sim.IsDayOver())
		{
			OnResumeRequired.Broadcast();
		}
	});
}

void ABusinessGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SimTimerHandle);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ABusinessGameMode::HandleChanges(EBusinessChange Changes)
{
	const double Now = GetWorld()->GetTimeSeconds();
	// StartDay · 입력 뒤에도 ProcessDueEvents(Now)를 거친다
	Changes |= Sim.ProcessDueEvents(Now);

	if (EnumHasAnyFlags(Changes, EBusinessChange::Customers))
	{
		UE_LOG(LogAIGame, Log, TEXT("Customers changed. Now=%.2f Waiting=%d Missed=%d"),
			Now, Sim.GetWaitingCustomers().Num(), Sim.GetResult().MissedCustomers);
		OnCustomersChanged.Broadcast();
	}
	// TODO(T11): Workbench면 OnWorkbenchChanged.Broadcast(Sim.GetWorkbenchState()) — GetWorkbenchState가 T11에서 생긴다
	if (EnumHasAnyFlags(Changes, EBusinessChange::Steamer))
	{
		OnSteamerChanged.Broadcast();
	}
	if (EnumHasAnyFlags(Changes, EBusinessChange::Shelf))
	{
		OnShelfChanged.Broadcast();
	}
	if (EnumHasAnyFlags(Changes, EBusinessChange::Gold))
	{
		OnGoldChanged.Broadcast(Sim.GetCurrentGold());
	}

	if (EnumHasAnyFlags(Changes, EBusinessChange::DayEnded))
	{
		GetWorldTimerManager().ClearTimer(SimTimerHandle);

		const FDayResult& Result = Sim.GetResult();
		const EDayOutcome Outcome = GetGameInstance<UShopGameInstance>()->ApplyDayResult(Result);
		UE_LOG(LogAIGame, Log, TEXT("Day ended. Now=%.2f Sales=%d IngredientCost=%d Rent=%d Missed=%d Outcome=%s"),
			Now, Result.Sales, Result.IngredientCost, Result.Rent, Result.MissedCustomers,
			*StaticEnum<EDayOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome)));
		// 순서 중요: 진행 값 반영 · 저장이 끝난 뒤 알린다
		OnDayEnded.Broadcast(Result, Outcome);
		return;
	}

	// 영업 종료 전이면 DayEnd 이벤트가 힙에 남아 있다. 비어 있으면 GetValue가 checkf로 멈춘다(Optional.h)
	const double Delay = Sim.GetNextEventTime().GetValue() - Now;
	// ADR-0004: 손님별 타이머 대신 힙 맨 앞 시각으로 핸들 하나만 다시 건다
	// ProcessDueEvents(Now)가 Now 이하 이벤트를 모두 꺼냈으므로 Delay > 0이다. 0 이하면 타이머가 등록되지 않는다(TimerManager.cpp InternalSetTimer의 InRate > 0.f)
	// 같은 핸들로 다시 걸면 기존 타이머를 지우고 건다. 이 함수가 타이머 콜백으로 실행 중일 때도 된다(TimerManager.cpp InternalClearTimer의 Executing)
	GetWorldTimerManager().SetTimer(SimTimerHandle,
		FTimerDelegate::CreateUObject(this, &ABusinessGameMode::HandleChanges, EBusinessChange::None), Delay, false);
}
