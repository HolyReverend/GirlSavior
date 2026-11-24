// UnrealBuildTool excludes the module Tests folder from the packaged game target.
#if WITH_EDITOR || UE_BUILD_DEVELOPMENT

#include "Blueprint/UserWidget.h"
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Camp/GirlSaviorCampInteractable.h"
#include "Camp/GirlSaviorCampPlayerController.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatHUD.h"
#include "Encounters/GirlSaviorCombatRoom.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Expeditions/GirlSaviorExpeditionSessionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace GirlSavior::CampRoundTrips
{
DEFINE_LOG_CATEGORY_STATIC(LogCampRoundTrips, Log, All);

const FName Skills[] = {TEXT("StaticTrace"), TEXT("MechanismOverdrive"), TEXT("TravelersBarrier")};
constexpr int32 Pairs[3][2] = {{0, 1}, {1, 2}, {2, 0}};

// PIE and cooked Development both start the probe at the saved Camp entry point.
bool IsCampWorld(const UWorld& World)
{
    return (World.WorldType == EWorldType::PIE || World.WorldType == EWorldType::Game)
        && World.GetMapName().EndsWith(TEXT("CampPrototype"));
}

class FProbe
{
public:
    // Reacquire world objects after travel. The game instance survives the complete run.
    explicit FProbe(UWorld& World)
        : GameInstance(World.GetGameInstance())
        , PlayWorldType(World.WorldType)
        , bPackaged(FPlatformProperties::RequiresCookedData())
    {
        if (!GameInstance.IsValid() || !IsCampWorld(World))
        {
            UE_LOG(LogCampRoundTrips, Error, TEXT("Start GirlSavior.TestCampExpeditions in a fresh CampPrototype session."));
            return;
        }
        Advance(EStep::CampArrival);
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FProbe::Tick);
    }

    ~FProbe()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        StopCadence();
        FTSTicker::GetCoreTicker().RemoveTicker(PauseHandle);
        if (auto* World = PausedWorld.Get())
        {
            UGameplayStatics::SetGamePaused(World, false);
        }
    }

