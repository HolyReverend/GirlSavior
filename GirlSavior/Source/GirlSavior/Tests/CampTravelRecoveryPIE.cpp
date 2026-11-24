#if WITH_EDITOR

#include "Camp/GirlSaviorCampPlayerController.h"
#include "Camp/GirlSaviorCampPreparation.h"
#include "Camp/GirlSaviorCampDefinition.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorSafeArea.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatHUD.h"
#include "Encounters/GirlSaviorCombatRoom.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Expeditions/GirlSaviorExpeditionSessionSubsystem.h"
#include "Feedback/GirlSaviorCombatFeedbackPresenter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace GirlSavior::CampRecoveryPIE
{
DEFINE_LOG_CATEGORY_STATIC(LogCampRecoveryPIE, Log, All);

enum class ECase : uint8 { Missing, Malformed, Stale, Init, Fallback, Direct, StackedBarrier };
enum class EStep : uint8 { Start, Destination, Back, DirectExit, DirectRetry, StackedActive, StackedCleanup, StackedOutcome };

// Track only the original single-player PIE instance across absolute level loads.
class FProbe
{
public:
    FProbe(UWorld& World, ECase InCase) : Instance(World.GetGameInstance()), Case(InCase)
    {
        Catalog = LoadObject<UGirlSaviorCampDefinition>(nullptr,
            TEXT("/Game/Data/DataAssets/Camp/DA_CampPrototype.DA_CampPrototype"));
        if (!Catalog.IsValid() || !Instance.IsValid() || !IsMap(World, Catalog->CampMap))
        {
            UE_LOG(LogCampRecoveryPIE, Error, TEXT("Start this probe in a fresh CampPrototype PIE."));
            return;
        }
        Advance(EStep::Start);
        Pre = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FProbe::BeforeActors);
        Post = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FProbe::AfterActors);
    }

    ~FProbe() { Stop(); }

