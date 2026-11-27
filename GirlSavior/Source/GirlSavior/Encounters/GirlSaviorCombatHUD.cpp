#include "Encounters/GirlSaviorCombatHUD.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorDashSettings.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Components/Button.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCombatHUD"

void AGirlSaviorCombatHUD::ReleasePresentation()
{
    UnbindEncounter();
    RemovePresentation();
}

namespace
{
const FLinearColor DashUnavailableColor(0.55f, 0.61f, 0.65f);
const FLinearColor ReadyColor(0.74f, 0.84f, 0.78f);
const FLinearColor ProtectedColor(0.35f, 0.85f, 0.96f);
const FLinearColor CooldownColor(0.93f, 0.64f, 0.38f);

FText GetDefeatCause(const AGirlSaviorCombatEncounter& Encounter)
{
    const FText& Cause = Encounter.GetDefeatResult().Spec.Cause;
    return Cause.IsEmpty() ? LOCTEXT("UnknownCause", "Смертельное попадание") : Cause;
}
}

void AGirlSaviorCombatHUD::BeginPlay()
{
    Super::BeginPlay();
    const auto* Controller = GetOwningPlayerController();
    if (!Controller || !Controller->GetLocalPlayer())
    {
        return;
    }
    if (!Encounter.IsValid())
    {
        // Isolated fixture maps still use the native widgets without a feedback presenter.
        for (TActorIterator<AGirlSaviorCombatEncounter> It(GetWorld()); It; ++It)
        {
            ConfigurePresentation(**It, nullptr, nullptr);
            break;
        }
    }
}

void AGirlSaviorCombatHUD::ConfigurePresentation(AGirlSaviorCombatEncounter& InEncounter,
    TSubclassOf<UGirlSaviorCombatVitalsWidget> InVitalsClass,
    TSubclassOf<UGirlSaviorCombatOutcomeWidget> InOutcomeClass)
{
    UClass* ResolvedVitalsClass = InVitalsClass ? InVitalsClass.Get() : UGirlSaviorCombatVitalsWidget::StaticClass();
    UClass* ResolvedOutcomeClass = InOutcomeClass ? InOutcomeClass.Get() : UGirlSaviorCombatOutcomeWidget::StaticClass();
    if (Encounter.Get() == &InEncounter && VitalsClass.Get() == ResolvedVitalsClass
        && OutcomeClass.Get() == ResolvedOutcomeClass && VitalsWidget && OutcomeWidget)
    {
        return;
    }
    UnbindEncounter();
    Encounter = &InEncounter;
    VitalsClass = ResolvedVitalsClass;
    OutcomeClass = ResolvedOutcomeClass;
    InEncounter.OnHeroChanged.AddUObject(this, &ThisClass::BindHero);
    InEncounter.OnRunStateChanged.AddUObject(this, &ThisClass::HandleRunStateChanged);
    RebuildPresentation();
}

void AGirlSaviorCombatHUD::RebuildPresentation()
{
    RemovePresentation();
    APlayerController* Controller = GetOwningPlayerController();
    const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
    if (!LocalPlayer || !LocalPlayer->ViewportClient)
    {
        return;
    }
    VitalsWidget = CreateWidget<UGirlSaviorCombatVitalsWidget>(Controller, VitalsClass);
    OutcomeWidget = CreateWidget<UGirlSaviorCombatOutcomeWidget>(Controller, OutcomeClass);
    if (!VitalsWidget || !OutcomeWidget)
    {
        RemovePresentation();
        return;
    }
    VitalsWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    OutcomeWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    VitalsWidget->AddToPlayerScreen(20);
    OutcomeWidget->AddToPlayerScreen(30);
    OutcomeWidget->OnRetryRequested.AddUObject(this, &ThisClass::HandleRetry);

    BindHero(Encounter->GetHero());
    HandleRunStateChanged(Encounter->GetRunState());

    // Retry reuses the game viewport. Return focus there once the new HUD exists.
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::FocusGameplayViewport));
}

void AGirlSaviorCombatHUD::RemovePresentation()
{
    if (VitalsWidget)
    {
        VitalsWidget->RemoveFromParent();
        VitalsWidget = nullptr;
    }
    if (OutcomeWidget)
    {
        OutcomeWidget->OnRetryRequested.RemoveAll(this);
        OutcomeWidget->RemoveFromParent();
        OutcomeWidget = nullptr;
    }
}

void AGirlSaviorCombatHUD::UnbindEncounter()
{
    if (AGirlSaviorCombatEncounter* Current = Encounter.Get())
    {
        Current->OnHeroChanged.RemoveAll(this);
        Current->OnRunStateChanged.RemoveAll(this);
    }
    Encounter.Reset();
    BindHero(nullptr);
}