private:
    enum class EStep : uint8
    {
        CampArrival, TablePrompt, Missions, RouteArrival, Activate, Cadence, Enemy, SlowExpiry, Recharge, Outcome
    };

    void Advance(EStep Next)
    {
        Step = Next;
        Deadline = FPlatformTime::Seconds() + 25.0;
    }

    bool Check(bool bPassed, const TCHAR* What)
    {
        ++Checks;
        if (!bPassed)
        {
            ++Failures;
        }
        UE_LOG(LogCampRoundTrips, Display, TEXT("%s pair=%d step=%d %s"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), Round + 1, static_cast<int32>(Step), What);
        return bPassed;
    }

    void Finish()
    {
        StopCadence();
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        TickHandle.Reset();
        UE_LOG(LogCampRoundTrips, Display, TEXT("%s completed: %d checks, %d failures, %d completed round trips."),
            bPackaged ? TEXT("CAMP 4 packaged") : TEXT("CAMP 3 PIE"), Checks, Failures, Round);
    }

    bool Includes(int32 Skill) const
    {
        return Pairs[Round][0] == Skill || Pairs[Round][1] == Skill;
    }

    static bool MatchesPair(TConstArrayView<FName> Ids, int32 Index)
    {
        return Ids.Num() == 2 && Ids[0] == Skills[Pairs[Index][0]] && Ids[1] == Skills[Pairs[Index][1]];
    }

    // Capture the actual Slate HUD as part of the reproducible round-trip record.
    void Capture(const TCHAR* Filename) const
    {
        const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
            / (bPackaged ? TEXT("Camp4") : TEXT("Camp3")));
        IFileManager::Get().MakeDirectory(*Folder, true);
        FScreenshotRequest::RequestScreenshot(Folder / Filename, true, false);
    }

    // PIE can start with editor focus instead of game focus after a world change.
    static void FocusViewport(APlayerController& Controller)
    {
        const auto* Player = Controller.GetLocalPlayer();
        const auto Viewport = Player && Player->ViewportClient ? Player->ViewportClient->GetGameViewportWidget() : nullptr;
        if (Viewport)
        {
            auto& Slate = FSlateApplication::Get();
            if (const auto Window = Slate.FindWidgetWindow(Viewport.ToSharedRef()))
            {
                Window->BringToFront(true);
            }
            Slate.SetUserFocus(Slate.GetUserIndexForKeyboard(), Viewport, EFocusCause::SetDirectly);
        }
    }

    static void SendE(APlayerController& Controller, EInputEvent Event)
    {
        const auto* Player = Controller.GetLocalPlayer();
        FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
        Controller.InputKey(FInputKeyEventArgs(Viewport, INPUTDEVICEID_NONE, EKeys::E, Event, FPlatformTime::Cycles64()));
    }

    // Reposition only the temporary probe pawn. Camp 2 covers ordinary walking to these targets.
    static void Place(APawn& Pawn, const FVector& Position)
    {
        if (auto* Character = Cast<ACharacter>(&Pawn))
        {
            Character->GetCharacterMovement()->StopMovementImmediately();
        }
        Pawn.SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        const auto* PC = Cast<APlayerController>(Pawn.GetController());
        if (auto* Camera = PC ? Cast<AGirlSaviorPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
        {
            Camera->RequestFollowSnap();
        }
    }

    static UButton* FindButton(const AGirlSaviorCampPlayerController& Controller, FName Name)
    {
        const UUserWidget* Modal = Controller.GetModalWidget();
        return Modal ? Cast<UButton>(Modal->GetWidgetFromName(Name)) : nullptr;
    }

    bool Click(const AGirlSaviorCampPlayerController& Controller, FName Name)
    {
        UButton* Button = FindButton(Controller, Name);
        if (!Check(Button && Button->GetIsEnabled(), *FString::Printf(TEXT("enabled button %s"), *Name.ToString())))
        {
            return false;
        }
        Button->OnClicked.Broadcast();
        return true;
    }

    static FName CardButton(int32 Index)
    {
        return FName(*FString::Printf(TEXT("CampAbilityButton%d"), Index));
    }

    // Open the saved Camp table through its real input action and seed from the committed session.
    void TickCamp(UWorld& World, UGirlSaviorExpeditionSessionSubsystem& Session)
    {
        auto* Controller = Cast<AGirlSaviorCampPlayerController>(World.GetFirstPlayerController());
        if (!Controller || !Controller->GetPawn())
        {
            return;
        }

        switch (Step)
        {
        case EStep::CampArrival:
            if (TActorIterator<AGirlSaviorCampInteractable> It(&World); It)
            {
                for (; It; ++It)
                {
                    if (It->GetInteractionId() != TEXT("ExpeditionTable"))
                    {
                        continue;
                    }
                    if (Round != 0)
                    {
                        Check(!Session.GetActiveRunId(World).IsValid(), TEXT("returned Camp retired active RunId"));
                    }
                    Check(FMath::IsNearlyEqual(FVector::Dist2D(Controller->GetPawn()->GetActorLocation(),
                        It->GetInteractionLocation()), 300.0, 30.0), TEXT("Camp uses the saved start near the table"));
                    FocusViewport(*Controller);
                    Place(*Controller->GetPawn(), It->GetInteractionLocation() + FVector(-115, -65, 0));
                    Advance(EStep::TablePrompt);
                    break;
                }
            }
            break;
        case EStep::TablePrompt:
            if (const auto* Target = Controller->GetDisplayedTarget(); Target && Target->GetInteractionId() == TEXT("ExpeditionTable"))
            {
                SendE(*Controller, IE_Pressed);
                Advance(EStep::Missions);
            }
            break;
        case EStep::Missions:
        {
            if (Controller->GetCurrentPage() != EGirlSaviorCampPage::Missions)
            {
                return;
            }
            SendE(*Controller, IE_Released);
            if (Round == 0)
            {
                Check(Controller->GetDraft().GetAbilityIds().IsEmpty()
                    && Session.GetLastConfirmedAbilityIds().IsEmpty(), TEXT("first opening has no remembered pair"));
            }
            else
            {
                Check(MatchesPair(Controller->GetDraft().GetAbilityIds(), Round - 1)
                    && MatchesPair(Session.GetLastConfirmedAbilityIds(), Round - 1),
                    TEXT("table reopened with the last confirmed pair in order"));
            }
            if (Round == (bPackaged ? 1 : UE_ARRAY_COUNT(Pairs)))
            {
                if (Click(*Controller, TEXT("MissionConfigureButton")))
                {
                    Capture(bPackaged ? TEXT("PackagedRememberedPair.png") : TEXT("Camp3RememberedPair.png"));
                }
                Finish();
                return;
            }
            if (!Click(*Controller, TEXT("MissionConfigureButton")))
            {
                Finish();
                return;
            }
            if (Round != 0)
            {
                Click(*Controller, CardButton(Pairs[Round - 1][0]));
                Click(*Controller, CardButton(Pairs[Round - 1][1]));
            }
            Click(*Controller, CardButton(Pairs[Round][0]));
            Click(*Controller, CardButton(Pairs[Round][1]));
            if (!Check(MatchesPair(Controller->GetDraft().GetAbilityIds(), Round), TEXT("selected the requested ordered pair")))
            {
                Finish();
                return;
            }
#if WITH_EDITOR
            if (!bPackaged)
            {
                TestEditorLaunch(World, Session, *Controller);
                break;
            }
#endif
            if (Click(*Controller, TEXT("LaunchButton"))
                && Check(Session.IsLaunchPendingFrom(World), TEXT("launch queued actual travel from the Camp table")))
            {
                Advance(EStep::RouteArrival);
            }
            else
            {
                Finish();
            }
            break;
        }
        default:
            break;
        }
    }

#if WITH_EDITOR
    // Keep controlled dispatch failures and the dispatch counter confined to the editor.
    void TestEditorLaunch(UWorld& World, UGirlSaviorExpeditionSessionSubsystem& Session,
        AGirlSaviorCampPlayerController& Controller)
    {
        if (Round == 0)
        {
            Session.SetNextDispatchForTest([](UWorld&, const TSoftObjectPtr<UWorld>&, const FString&) { return false; });
            Click(Controller, TEXT("LaunchButton"));
            const UButton* Launch = FindButton(Controller, TEXT("LaunchButton"));
            Check(!Session.IsLaunchPendingFrom(World) && MatchesPair(Controller.GetDraft().GetAbilityIds(), Round)
                && Launch && Launch->GetIsEnabled() && Launch->HasUserFocus(&Controller),
                TEXT("failed dispatch preserves selection and permits retry"));
        }
        int32 Dispatches = 0;
        Session.SetNextDispatchForTest([&Dispatches](UWorld& Origin, const TSoftObjectPtr<UWorld>& Destination,
            const FString& Options)
        {
            ++Dispatches;
            UGameplayStatics::OpenLevelBySoftObjectPtr(&Origin, Destination, true, Options);
            return true;
        });
        if (Click(Controller, TEXT("LaunchButton")))
        {
            UButton* Launch = FindButton(Controller, TEXT("LaunchButton"));
            if (Launch)
            {
                Launch->OnClicked.Broadcast();
            }
            Check(Dispatches == 1 && Launch && !Launch->GetIsEnabled()
                && Session.IsLaunchPendingFrom(World), TEXT("double click schedules only one launch"));
            Session.SetNextDispatchForTest({});
            Advance(EStep::RouteArrival);
        }
        else
        {
            Session.SetNextDispatchForTest({});
            Finish();
        }
    }
#endif

    FGirlSaviorDamageResult HitHero(AGirlSaviorCharacter& Hero, float Damage)
    {
        FGirlSaviorDamageSpec Spec;
        Spec.SourceFaction = EGirlSaviorFaction::Enemy;
        Spec.SourceId = TEXT("Camp.PIE");
        Spec.Delivery = EGirlSaviorDamageDelivery::Area;
        Spec.Physical = Damage;
        Spec.Cause = NSLOCTEXT("GirlSaviorCampProbe", "Cause", "Проверочное попадание");
        return Hero.GetDamageReceiver()->ResolveDamage(Spec);
    }

    // Send a registered pistol projectile through the real sweep and damage resolver.
    bool PistolHit(UWorld& World, AGirlSaviorCombatEncounter& Encounter, AGirlSaviorCharacter& Hero,
        AGirlSaviorEnemyCharacter& Enemy)
    {
        auto* Receiver = Enemy.GetDamageReceiver();
        const double HealthBefore = Receiver->GetHealth();
        const double BarrierBefore = Receiver->GetBarrier();
        const FVector Center = Enemy.GetActorLocation();
        const FVector Directions[] = {FVector::ForwardVector, -FVector::ForwardVector,
            FVector::RightVector, -FVector::RightVector};
        for (const FVector& Direction : Directions)
        {
            FGirlSaviorProjectileLaunch Launch;
            Launch.Radius = 4.f;
            Launch.Speed = 1500.f;
            Launch.MaxDistance = 140.f;
            Launch.Damage.Physical = 2.f;
            Launch.Damage.SourceFaction = EGirlSaviorFaction::Hero;
            Launch.Damage.SourceId = TEXT("Hero.Pistol");
            Launch.Damage.Instigator = &Hero;
            Launch.Damage.AttackId = Encounter.AllocateAttackId();
            Launch.Damage.Origin = Center - Direction * 110.f;
            if (Encounter.IsPointInSafeArea(Launch.Damage.Origin))
            {
                continue;
            }
            auto* Shot = AGirlSaviorTestProjectile::SpawnCombatProjectile(World, Hero.GetFireComponent()->ProjectileClass,
                &Hero, Direction, Launch, Encounter);
            for (int32 Frame = 0; Frame < 12 && IsValid(Shot); ++Frame)
            {
                Shot->Tick(1.f / 60.f);
            }
            if (IsValid(Shot))
            {
                Shot->Destroy();
            }
            if (Receiver->GetHealth() < HealthBefore || Receiver->GetBarrier() < BarrierBefore)
            {
                return Receiver->IsAlive();
            }
        }
        return false;
    }

    void StartOutcome(AGirlSaviorCombatEncounter& Encounter, AGirlSaviorCharacter& Hero)
    {
        // Isolate success and defeat while keeping the real terminal encounter and HUD path.
        if (Round == 1)
        {
            Check(HitHero(Hero, 10000.f).bKilled, TEXT("real damage resolver produced lethal result"));
        }
        else
        {
            FVector Location = Encounter.ExitVolume->GetComponentLocation();
            Location.Z = Hero.GetActorLocation().Z;
            Place(Hero, Location);
        }
        Advance(EStep::Outcome);
    }

    // Observe four real shots and their muzzle cues while the combat fire component schedules them.
    void StartCadence(APlayerController& Controller, AGirlSaviorCharacter& Hero)
    {
        FocusViewport(Controller);
        const ULocalPlayer* Player = Controller.GetLocalPlayer();
        const FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
        if (Viewport)
        {
            const FIntPoint Size = Viewport->GetSizeXY();
            Controller.SetMouseLocation(Size.X * 3 / 4, Size.Y / 2);
        }
        auto* Fire = Hero.GetFireComponent();
        CueFire = Fire;
        ShotTimes.Reset();
        const TWeakObjectPtr<UWorld> CueWorld(Hero.GetWorld());
        CueHandle = Fire->OnCombatShotCue.AddLambda([this, CueWorld](FVector, FVector)
        {
            if (const auto* World = CueWorld.Get())
            {
                ShotTimes.Add(World->GetTimeSeconds());
            }
        });
        EffectTime = Hero.GetWorld()->GetTimeSeconds();
        bFireStarted = false;
        Advance(EStep::Cadence);
    }

    void StopCadence()
    {
        if (auto* Fire = CueFire.Get())
        {
            Fire->CancelFireSeries();
            Fire->OnCombatShotCue.Remove(CueHandle);
        }
        CueFire.Reset();
        CueHandle.Reset();
    }

    // Wait in editor real time while Unreal game time and its effect timers are paused.
    void CheckPausedEffect(UWorld& World, float Duration, const TCHAR* Label, TFunction<bool()> Predicate)
    {
        if (!Check(UGameplayStatics::SetGamePaused(&World, true), TEXT("route admits a real game pause")))
        {
            return;
        }
        PausedWorld = &World;
        const double StartedAt = World.GetTimeSeconds();
        PauseHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [this, StartedAt, Label, Predicate = MoveTemp(Predicate)](float)
            {
                if (auto* Current = PausedWorld.Get())
                {
                    Check(Current->IsPaused() && FMath::IsNearlyEqual(Current->GetTimeSeconds(), StartedAt, 0.02)
                        && Predicate(), Label);
                    UGameplayStatics::SetGamePaused(Current, false);
                }
                PausedWorld.Reset();
                PauseHandle.Reset();
                return false;
            }), Duration);
    }

    // Use real game-time timers. Suppress attacks during measurements to avoid unrelated incoming hits.
    void TickRoute(UWorld& World, UGirlSaviorExpeditionSessionSubsystem& Session)
    {
        const TActorIterator<AGirlSaviorCombatEncounter> It(&World);
        auto* Encounter = It ? *It : nullptr;
        auto* Hero = Encounter ? Encounter->GetHero() : nullptr;
        if (!Hero || !Encounter->GetCampRunId().IsValid())
        {
            return;
        }
        auto* PC = Cast<APlayerController>(Hero->GetController());
        auto* HUD = PC ? Cast<AGirlSaviorCombatHUD>(PC->GetHUD()) : nullptr;
        auto* Vitals = HUD ? HUD->GetVitalsWidget() : nullptr;
        auto* Receiver = Hero->GetDamageReceiver();

        switch (Step)
        {
        case EStep::RouteArrival:
        {
            const FGuid NewRun = Encounter->GetCampRunId();
            Check(Encounter->IsCampOrigin() && NewRun != PriorRun && NewRun == Session.GetActiveRunId(World),
                TEXT("Camp request committed with a new matched RunId"));
            PriorRun = NewRun;
            Check(MatchesPair(Session.GetLastConfirmedAbilityIds(), Round), TEXT("destination committed the new ordered pair"));
            Check(Encounter->GetRunState() == EGirlSaviorRunState::Waiting
                && Encounter->IsPointInSafeArea(Hero->GetActorLocation()), TEXT("spawned in the existing safe pocket"));
            Check(FMath::IsNearlyEqual(Receiver->GetHealth(), Receiver->GetMaxHealth()), TEXT("fresh hero starts with full health"));
            Check(FMath::IsNearlyEqual(Hero->GetFireComponent()->GetEffectiveShotsPerSecond(),
                Includes(1) ? 4.24f : 4.0f, 0.0001f), TEXT("Ida's effective pistol cadence is present only when selected"));
            ExpectedBarrier = Includes(2) ? Receiver->GetMaxHealth() * 0.08 : 0.0;
            Check(FMath::IsNearlyEqual(Receiver->GetBarrier(), ExpectedBarrier, 0.0001) && Vitals
                && FMath::IsNearlyEqual(Vitals->GetDisplayedBarrier(), ExpectedBarrier, 0.0001),
                TEXT("Saira's initial barrier and HUD match the selected pair"));
            if (!Check(!Encounter->Rooms.IsEmpty() && Encounter->Rooms[0], TEXT("route has its authored first room")))
            {
                Finish();
                return;
            }
            FVector Location = Encounter->Rooms[0]->Volume->GetComponentLocation();
            Location.Z = Hero->GetActorLocation().Z;
            Place(*Hero, Location);
            Advance(EStep::Activate);
            break;
        }
        case EStep::Activate:
            if (Encounter->GetRunState() == EGirlSaviorRunState::Active)
            {
                if (PC)
                {
                    StartCadence(*PC, *Hero);
                }
                else
                {
                    Check(false, TEXT("measure pistol cadence with a local controller"));
                    Advance(EStep::Enemy);
                }
            }
            break;
        case EStep::Cadence:
        {
            if (!bFireStarted && Hero->GetAimComponent()->IsCursorInsideGameView())
            {
                Hero->GetFireComponent()->SetFireButtonDown(true);
                bFireStarted = true;
            }
            if (ShotTimes.Num() >= 4 || World.GetTimeSeconds() > EffectTime + 2.5)
            {
                const double IdealInterval = 1.0 / (Includes(1) ? 4.24 : 4.0);
                bool bIntervalsMatch = ShotTimes.Num() == 4;
                for (int32 Index = 1; Index < ShotTimes.Num(); ++Index)
                {
                    const double Interval = ShotTimes[Index] - ShotTimes[Index - 1];
                    bIntervalsMatch &= Interval >= IdealInterval - 0.0001 && Interval <= IdealInterval + 0.055;
                    UE_LOG(LogCampRoundTrips, Display, TEXT("Measured pistol interval pair=%d actual=%.4f ideal=%.4f"),
                        Round + 1, Interval, IdealInterval);
                }
                Check(bIntervalsMatch, TEXT("four real muzzle cues obey the selected pistol deadline"));
                StopCadence();
                Advance(EStep::Enemy);
            }
            break;
        }
        case EStep::Enemy:
            for (auto* Enemy : Encounter->GetLivingEnemies())
            {
                if (!IsValid(Enemy) || !Enemy->GetDamageReceiver()->IsAlive())
                {
                    continue;
                }
                BaseWalk = Enemy->GetCharacterMovement()->MaxWalkSpeed;
                Enemy->StopForEncounter();
                TestedEnemy = Enemy;
                Check(PistolHit(World, *Encounter, *Hero, *Enemy), TEXT("registered pistol projectile damaged a living enemy"));
                Check(FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,
                    BaseWalk * (Includes(0) ? 0.9f : 1.f), 0.001f),
                    TEXT("only a selected Liora slows a living enemy"));
                EffectTime = World.GetTimeSeconds();
                Advance(EStep::SlowExpiry);
                if (Round == 0)
                {
                    CheckPausedEffect(World, 1.8f, TEXT("Liora's slow survives a pause longer than its duration"), [this]
                    {
                        return TestedEnemy.IsValid() && FMath::IsNearlyEqual(
                            TestedEnemy->GetCharacterMovement()->MaxWalkSpeed, BaseWalk * 0.9f, 0.001f);
                    });
                }
                break;
            }
            break;
        case EStep::SlowExpiry:
            if (World.GetTimeSeconds() < EffectTime + 1.65)
            {
                return;
            }
            Check(TestedEnemy.IsValid() && FMath::IsNearlyEqual(TestedEnemy->GetCharacterMovement()->MaxWalkSpeed,
                BaseWalk, 0.001f), TEXT("walking speed returns to base after the game-time slow window"));
            if (Includes(2))
            {
                const auto Hit = HitHero(*Hero, 2.f);
                Check(Hit.BarrierLost > 0.0 && Hit.HealthLost == 0.0 && Vitals
                    && FMath::IsNearlyEqual(Vitals->GetDisplayedBarrier(), Hit.BarrierAfter, 0.0001),
                    TEXT("a fully absorbed hit spends support barrier and updates the HUD"));
                EffectTime = World.GetTimeSeconds();
                Advance(EStep::Recharge);
                if (Round == 1)
                {
                    const TWeakObjectPtr<UGirlSaviorDamageReceiverComponent> WeakReceiver(Receiver);
                    CheckPausedEffect(World, 6.3f, TEXT("Saira's barrier stays spent during a pause longer than its recharge"),
                        [this, WeakReceiver]
                        {
                            return WeakReceiver.IsValid() && WeakReceiver->GetBarrier() < ExpectedBarrier - 0.1;
                        });
                }
            }
            else
            {
                StartOutcome(*Encounter, *Hero);
            }
            break;
        case EStep::Recharge:
            if (World.GetTimeSeconds() >= EffectTime + 6.2)
            {
                Check(Vitals && FMath::IsNearlyEqual(Receiver->GetBarrier(), ExpectedBarrier, 0.0001)
                    && FMath::IsNearlyEqual(Vitals->GetDisplayedBarrier(), ExpectedBarrier, 0.0001),
                    TEXT("barrier recharge restores both receiver and HUD after six game seconds"));
                StartOutcome(*Encounter, *Hero);
            }
            break;
        case EStep::Outcome:
        {
            auto* Outcome = HUD ? HUD->GetOutcomeWidget() : nullptr;
            if (!Outcome || !Outcome->IsOutcomePanelVisible())
            {
                return;
            }
            if (CapturedRound != Round)
            {
                Capture(bPackaged ? TEXT("PackagedSuccess.png")
                    : (Round == 1 ? TEXT("Camp3Defeat.png") : TEXT("Camp3Success.png")));
                CapturedRound = Round;
                return;
            }
            Check(Encounter->GetRunState() == (Round == 1 ? EGirlSaviorRunState::Defeated : EGirlSaviorRunState::Succeeded),
                TEXT("existing encounter produced the expected terminal result"));
            if (!Check(Outcome->GetDisplayedActionLabel().ToString() == TEXT("В лагерь")
                && Outcome->GetRetryButton() && Outcome->GetRetryButton()->GetIsEnabled(), TEXT("terminal panel offers return to Camp")))
            {
                Finish();
                return;
            }
#if WITH_EDITOR
            if (!bPackaged && Round == 0)
            {
                Session.SetNextDispatchForTest([](UWorld&, const TSoftObjectPtr<UWorld>&, const FString&) { return false; });
                Outcome->GetRetryButton()->OnClicked.Broadcast();
                Check(Outcome->GetRetryButton()->GetIsEnabled() && !Outcome->GetActionStatus().IsEmpty()
                    && Outcome->GetRetryButton()->HasUserFocus(PC)
                    && !Session.IsReturnPendingFrom(World), TEXT("failed return leaves a focused usable outcome action"));
            }
#endif
            Outcome->GetRetryButton()->OnClicked.Broadcast();
            Check(Session.IsReturnPendingFrom(World), TEXT("outcome action scheduled real travel to Camp"));
            ++Round;
            TestedEnemy.Reset();
            Advance(EStep::CampArrival);
            break;
        }
        default:
            break;
        }

        if (Step == EStep::Cadence || Step == EStep::SlowExpiry || Step == EStep::Recharge)
        {
            for (auto* Enemy : Encounter->GetLivingEnemies())
            {
                if (IsValid(Enemy))
                {
                    Enemy->StopForEncounter();
                }
            }
        }
    }

    void Tick(UWorld* World, ELevelTick, float)
    {
        if (!GameInstance.IsValid())
        {
            FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
            return;
        }
        if (!World || World->WorldType != PlayWorldType || World->GetGameInstance() != GameInstance.Get())
        {
            return;
        }
        if (FPlatformTime::Seconds() > Deadline)
        {
            Check(false, *FString::Printf(TEXT("timed out in %s"), *World->GetMapName()));
            Finish();
            return;
        }
        auto* Session = GameInstance->GetSubsystem<UGirlSaviorExpeditionSessionSubsystem>();
        if (!Session)
        {
            Check(false, TEXT("session subsystem missing"));
            Finish();
            return;
        }
        if (World->GetMapName().EndsWith(TEXT("CampPrototype")))
        {
            TickCamp(*World, *Session);
        }
        else if (World->GetMapName().EndsWith(TEXT("CombatPrototypeRoute")))
        {
            TickRoute(*World, *Session);
        }
    }

    TWeakObjectPtr<UGameInstance> GameInstance;
    EWorldType::Type PlayWorldType;
    bool bPackaged;
    TWeakObjectPtr<AGirlSaviorEnemyCharacter> TestedEnemy;
    TWeakObjectPtr<UGirlSaviorFireComponent> CueFire;
    TWeakObjectPtr<UWorld> PausedWorld;
    FTSTicker::FDelegateHandle PauseHandle;
    FDelegateHandle CueHandle;
    TArray<double> ShotTimes;
    int32 CapturedRound = INDEX_NONE;
    bool bFireStarted = false;
    FDelegateHandle TickHandle;
    EStep Step = EStep::CampArrival;
    FGuid PriorRun;
    int32 Round = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    double Deadline = 0.0;
    double EffectTime = 0.0;
    double ExpectedBarrier = 0.0;
    float BaseWalk = 0.f;
};