private:
    // Compare the authored package with the current PIE copy.
    static bool IsMap(const UWorld& World, const TSoftObjectPtr<UWorld>& Map)
    {
        return FName(*UWorld::RemovePIEPrefix(World.GetPackage()->GetName())) == Map.ToSoftObjectPath().GetLongPackageFName();
    }

    bool IsProbeWorld(const UWorld& World) const
    {
        return Instance.IsValid() && Catalog.IsValid() && World.WorldType == EWorldType::PIE
            && World.GetGameInstance() == Instance.Get();
    }

    void Advance(EStep Next)
    {
        Step = Next;
        Deadline = FPlatformTime::Seconds() + 20.0;
    }

    bool Check(bool bPass, const TCHAR* Name)
    {
        ++Checks;
        if (bPass)
        {
            UE_LOG(LogCampRecoveryPIE, Display, TEXT("PASS case=%d %s"), int32(Case), Name);
            return true;
        }
        ++Failures;
        UE_LOG(LogCampRecoveryPIE, Error, TEXT("FAIL case=%d %s"), int32(Case), Name);
        return false;
    }

    void Finish()
    {
        UE_LOG(LogCampRecoveryPIE, Display, TEXT("Camp recovery PIE completed: %d checks, %d failures."), Checks, Failures);
        Stop();
    }

    void Stop()
    {
        FWorldDelegates::OnWorldPreActorTick.Remove(Pre);
        FWorldDelegates::OnWorldPostActorTick.Remove(Post);
        Pre.Reset();
        Post.Reset();
    }

    // Break one authored dependency only in the destination PIE copy, before its encounter initializes.
    void BeforeActors(UWorld* World, ELevelTick, float)
    {
        if ((Case != ECase::Init && Case != ECase::StackedBarrier) || Step != EStep::Destination || bInvalidated
            || !World || !IsProbeWorld(*World) || !IsMap(*World, Catalog->ExpeditionMap))
        {
            return;
        }
        if (Case == ECase::StackedBarrier)
        {
            auto* Hero = Cast<AGirlSaviorCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            if (Hero && Hero->CombatVitals && !Hero->GetDamageReceiver()->HasVitals())
            {
                // Duplicate only the destination pawn's reference before encounter initialization.
                Hero->CombatVitals = DuplicateObject(Hero->CombatVitals.Get(), Hero);
                Hero->CombatVitals->InitialBarrier = 4.f;
                bInvalidated = true;
            }
            return;
        }
        for (TActorIterator<AGirlSaviorCombatEncounter> It(World); It; ++It)
        {
            bInvalidated = IsValid(It->SafeArea);
            It->SafeArea = nullptr;
            break;
        }
    }

    // Route real absolute travel through the normal pending gate, overriding only the injected failure.
    void Launch(UWorld& World, UGirlSaviorExpeditionSessionSubsystem& Session)
    {
        Origin = &World;
        Advance(EStep::Destination);
        if (Case == ECase::Direct)
        {
            UGameplayStatics::OpenLevelBySoftObjectPtr(&World, Catalog->ExpeditionMap, true);
            return;
        }
        if (Case != ECase::Init && Case != ECase::StackedBarrier)
        {
            const ECase Failure = Case;
            const FGuid OldId = RejectedId;
            Session.SetNextDispatchForTest([Failure, OldId](UWorld& From, const TSoftObjectPtr<UWorld>& Map, const FString&)
            {
                if (Failure == ECase::Fallback)
                {
                    GEngine->SetClientTravel(&From, TEXT("/Game/Maps/TestMaps/Camp3IntentionallyMissing"), TRAVEL_Absolute);
                }
                else
                {
                    const FString Options = Failure == ECase::Missing ? FString() : Failure == ECase::Malformed
                        ? FString(TEXT("CampRunId=malformed"))
                        : TEXT("CampRunId=") + OldId.ToString(EGuidFormats::DigitsWithHyphens);
                    UGameplayStatics::OpenLevelBySoftObjectPtr(&From, Map, true, Options);
                }
                return true;
            });
        }

        FGirlSaviorCampPreparation Draft;
        Draft.Begin(*Catalog.Get());
        Draft.ToggleAbility(*Catalog.Get(), TEXT("MechanismOverdrive"));
        Draft.ToggleAbility(*Catalog.Get(), TEXT("TravelersBarrier"));
        FText Error;
        if (!Check(Session.RequestLaunch(World, *Catalog.Get(), Draft, Error), TEXT("Camp accepted the launch request")))
        {
            UE_LOG(LogCampRecoveryPIE, Error, TEXT("%s"), *Error.ToString());
            Finish();
        }
    }

    // Wait for possession and the outcome HUD in each world before continuing.
    void AfterActors(UWorld* World, ELevelTick, float)
    {
        if (!World || !IsProbeWorld(*World))
        {
            return;
        }
        if (FPlatformTime::Seconds() > Deadline)
        {
            UE_LOG(LogCampRecoveryPIE, Error, TEXT("Timed out case=%d step=%d map=%s"), int32(Case), int32(Step), *World->GetMapName());
            Check(false, TEXT("Travel and outcome finished within the time limit"));
            Finish();
            return;
        }
        auto* Session = Instance->GetSubsystem<UGirlSaviorExpeditionSessionSubsystem>();
        auto* Controller = World->GetFirstPlayerController();
        if (!Session || !Controller || !Controller->GetPawn())
        {
            return;
        }

        const bool bCamp = IsMap(*World, Catalog->CampMap);
        if (Step == EStep::Start && bCamp && Cast<AGirlSaviorCampPlayerController>(Controller))
        {
            Launch(*World, *Session);
            return;
        }
        if (Step == EStep::Back && bCamp && Origin.Get() != World && Cast<AGirlSaviorCampPlayerController>(Controller))
        {
            if (Case == ECase::StackedBarrier)
            {
                const auto Remembered = Session->GetLastConfirmedAbilityIds();
                Check(Remembered.Num() == 2 && Remembered[0] == TEXT("MechanismOverdrive")
                    && Remembered[1] == TEXT("TravelersBarrier"), TEXT("Stacked barrier defeat returned to Camp with the confirmed pair"));
                Finish();
                return;
            }
            Check(Session->GetLastConfirmedAbilityIds().IsEmpty(), TEXT("Returning after failure kept the remembered pair empty"));
            if (Case == ECase::Init || Case == ECase::Fallback)
            {
                Finish();
                return;
            }
            Case = static_cast<ECase>(int32(Case) + 1);
            Advance(EStep::Start);
            return;
        }
        if (Case == ECase::Fallback && Step == EStep::Destination && Origin.Get() != World)
        {
            Check(FName(*UWorld::RemovePIEPrefix(World->GetPackage()->GetName())) == TEXT("/Game/Maps/Demo"),
                TEXT("Engine failure replaced the origin with Demo"));
            Check(Session->GetLastConfirmedAbilityIds().IsEmpty(), TEXT("Engine fallback did not confirm the pair"));
            // A deliberately rejected dispatch proves the fallback cleared the session's launch lock.
            Case = ECase::Init;
            Advance(EStep::Start);
            UGameplayStatics::OpenLevelBySoftObjectPtr(World, Catalog->CampMap, true);
            return;
        }
        if (!IsMap(*World, Catalog->ExpeditionMap) || Origin.Get() == World)
        {
            return;
        }
        TActorIterator<AGirlSaviorCombatEncounter> EncounterIt(World);
        auto* Encounter = EncounterIt ? *EncounterIt : nullptr;
        const auto* HUD = Cast<AGirlSaviorCombatHUD>(Controller->GetHUD());
        auto* Outcome = HUD ? HUD->GetOutcomeWidget() : nullptr;
        if (!Encounter || !Outcome)
        {
            return;
        }
        if (Case == ECase::Direct)
        {
            CheckDirect(*World, *Encounter, *Outcome);
            return;
        }
        if (Case == ECase::StackedBarrier)
        {
            CheckStackedBarrier(*World, *Encounter, *HUD, *Session);
            return;
        }
        if (Step != EStep::Destination || !Outcome->IsOutcomePanelVisible())
        {
            return;
        }

        const auto* Hero = Cast<AGirlSaviorCharacter>(Controller->GetPawn());
        const auto* Receiver = Hero ? Hero->GetDamageReceiver() : nullptr;
        Check(Encounter->GetRunState() == EGirlSaviorRunState::Recovery && Encounter->CanReturnToCamp()
            && Outcome->GetDisplayedActionLabel().ToString() == TEXT("В лагерь") && !Outcome->GetDisplayedCause().IsEmpty(),
            TEXT("The trusted failed launch shows a diagnostic and the Camp action"));
        Check(Hero && Receiver && FMath::IsNearlyEqual(Hero->GetFireComponent()->GetEffectiveShotsPerSecond(), 4.f)
            && Receiver->GetMaxBarrier() == Receiver->GetInitialBarrier() && Session->GetLastConfirmedAbilityIds().IsEmpty(),
            TEXT("Recovery applied no support and confirmed no choice"));
        Check(Case != ECase::Init || bInvalidated, TEXT("The intended route fault exercised recovery"));
        RejectedId = Encounter->GetCampRunId();
        Origin = World;
        Advance(EStep::Back);
        Outcome->GetRetryButton()->OnClicked.Broadcast();
        Check(Session->IsReturnPendingFrom(*World), TEXT("Outcome action requested real travel to Camp"));
    }

    // Exercise the combined barrier through the real HUD, damage presentation and terminal cleanup.
    void CheckStackedBarrier(UWorld& World, AGirlSaviorCombatEncounter& Encounter,
        const AGirlSaviorCombatHUD& HUD, UGirlSaviorExpeditionSessionSubsystem& Session)
    {
        auto* Hero = Encounter.GetHero();
        auto* Vitals = HUD.GetVitalsWidget();
        auto* Outcome = HUD.GetOutcomeWidget();
        TActorIterator<AGirlSaviorCombatFeedbackPresenter> PresenterIt(&World);
        auto* Presenter = PresenterIt ? *PresenterIt : nullptr;
        if (!Hero || !Vitals || !Outcome || !Presenter)
        {
            return;
        }
        auto* Receiver = Hero->GetDamageReceiver();
        auto* Controller = World.GetFirstPlayerController();
        if (Step == EStep::Destination)
        {
            if (!Check(bInvalidated && Hero->CombatVitals && Hero->CombatVitals->GetOuter() == Hero,
                TEXT("Stacked barrier tuning was duplicated only onto the destination pawn")))
            {
                Finish();
                return;
            }
            Check(Receiver->GetInitialBarrier() == 4.0 && FMath::IsNearlyEqual(Receiver->GetMaxBarrier(), 12.0, 1.e-5)
                && FMath::IsNearlyEqual(Receiver->GetBarrier(), 12.0, 1.e-5)
                && FMath::IsNearlyEqual(Vitals->GetDisplayedBarrier(), 12.0, 1.e-5) && Vitals->GetDisplayedHealth() == 100.0,
                TEXT("Baseline four plus Saira eight is visible as twelve in receiver and HUD"));
            if (!Check(Encounter.Rooms.IsValidIndex(0) && IsValid(Encounter.Rooms[0]), TEXT("Stacked fixture has an authored first room")))
            {
                Finish();
                return;
            }
            FVector Position = Encounter.Rooms[0]->Volume->GetComponentLocation();
            Position.Z = Hero->GetActorLocation().Z;
            Hero->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
            Advance(EStep::StackedActive);
        }
        else if (Step == EStep::StackedActive && Encounter.GetRunState() == EGirlSaviorRunState::Active)
        {
            const int32 DefeatBefore = Presenter->GetCueCount(EGirlSaviorFeedbackCue::HeroDefeat);
            const int32 BreakBefore = Presenter->GetCueCount(EGirlSaviorFeedbackCue::BarrierBreak);
            const int32 HealthBefore = Presenter->GetCueCount(EGirlSaviorFeedbackCue::HealthHit);
            FGirlSaviorDamageSpec Spec;
            Spec.SourceFaction = EGirlSaviorFaction::Enemy;
            Spec.SourceId = TEXT("Camp4.StackedBarrier.Lethal");
            Spec.Cause = NSLOCTEXT("GirlSaviorCamp4Tests", "StackedBarrierLethal", "Проверка составного барьера");
            Spec.Delivery = EGirlSaviorDamageDelivery::Area;
            Spec.Physical = 1000.f;
            const auto Result = Receiver->ResolveDamage(Spec);
            Check(Result.bKilled && Result.HealthBefore == 100.0 && Result.HealthAfter == 0.0
                && FMath::IsNearlyEqual(Result.BarrierBefore, 12.0, 1.e-5) && Result.BarrierAfter == 0.0
                && Vitals->GetDisplayedHealth() == 0.0 && Vitals->GetDisplayedBarrier() == 0.0,
                TEXT("Lethal stacked hit publishes zero health and barrier to HUD"));
            Check(Presenter->GetCueCount(EGirlSaviorFeedbackCue::HeroDefeat) == DefeatBefore + 1
                && Presenter->GetCueCount(EGirlSaviorFeedbackCue::BarrierBreak) == BreakBefore + 1
                && Presenter->GetCueCount(EGirlSaviorFeedbackCue::HealthHit) == HealthBefore,
                TEXT("Lethal stacked hit emits one break and defeat cue with death precedence"));
            Check(Encounter.GetRunState() == EGirlSaviorRunState::Defeated && Outcome->IsDeathNoticeVisible()
                && Outcome->GetDisplayedCause().EqualTo(Spec.Cause) && !Outcome->IsOutcomePanelVisible(),
                TEXT("Lethal stacked hit shows immediate notice and retains the defeat delay"));
            Advance(EStep::StackedCleanup);
        }
        else if (Step == EStep::StackedCleanup)
        {
            Check(Receiver->GetMaxBarrier() == 4.0 && Receiver->GetBarrier() == 0.0
                && Hero->GetCharacterMovement()->MovementMode == MOVE_None,
                TEXT("Next tick removes support capacity while retaining the baseline barrier cap"));
            Check(Encounter.GetLiveEnemyCount() == 0 && Encounter.GetLiveProjectileCount() == 0
                && Presenter->GetActiveWarningCount() == 0, TEXT("Stacked lethal cleanup clears encounter threats and warnings"));
            Advance(EStep::StackedOutcome);
        }
        else if (Step == EStep::StackedOutcome && Outcome->IsOutcomePanelVisible())
        {
            Check(Outcome->GetDisplayedActionLabel().ToString() == TEXT("В лагерь")
                && Outcome->GetRetryButton()->HasUserFocus(Controller), TEXT("Stacked defeat outcome focuses its Camp return action"));
            Origin = &World;
            Advance(EStep::Back);
            Outcome->GetRetryButton()->OnClicked.Broadcast();
            Check(Session.IsReturnPendingFrom(World), TEXT("Stacked defeat action requested actual return travel"));
        }
    }

    // Exercise standalone success and the original Retry path through the real exit volume.
    void CheckDirect(UWorld& World, AGirlSaviorCombatEncounter& Encounter,
        UGirlSaviorCombatOutcomeWidget& Outcome)
    {
        auto* Hero = Encounter.GetHero();
        if (!Hero)
        {
            return;
        }
        if (Step == EStep::Destination || Step == EStep::DirectRetry)
        {
            const auto* Receiver = Hero->GetDamageReceiver();
            Check(!Encounter.IsCampOrigin() && !Encounter.GetCampRunId().IsValid()
                && Encounter.GetRunState() == EGirlSaviorRunState::Waiting
                && FMath::IsNearlyEqual(Hero->GetFireComponent()->GetEffectiveShotsPerSecond(), 4.f)
                && Receiver->GetMaxBarrier() == Receiver->GetInitialBarrier() && Receiver->GetHealth() == Receiver->GetMaxHealth(),
                TEXT("Standalone starts in the safe pocket with baseline vitals and cadence"));
            if (Step == EStep::DirectRetry)
            {
                Finish();
                return;
            }
            if (!Check(Encounter.Rooms.IsValidIndex(0) && IsValid(Encounter.Rooms[0]), TEXT("Route has an authored first room")))
            {
                Finish();
                return;
            }
            FVector Position = Encounter.Rooms[0]->Volume->GetComponentLocation();
            Position.Z = 90.0;
            Hero->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
            Advance(EStep::DirectExit);
        }
        else if (Step == EStep::DirectExit && Encounter.GetRunState() == EGirlSaviorRunState::Active)
        {
            Hero->SetActorLocation(Encounter.ExitVolume->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
        }
        else if (Step == EStep::DirectExit && Outcome.IsOutcomePanelVisible())
        {
            Check(Encounter.GetRunState() == EGirlSaviorRunState::Succeeded
                && Outcome.GetDisplayedActionLabel().ToString() == TEXT("Повторить"), TEXT("Standalone victory presents Retry"));
            Origin = &World;
            Advance(EStep::DirectRetry);
            Outcome.GetRetryButton()->OnClicked.Broadcast();
        }
    }

    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UGirlSaviorCampDefinition> Catalog;
    TWeakObjectPtr<UWorld> Origin;
    FDelegateHandle Pre;
    FDelegateHandle Post;
    FGuid RejectedId;
    ECase Case;
    EStep Step = EStep::Start;
    double Deadline = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    bool bInvalidated = false;
};

TUniquePtr<FProbe> ActiveProbe;
FAutoConsoleCommandWithWorldAndArgs RecoveryCommand(TEXT("GirlSavior.TestCampRecovery"),
    TEXT("Run Camp recovery in fresh CampPrototype PIE. Optional Fallback, Direct or StackedBarrier exercises focused acceptance cases."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE)
        {
            const ECase Case = Args.Contains(TEXT("Fallback")) ? ECase::Fallback
                : Args.Contains(TEXT("Direct")) ? ECase::Direct
                : Args.Contains(TEXT("StackedBarrier")) ? ECase::StackedBarrier : ECase::Missing;
            ActiveProbe.Reset();
            ActiveProbe = MakeUnique<FProbe>(*World, Case);
        }
    }));
}

#endif
