#include "Encounters/GirlSaviorCombatHUD.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCombatHUD"

// The viewport owns presentation and the encounter owns every gameplay transition.
void AGirlSaviorCombatHUD::BeginPlay()
{
    Super::BeginPlay();
    const auto* Player = GetOwningPlayerController() ? GetOwningPlayerController()->GetLocalPlayer() : nullptr;
    Viewport = Player ? Player->ViewportClient.Get() : nullptr;
    if (!Viewport.IsValid())
    {
        return;
    }
    for (TActorIterator<AGirlSaviorCombatEncounter> It(GetWorld()); It; ++It)
    {
        Encounter = *It;
        break;
    }
    if (!Encounter.IsValid())
    {
        return;
    }

    const FSlateBrush* PanelBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor PanelColor(0.025f, 0.035f, 0.052f, 0.94f);
    const FLinearColor TextColor(0.91f, 0.94f, 0.96f);
    const FLinearColor DetailColor(0.51f, 0.81f, 0.82f);

    Overlay = SNew(SOverlay)
        .Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot()
        .HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)
        [
            SNew(SBorder)
            .Visibility(EVisibility::HitTestInvisible)
            .BorderImage(PanelBrush).BorderBackgroundColor(PanelColor).Padding(12)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock).Text_UObject(this, &ThisClass::GetVitalsText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(TextColor)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [
                    SNew(STextBlock).Text_UObject(this, &ThisClass::GetDashText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(DetailColor)
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBorder)
            .Visibility_UObject(this, &ThisClass::GetOutcomeVisibility)
            .BorderImage(PanelBrush).BorderBackgroundColor(PanelColor).Padding(26)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock).Text_UObject(this, &ThisClass::GetOutcomeTitle)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 27)).ColorAndOpacity(TextColor)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)
                [
                    SNew(STextBlock).Text_UObject(this, &ThisClass::GetOutcomeCause)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(DetailColor)
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                [
                    SAssignNew(RetryButton, SButton).Text(LOCTEXT("Retry", "Повторить"))
                        .OnClicked_UObject(this, &ThisClass::HandleRetry)
                ]
            ]
        ];
    Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 20);
    // The viewport survives level travel and can still have the old retry button's focus.
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::FocusGameplayViewport));
}

void AGirlSaviorCombatHUD::FocusGameplayViewport()
{
    const TSharedPtr<SViewport> GameWidget = Viewport.IsValid() ? Viewport->GetGameViewportWidget() : nullptr;
    if (FSlateApplication::IsInitialized() && GameWidget.IsValid() && Encounter.IsValid()
        && Encounter->GetRunState() == EGirlSaviorRunState::Waiting)
    {
        FSlateApplication::Get().SetKeyboardFocus(GameWidget, EFocusCause::SetDirectly);
    }
}

void AGirlSaviorCombatHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bOutcomeFocused && FSlateApplication::IsInitialized() && RetryButton.IsValid()
        && Encounter.IsValid() && Encounter->IsOutcomeVisible())
    {
        bOutcomeFocused = true;
        FSlateApplication::Get().SetKeyboardFocus(RetryButton, EFocusCause::SetDirectly);
    }
}

FText AGirlSaviorCombatHUD::GetVitalsText() const
{
    const auto* Hero = Encounter.IsValid() ? Encounter->GetHero() : nullptr;
    if (!Hero)
    {
        return FText::GetEmpty();
    }
    const UGirlSaviorDamageReceiverComponent& Vitals = *Hero->GetDamageReceiver();
    const FText Health = FText::Format(LOCTEXT("Health", "Здоровье {0} / {1}"),
        FText::AsNumber(Vitals.GetHealth()), FText::AsNumber(Vitals.GetMaxHealth()));
    return Vitals.GetBarrier() > 0.0
        ? FText::Format(LOCTEXT("WithBarrier", "{0}    Барьер {1}"), Health, FText::AsNumber(Vitals.GetBarrier()))
        : Health;
}

FText AGirlSaviorCombatHUD::GetDashText() const
{
    const auto* Hero = Encounter.IsValid() ? Encounter->GetHero() : nullptr;
    const auto* Move = Hero ? Cast<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement()) : nullptr;
    const auto* Controller = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
    if (!Move || Encounter->GetRunState() != EGirlSaviorRunState::Active
        || !Controller || !Controller->CanUseGameplayInput() || Controller->IsInSafeArea())
    {
        return LOCTEXT("DashUnavailable", "Рывок недоступен");
    }
    if (Move->IsDashProtected())
    {
        return LOCTEXT("DashProtected", "Защита от прямых попаданий");
    }
    const double Remaining = Move->GetDashCooldownRemaining();
    if (Remaining <= 0.0)
    {
        return LOCTEXT("DashReady", "Рывок готов · Left Shift");
    }
    FNumberFormattingOptions Format;
    Format.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
    return FText::Format(LOCTEXT("DashCooldown", "Рывок через {0} с"), FText::AsNumber(Remaining, &Format));
}

FText AGirlSaviorCombatHUD::GetOutcomeTitle() const
{
    return Encounter.IsValid() && Encounter->GetRunState() == EGirlSaviorRunState::Succeeded
        ? LOCTEXT("Success", "Выход достигнут") : LOCTEXT("Defeat", "Герой погиб");
}

FText AGirlSaviorCombatHUD::GetOutcomeCause() const
{
    if (!Encounter.IsValid() || Encounter->GetRunState() != EGirlSaviorRunState::Defeated)
    {
        return FText::GetEmpty();
    }
    const FText& Cause = Encounter->GetDefeatResult().Spec.Cause;
    return Cause.IsEmpty() ? LOCTEXT("UnknownCause", "Смертельное попадание") : Cause;
}

EVisibility AGirlSaviorCombatHUD::GetOutcomeVisibility() const
{
    return Encounter.IsValid() && Encounter->IsOutcomeVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply AGirlSaviorCombatHUD::HandleRetry()
{
    if (Encounter.IsValid())
    {
        Encounter->Retry();
    }
    return FReply::Handled();
}

void AGirlSaviorCombatHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    if (Viewport.IsValid() && Overlay.IsValid())
    {
        Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    }
    Overlay.Reset();
    RetryButton.Reset();
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