TUniquePtr<FProbe> ActiveProbe;
FTSTicker::FDelegateHandle PendingStart;

// ExecCmds may provide no world at startup. Wait for the main Camp game context in that case.
void StartCampProbe(UWorld* World)
{
    FTSTicker::RemoveTicker(PendingStart);
    PendingStart.Reset();
    if (World && IsCampWorld(*World) && World->GetGameInstance())
    {
        ActiveProbe = MakeUnique<FProbe>(*World);
        return;
    }

    const double Deadline = FPlatformTime::Seconds() + 25.0;
    PendingStart = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline](float)
    {
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* Candidate = Context.World();
                if (Candidate && IsCampWorld(*Candidate) && Candidate->GetGameInstance())
                {
                    ActiveProbe = MakeUnique<FProbe>(*Candidate);
                    return false;
                }
            }
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            UE_LOG(LogCampRoundTrips, Error, TEXT("Timed out waiting for the initial CampPrototype game world."));
            return false;
        }
        return true;
    }));
}

FAutoConsoleCommandWithWorldAndArgs VerifyCampExpeditions(TEXT("GirlSavior.TestCampExpeditions"),
    TEXT("Run three Camp round trips in PIE or one in a fresh packaged Development Camp session."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
    {
        StartCampProbe(World);
    }));
}

#endif
