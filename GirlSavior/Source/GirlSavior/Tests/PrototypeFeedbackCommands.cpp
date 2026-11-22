#if WITH_EDITOR

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorCombatPistolSettings.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/Button.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatHUD.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

namespace
{
// Drive real combat services in PIE with deterministic stimuli for the presentation checks.
void RunFeedbackFixture(const TArray<FString>& Args, UWorld* World)
{
    if (!World || World->WorldType != EWorldType::PIE || Args.IsEmpty())
    {
        return;
    }
    TActorIterator<AGirlSaviorCombatEncounter> Encounters(World);
    AGirlSaviorCombatEncounter* Encounter = Encounters ? *Encounters : nullptr;
    auto* Hero = Encounter && Encounter->HasFeedbackPresenter() ? Encounter->GetHero() : nullptr;
    auto* Controller = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
    if (!Controller)
    {
        return;
    }
    const auto* Player = Controller->GetLocalPlayer();
    UGameViewportClient* Client = Player ? Player->ViewportClient.Get() : nullptr;
    FViewport* Viewport = Client ? Client->Viewport : nullptr;
    const FString& Action = Args[0];
    if (Action == TEXT("Viewport") && Client && Args.Num() == 3)
    {
        if (auto* Scene = Client->GetGameViewport())
        {
            Scene->SetFixedViewportSize(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
        }
        return;
    }
    if (Action == TEXT("Focus") && Client && FSlateApplication::IsInitialized())
    {
        if (const auto Widget = Client->GetGameViewportWidget())
        {
            FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
        }
        UE_LOG(LogTemp, Display, TEXT("CP4 input available=%d focus=%d foreground=%d"),
            Controller->CanUseGameplayInput(), Viewport && Viewport->HasFocus(), Viewport && Viewport->IsForegroundWindow());
        return;
    }
    if (Action == TEXT("MeasureView") && Player && Viewport)
    {
        FSceneViewProjectionData Projection;
        if (Player->GetProjectionData(Viewport, Projection))
        {
            const FIntRect& Rect = Projection.GetConstrainedViewRect();
            UE_LOG(LogTemp, Display, TEXT("CP4 constrained view %dx%d."), Rect.Width(), Rect.Height());
        }
        return;
    }
    if (Action == TEXT("Retry"))
    {
        auto* HUD = Cast<AGirlSaviorCombatHUD>(Controller->GetHUD());
        if (HUD && HUD->GetOutcomeWidget() && HUD->GetOutcomeWidget()->IsOutcomePanelVisible())
        {
            auto* Retry = HUD->GetOutcomeWidget()->GetRetryButton();
            if (Retry && Retry->HasUserFocus(Controller))
            {
                UE_LOG(LogTemp, Display, TEXT("CP4 Retry button owns the player's focus."));
                Retry->OnClicked.Broadcast();
            }
        }
        return;
    }
    if (Action == TEXT("Aim") && Args.Num() == 3)
    {
        FVector2D Pixel;
        if (Controller->ProjectWorldLocationToScreen(FVector(FCString::Atod(*Args[1]), FCString::Atod(*Args[2]),
            Hero->GetActorLocation().Z), Pixel))
        {
            Controller->SetMouseLocation(FMath::RoundToInt(Pixel.X), FMath::RoundToInt(Pixel.Y));
        }
        return;
    }
    if ((Action == TEXT("Fire") || Action == TEXT("Move")) && Args.Num() == 2 && Viewport)
    {
        const EInputEvent Event = Args[1] == TEXT("Down") ? IE_Pressed : IE_Released;
        const FKey& Key = Action == TEXT("Fire") ? EKeys::LeftMouseButton : EKeys::D;
        Controller->InputKey(FInputKeyEventArgs(Viewport, INPUTDEVICEID_NONE, Key,
            Event, FPlatformTime::Cycles64()));
        return;
    }
    if (Encounter->GetRunState() != EGirlSaviorRunState::Active)
    {
        return;
    }
    if (Action == TEXT("Hit") || Action == TEXT("Dash"))
    {
        const bool bDash = Action == TEXT("Dash");
        if (bDash && !CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement())
            ->TryStartDash(FVector::ForwardVector))
        {
            UE_LOG(LogTemp, Display, TEXT("CP4 dash could not start (input=%d)."), Controller->CanUseGameplayInput());
            return;
        }
        FGirlSaviorDamageSpec Spec;
        Spec.SourceFaction = EGirlSaviorFaction::Enemy;
        Spec.SourceId = TEXT("CP4.PIE.Fixture");
        Spec.Delivery = bDash ? EGirlSaviorDamageDelivery::DirectProjectile : EGirlSaviorDamageDelivery::Area;
        Spec.Physical = bDash ? 7.f : Args.Num() == 2 ? FCString::Atof(*Args[1]) : 0.f;
        const auto Result = Hero->GetDamageReceiver()->ResolveDamage(Spec);
        UE_LOG(LogTemp, Display, TEXT("CP4 fixture damage result=%d health=%.1f barrier=%.1f."),
            static_cast<int32>(Result.Outcome), Result.HealthAfter, Result.BarrierAfter);
    }
    if (Action == TEXT("Shot") && Args.Num() == 3)
    {
        auto* Fire = Hero->GetFireComponent();
        const auto* Settings = Fire->CombatPistol.Get();
        if (!Settings)
        {
            return;
        }
        FGirlSaviorProjectileLaunch Launch;
        Launch.Radius = Settings->ProjectileRadius;
        Launch.Speed = Settings->ProjectileSpeed;
        Launch.MaxDistance = Settings->MaxDistance;
        Launch.Damage.Physical = Settings->Damage;
        Launch.Damage.SourceFaction = EGirlSaviorFaction::Hero;
        Launch.Damage.SourceId = TEXT("CP4.PIE.Pistol");
        Launch.Damage.Instigator = Hero;
        Launch.Damage.AttackId = Encounter->AllocateAttackId();
        Launch.Damage.Origin = Hero->GetActorLocation();
        const FVector Direction = FVector(FCString::Atod(*Args[1]), FCString::Atod(*Args[2]), 0).GetSafeNormal();
        if (AGirlSaviorTestProjectile::SpawnCombatProjectile(*World, Fire->ProjectileClass, Hero, Direction, Launch, *Encounter))
        {
            Fire->PublishShotCue(Launch.Damage.Origin, Direction);
        }
    }
}

FAutoConsoleCommandWithWorldAndArgs FeedbackFixtureCommand(TEXT("GirlSavior.TestFeedback"),
    TEXT("CP4 PIE stimuli: Viewport W H, MeasureView, Focus, Aim X Y, Fire or Move Down/Up, Hit N, Dash, Shot Dx Dy, Retry."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunFeedbackFixture));
}

#endif