// Health and barrier are sampled once on possession, then updated together by the committed damage event.
void AGirlSaviorCombatHUD::BindHero(AGirlSaviorCharacter* Hero)
{
    if (auto* Previous = BoundHero.Get())
    {
        Previous->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::HandleMovementModeChanged);
    }
    if (auto* PreviousReceiver = Receiver.Get())
    {
        PreviousReceiver->OnDamageResolved.RemoveDynamic(this, &ThisClass::HandleDamageResolved);
        PreviousReceiver->OnBarrierChanged.RemoveAll(this);
    }
    if (auto* PreviousFire = FireComponent.Get())
    {
        PreviousFire->OnWeaponChanged.RemoveAll(this);
    }
    BoundHero = Hero;
    Receiver = Hero ? Hero->GetDamageReceiver() : nullptr;
    FireComponent = Hero ? Hero->GetFireComponent() : nullptr;
    if (auto* CurrentFire = FireComponent.Get())
    {
        CurrentFire->OnWeaponChanged.AddUObject(this, &ThisClass::HandleWeaponChanged);
    }
    if (Hero)
    {
        Hero->MovementModeChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleMovementModeChanged);
    }
    auto* Current = Receiver.Get();
    if (Current)
    {
        Current->OnDamageResolved.AddUniqueDynamic(this, &ThisClass::HandleDamageResolved);
        Current->OnBarrierChanged.AddUObject(this, &ThisClass::HandleBarrierChanged);
    }
    if (VitalsWidget)
    {
        VitalsWidget->SetVisibility(Current ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (Current)
        {
            VitalsWidget->SetVitals(Current->GetHealth(), Current->GetMaxHealth(),
                Current->GetBarrier(), Current->GetMaxBarrier());
        }
        else
        {
            VitalsWidget->SetVitals(0.0, 0.0, 0.0, 0.0);
        }
    }
    DashReadout = EDashReadout::Unset;
    CacheDashDeadline();
    RefreshDash();
    RefreshWeaponView();
}

// Sample after binding so the HUD reflects a configured hero before the first switch.
void AGirlSaviorCombatHUD::RefreshWeaponView()
{
    if (VitalsWidget)
    {
        const auto* Fire = FireComponent.Get();
        VitalsWidget->SetWeaponView(Fire && Fire->bUsesExpeditionWeapons ? Fire->GetWeaponView() : FGirlSaviorWeaponView{});
    }
}

void AGirlSaviorCombatHUD::HandleWeaponChanged(int32 ActiveIndex)
{
    RefreshWeaponView();
}

void AGirlSaviorCombatHUD::HandleDamageResolved(const FGirlSaviorDamageResult& Result)
{
    if (VitalsWidget && Receiver.IsValid() && Result.Outcome == EGirlSaviorDamageOutcome::Applied)
    {
        VitalsWidget->SetVitals(Result.HealthAfter, Receiver->GetMaxHealth(),
            Result.BarrierAfter, Receiver->GetMaxBarrier());
    }
}

// Recharge and grant removal publish a vitals update without a new hit.
void AGirlSaviorCombatHUD::HandleBarrierChanged()
{
    if (VitalsWidget && Receiver.IsValid())
    {
        VitalsWidget->SetVitals(Receiver->GetHealth(), Receiver->GetMaxHealth(),
            Receiver->GetBarrier(), Receiver->GetMaxBarrier());
    }
}

void AGirlSaviorCombatHUD::HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
    // Dash writes its cooldown deadline before entering the custom movement mode.
    CacheDashDeadline();
    RefreshDash();
}

void AGirlSaviorCombatHUD::CacheDashDeadline()
{
    const auto* Hero = BoundHero.Get();
    const auto* Move = Hero ? Cast<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement()) : nullptr;
    DashReadyTime = Move ? GetWorld()->GetTimeSeconds() + Move->GetDashCooldownRemaining() : 0.0;
}

void AGirlSaviorCombatHUD::RefreshDash()
{
    if (!VitalsWidget)
    {
        return;
    }
    const auto* Hero = BoundHero.Get();
    const auto* Move = Hero ? Cast<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement()) : nullptr;
    const auto* Controller = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
    EDashReadout State = EDashReadout::Unavailable;
    int32 RemainingTenths = 0;
    if (Move && Controller && Encounter.IsValid() && Encounter->GetRunState() == EGirlSaviorRunState::Active
        && Receiver.IsValid() && Receiver->IsAlive() && Controller->CanUseGameplayInput()
        && !Controller->IsInSafeArea() && !Controller->IsMoveInputIgnored())
    {
        if (Move->IsDashing())
        {
            State = Move->IsDashProtected() ? EDashReadout::Protected : EDashReadout::Dashing;
        }
        else if (Move->MovementMode == MOVE_Walking && Move->DashSettings && Move->DashSettings->IsValidForDash())
        {
            const double Remaining = DashReadyTime - GetWorld()->GetTimeSeconds();
            if (Remaining > 0.0)
            {
                State = EDashReadout::Cooldown;
                RemainingTenths = FMath::CeilToInt32(FMath::Min(Remaining * 10.0, double(MAX_int32)));
            }
            else
            {
                State = EDashReadout::Ready;
            }
        }
    }
    if (DashReadout == State && DashCooldownTenths == RemainingTenths)
    {
        return;
    }
    DashReadout = State;
    DashCooldownTenths = RemainingTenths;
    switch (State)
    {
    case EDashReadout::Ready:
        VitalsWidget->SetDash(LOCTEXT("DashReady", "Рывок готов · Left Shift"), ReadyColor);
        break;
    case EDashReadout::Protected:
        VitalsWidget->SetDash(LOCTEXT("DashProtected", "Защита от прямых атак"), ProtectedColor);
        break;
    case EDashReadout::Dashing:
        VitalsWidget->SetDash(LOCTEXT("Dashing", "Рывок"), ReadyColor);
        break;
    case EDashReadout::Cooldown:
    {
        FNumberFormattingOptions Format;
        Format.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
        VitalsWidget->SetDash(FText::Format(LOCTEXT("DashCooldown", "Рывок через {0} с"),
            FText::AsNumber(RemainingTenths / 10.0, &Format)), CooldownColor);
        break;
    }
    default:
        VitalsWidget->SetDash(LOCTEXT("DashUnavailable", "Рывок недоступен"), DashUnavailableColor);
        break;
    }
}

void AGirlSaviorCombatHUD::HandleRunStateChanged(EGirlSaviorRunState State)
{
    if (OutcomeWidget && Encounter.IsValid())
    {
        if (State == EGirlSaviorRunState::Defeated)
        {
            OutcomeWidget->ShowDeathNotice(GetDefeatCause(*Encounter.Get()));
        }
        else if (State != EGirlSaviorRunState::Succeeded)
        {
            OutcomeWidget->ClearResult();
        }
        RefreshOutcome();
    }
    RefreshDash();
}

void AGirlSaviorCombatHUD::RefreshOutcome()
{
    if (!OutcomeWidget || !Encounter.IsValid() || OutcomeWidget->IsOutcomePanelVisible()
        || !Encounter->IsOutcomeVisible())
    {
        return;
    }
    const auto State = Encounter->GetRunState();
    const FText Action = Encounter->IsCampOrigin()
        ? (Encounter->CanReturnToCamp() ? LOCTEXT("ReturnToCamp", "В лагерь") : FText::GetEmpty())
        : LOCTEXT("Retry", "Повторить");
    if (State == EGirlSaviorRunState::Recovery)
    {
        OutcomeWidget->ShowRecovery(Encounter->GetRecoveryMessage(), Action);
    }
    else
    {
        const bool bDefeated = State == EGirlSaviorRunState::Defeated;
        OutcomeWidget->ShowResult(bDefeated, bDefeated ? GetDefeatCause(*Encounter.Get()) : FText::GetEmpty(), Action);
    }
    FocusOutcome();
}

// The visible action owns focus. A token without a trusted return still has a diagnostic panel.
void AGirlSaviorCombatHUD::FocusOutcome()
{
    if (APlayerController* Controller = GetOwningPlayerController())
    {
        Controller->bShowMouseCursor = true;
        UWidget* FocusWidget = OutcomeWidget->GetDisplayedActionLabel().IsEmpty()
            ? static_cast<UWidget*>(OutcomeWidget.Get()) : OutcomeWidget->GetRetryButton();
        OutcomeWidget->SetIsFocusable(true);
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(FocusWidget->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Controller->SetInputMode(Mode);
        FocusWidget->SetUserFocus(Controller);
    }
}

// A failed synchronous return leaves the same result, action and focus in place.
void AGirlSaviorCombatHUD::HandleRetry()
{
    if (Encounter.IsValid() && OutcomeWidget)
    {
        FText Error;
        const bool bRequested = Encounter->IsCampOrigin() ? Encounter->ReturnToCamp(Error) : Encounter->Retry();
        if (!bRequested)
        {
            OutcomeWidget->SetActionFailure(Error);
            FocusOutcome();
        }
    }
}

void AGirlSaviorCombatHUD::FocusGameplayViewport()
{
    const auto* Controller = GetOwningPlayerController();
    const auto* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
    const auto* Viewport = Player ? Player->ViewportClient.Get() : nullptr;
    const TSharedPtr<SViewport> GameWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    if (FSlateApplication::IsInitialized() && GameWidget && Encounter.IsValid()
        && (!OutcomeWidget || !OutcomeWidget->IsOutcomePanelVisible())
        && Encounter->GetRunState() == EGirlSaviorRunState::Waiting)
    {
        FSlateApplication::Get().SetKeyboardFocus(GameWidget, EFocusCause::SetDirectly);
    }
}

void AGirlSaviorCombatHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshDash();
    RefreshOutcome();
}

void AGirlSaviorCombatHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    UnbindEncounter();
    RemovePresentation();
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
