#include "Expeditions/GirlSaviorExpeditionSessionSubsystem.h"

#include "Camp/GirlSaviorCampPreparation.h"
#include "Camp/GirlSaviorCampPrototypeDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "GirlSaviorExpeditionSession"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorExpeditionSession, Log, All);

namespace GirlSavior::ExpeditionTravel
{
constexpr const TCHAR* RunOption = TEXT("CampRunId");

// Compare full asset packages after removing only Unreal's play-world prefix.
FString MapPackage(const UWorld& World)
{
    return UWorld::RemovePIEPrefix(World.GetPackage()->GetName());
}

bool MatchesMap(const UWorld& World, const TSoftObjectPtr<UWorld>& Map)
{
    return !Map.IsNull() && FName(*MapPackage(World)) == Map.ToSoftObjectPath().GetLongPackageFName();
}

// The engine may rewrite this string during validation. Keep the original soft reference immutable.
bool CanOpenMap(const TSoftObjectPtr<UWorld>& Map)
{
    FString Package = Map.ToSoftObjectPath().GetLongPackageName();
    return GEngine && !Package.IsEmpty() && GEngine->MakeSureMapNameIsValid(Package);
}

FString AbilityList(const TArray<FName>& Ids)
{
    return FString::JoinBy(Ids, TEXT(","), [](FName Id) { return Id.ToString(); });
}
}

// Check that both referenced definitions still match the immutable selection and can coexist.
bool FGirlSaviorExpeditionLaunch::ValidateAbilities(FText& OutError) const
{
    constexpr int32 Count = FGirlSaviorCampPreparation::SlotCount;
    if (!RunId.IsValid() || MissionId.IsNone() || Abilities.Num() != Count || AbilityIds.Num() != Count)
    {
        OutError = LOCTEXT("InvalidPayload", "Данные выбранных умений недоступны.");
        return false;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (!IsValid(Abilities[Index]) || Abilities[Index]->AbilityId != AbilityIds[Index])
        {
            OutError = LOCTEXT("MismatchedPayload", "Данные выбранных умений изменились.");
            return false;
        }
    }
    return UGirlSaviorSupportAbilityDefinition::ValidatePair(*Abilities[0], *Abilities[1], OutError);
}

// The delegates are global. The handlers match both our game instance and the pending origin.
void UGirlSaviorExpeditionSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine)
    {
        GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
    }
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

// Leaving the game instance also discards the remembered selection.
void UGirlSaviorExpeditionSessionSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnTravelFailure().RemoveAll(this);
    }
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    ClearRun();
    LastConfirmedAbilityIds.Reset();
#if WITH_EDITOR
    NextDispatchForTest = {};
#endif
    Super::Deinitialize();
}

// Camp travel is defined for the local single-player world, including single-player PIE.
bool UGirlSaviorExpeditionSessionSubsystem::OwnsWorld(const UWorld& World) const
{
    return World.IsGameWorld() && World.GetGameInstance() == GetGameInstance() && World.GetNetMode() == NM_Standalone;
}

// Confirm and retain the entire draft before queuing the one effective travel request.
bool UGirlSaviorExpeditionSessionSubsystem::RequestLaunch(UWorld& World, const UGirlSaviorCampPrototypeDefinition& Catalog,
    const FGirlSaviorCampPreparation& Draft, FText& OutError)
{
    using namespace GirlSavior::ExpeditionTravel;

    if (Phase != EPhase::Idle || !OwnsWorld(World) || !MatchesMap(World, Catalog.CampMap))
    {
        OutError = LOCTEXT("LaunchUnavailable", "Сейчас нельзя отправиться на вылазку.");
        return false;
    }

    FGirlSaviorSupportOffer First;
    FGirlSaviorSupportOffer Second;
    FText ValidationError;
    if (!Draft.ResolveForLaunch(Catalog, First, Second, ValidationError))
    {
        UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Launch validation failed in %s: %s"),
            *MapPackage(World), *ValidationError.ToString());
        OutError = LOCTEXT("ValidateDraft", "Проверьте карты вылазки и выбранные умения.");
        return false;
    }
    if (!CanOpenMap(Catalog.ExpeditionMap))
    {
        OutError = LOCTEXT("RouteUnavailable", "Карта боевого маршрута недоступна.");
        return false;
    }

    Launch.RunId = FGuid::NewGuid();
    Launch.MissionId = Draft.GetMissionId();
    Launch.AbilityIds = {First.Ability->AbilityId, Second.Ability->AbilityId};
    Launch.Abilities = {First.Ability, Second.Ability};
    Launch.CampMap = Catalog.CampMap;
    Launch.ExpeditionMap = Catalog.ExpeditionMap;
    Phase = EPhase::LaunchPending;
    OriginWorld = &World;
    UE_LOG(LogGirlSaviorExpeditionSession, Display, TEXT("Launch requested RunId=%s MissionId=%s AbilityIds=%s Map=%s"),
        *Launch.RunId.ToString(), *Launch.MissionId.ToString(), *AbilityList(Launch.AbilityIds),
        *Launch.ExpeditionMap.ToSoftObjectPath().GetLongPackageName());

    const FString Options = FString::Printf(TEXT("%s=%s"), RunOption, *Launch.RunId.ToString(EGuidFormats::DigitsWithHyphens));
    if (!Dispatch(World, Launch.ExpeditionMap, Options))
    {
        UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Launch dispatch did not leave origin RunId=%s Map=%s"),
            *Launch.RunId.ToString(), *MapPackage(World));
        ClearRun();
        OutError = LOCTEXT("LaunchDispatchFailed", "Не удалось открыть боевой маршрут. Попробуйте ещё раз.");
        return false;
    }
    return true;
}

// Only the expected destination may reserve a pending launch. A second claimant cannot replace it.
EGirlSaviorExpeditionClaim UGirlSaviorExpeditionSessionSubsystem::ClaimLaunch(UWorld& World, const FString& Options,
    FGirlSaviorExpeditionLaunch& OutLaunch, FText& OutError)
{
    using namespace GirlSavior::ExpeditionTravel;

    OutLaunch = {};
    const bool bHasOption = UGameplayStatics::HasOption(Options, RunOption);
    const bool bOwnedWorld = OwnsWorld(World);
    if (bOwnedWorld && ExpeditionWorld.Get() == &World)
    {
        OutError = LOCTEXT("DuplicateClaim", "Эта вылазка уже была открыта.");
        UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Duplicate launch claim RunId=%s Map=%s"),
            *Launch.RunId.ToString(), *MapPackage(World));
        return EGirlSaviorExpeditionClaim::Duplicate;
    }

    const bool bTrustedDestination = bOwnedWorld && !bTravelFailed && Phase == EPhase::LaunchPending
        && !ExpeditionWorld.IsValid() && OriginWorld.Get() != &World && MatchesMap(World, Launch.ExpeditionMap);
    if (!bHasOption && !bTrustedDestination)
    {
        return EGirlSaviorExpeditionClaim::Standalone;
    }

    FGuid IncomingId;
    const bool bMatchingToken = bHasOption
        && FGuid::ParseExact(UGameplayStatics::ParseOption(Options, RunOption), EGuidFormats::DigitsWithHyphens, IncomingId)
        && IncomingId.IsValid() && IncomingId == Launch.RunId;
    FText ValidationError;
    if (!bTrustedDestination || !bMatchingToken || !Launch.ValidateAbilities(ValidationError))
    {
        OutError = LOCTEXT("RejectedClaim", "Не удалось подтвердить данные вылазки.");
        UE_LOG(LogGirlSaviorExpeditionSession, Warning,
            TEXT("Rejected launch claim RunId=%s Map=%s TrustedDestination=%d MatchingToken=%d Details=%s"),
            *Launch.RunId.ToString(), *MapPackage(World), bTrustedDestination, bMatchingToken, *ValidationError.ToString());
        if (bTrustedDestination)
        {
            ExpeditionWorld = &World;
            EnterRecovery(World, Launch.RunId, OutError);
        }
        return EGirlSaviorExpeditionClaim::Rejected;
    }

    ExpeditionWorld = &World;
    OutLaunch = Launch;
    UE_LOG(LogGirlSaviorExpeditionSession, Display, TEXT("Launch claimed RunId=%s MissionId=%s AbilityIds=%s Map=%s"),
        *Launch.RunId.ToString(), *Launch.MissionId.ToString(), *AbilityList(Launch.AbilityIds), *MapPackage(World));
    return EGirlSaviorExpeditionClaim::Claimed;
}

// The pair becomes the remembered selection only after the encounter has attached support.
bool UGirlSaviorExpeditionSessionSubsystem::CommitLaunch(UWorld& World, FGuid RunId, FText& OutError)
{
    if (Phase != EPhase::LaunchPending || ExpeditionWorld.Get() != &World || !OwnsWorld(World)
        || bTravelFailed || !RunId.IsValid() || Launch.RunId != RunId || !Launch.ValidateAbilities(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = LOCTEXT("CommitRejected", "Не удалось завершить подготовку вылазки.");
        }
        return false;
    }

    LastConfirmedAbilityIds = Launch.AbilityIds;
    Phase = EPhase::InExpedition;
    OriginWorld.Reset();
    UE_LOG(LogGirlSaviorExpeditionSession, Display, TEXT("Launch committed RunId=%s Map=%s"),
        *RunId.ToString(), *GirlSavior::ExpeditionTravel::MapPackage(World));
    return true;
}

// Recovery is available only to a world that received this session's trusted launch.
bool UGirlSaviorExpeditionSessionSubsystem::EnterRecovery(UWorld& World, FGuid RunId, const FText& Error)
{
    if (!OwnsWorld(World) || ExpeditionWorld.Get() != &World || !RunId.IsValid() || Launch.RunId != RunId
        || (Phase != EPhase::LaunchPending && Phase != EPhase::InExpedition && Phase != EPhase::Recovery))
    {
        return false;
    }

    Phase = EPhase::Recovery;
    OriginWorld.Reset();
    UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Expedition recovery RunId=%s Map=%s Details=%s"),
        *RunId.ToString(), *GirlSavior::ExpeditionTravel::MapPackage(World), *Error.ToString());
    return true;
}

// The encounter owns outcome admission. The session separately checks who owns the active run.
bool UGirlSaviorExpeditionSessionSubsystem::CanReturn(const UWorld& World) const
{
    return (Phase == EPhase::InExpedition || Phase == EPhase::Recovery) && ExpeditionWorld.Get() == &World && OwnsWorld(World);
}

// Schedule an absolute return so the Camp world cannot inherit the expedition URL's token.
bool UGirlSaviorExpeditionSessionSubsystem::RequestReturn(UWorld& World, FText& OutError)
{
    using namespace GirlSavior::ExpeditionTravel;

    if (!CanReturn(World) || !CanOpenMap(Launch.CampMap))
    {
        OutError = LOCTEXT("CampUnavailable", "Карта лагеря недоступна. Попробуйте ещё раз.");
        return false;
    }

    const EPhase PreviousPhase = Phase;
    Phase = EPhase::ReturnPending;
    OriginWorld = &World;
    UE_LOG(LogGirlSaviorExpeditionSession, Display, TEXT("Return requested RunId=%s Map=%s"),
        *Launch.RunId.ToString(), *Launch.CampMap.ToSoftObjectPath().GetLongPackageName());
    if (!Dispatch(World, Launch.CampMap, FString()))
    {
        UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Return dispatch did not leave origin RunId=%s Map=%s"),
            *Launch.RunId.ToString(), *MapPackage(World));
        Phase = PreviousPhase;
        OriginWorld.Reset();
        OutError = LOCTEXT("ReturnDispatchFailed", "Не удалось открыть лагерь. Попробуйте ещё раз.");
        return false;
    }
    return true;
}

// A correctly possessed Camp pawn retires the run. Its remembered pair remains in the game instance.
void UGirlSaviorExpeditionSessionSubsystem::NotifyCampReady(UWorld& World)
{
    using namespace GirlSavior::ExpeditionTravel;

    if (Phase == EPhase::ReturnPending && !bTravelFailed && OwnsWorld(World)
        && OriginWorld.Get() != &World && MatchesMap(World, Launch.CampMap))
    {
        UE_LOG(LogGirlSaviorExpeditionSession, Display, TEXT("Camp ready RunId=%s Map=%s"),
            *Launch.RunId.ToString(), *MapPackage(World));
        ClearRun();
    }
}

// Only the claiming world can identify the active run.
FGuid UGirlSaviorExpeditionSessionSubsystem::GetActiveRunId(const UWorld& World) const
{
    return ExpeditionWorld.Get() == &World && OwnsWorld(World) ? Launch.RunId : FGuid();
}

// Prevent the Camp panel from dispatching a second launch while its request is active.
bool UGirlSaviorExpeditionSessionSubsystem::IsLaunchPendingFrom(const UWorld& World) const
{
    return Phase == EPhase::LaunchPending && OriginWorld.Get() == &World && OwnsWorld(World);
}

// Keep the outcome action disabled until the route world has completed the return request.
bool UGirlSaviorExpeditionSessionSubsystem::IsReturnPendingFrom(const UWorld& World) const
{
    return Phase == EPhase::ReturnPending && OriginWorld.Get() == &World && OwnsWorld(World);
}

// A scoped test may replace a single dispatch. Normal travel always uses the saved soft world reference.
bool UGirlSaviorExpeditionSessionSubsystem::Dispatch(UWorld& World, const TSoftObjectPtr<UWorld>& Map, const FString& Options)
{
#if WITH_EDITOR
    if (NextDispatchForTest)
    {
        FDispatchForTest Test = MoveTemp(NextDispatchForTest);
        NextDispatchForTest = {};
        return Test(World, Map, Options);
    }
#endif
    if (!GEngine || !GEngine->GetWorldContextFromWorld(&World))
    {
        return false;
    }
    UGameplayStatics::OpenLevelBySoftObjectPtr(&World, Map, true, Options);
    return true;
}

// Discard travel ownership and references without changing the last committed pair.
void UGirlSaviorExpeditionSessionSubsystem::ClearRun()
{
    Phase = EPhase::Idle;
    Launch = {};
    OriginWorld.Reset();
    ExpeditionWorld.Reset();
    bTravelFailed = false;
}

// Let the engine recover. The old widget may disappear even though the origin still exists here.
void UGirlSaviorExpeditionSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
    if (!World || !OwnsWorld(*World) || OriginWorld.Get() != World
        || (Phase != EPhase::LaunchPending && Phase != EPhase::ReturnPending))
    {
        return;
    }

    bTravelFailed = true;
    UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Engine travel failure RunId=%s Map=%s Type=%s Details=%s"),
        *Launch.RunId.ToString(), *GirlSavior::ExpeditionTravel::MapPackage(*World),
        ETravelFailure::ToString(Failure), *Error);
}

// Ignore notifications from other PIE sessions. Allow the expected new world to finish its handshake.
void UGirlSaviorExpeditionSessionSubsystem::HandlePostLoadMap(UWorld* World)
{
    using namespace GirlSavior::ExpeditionTravel;

    if (!World || !OwnsWorld(*World) || Phase == EPhase::Idle || OriginWorld.Get() == World
        || ExpeditionWorld.Get() == World)
    {
        return;
    }

    if (!bTravelFailed && ((Phase == EPhase::LaunchPending && MatchesMap(*World, Launch.ExpeditionMap))
        || (Phase == EPhase::ReturnPending && MatchesMap(*World, Launch.CampMap))))
    {
        return;
    }

    UE_LOG(LogGirlSaviorExpeditionSession, Warning, TEXT("Cleared stale expedition state RunId=%s Map=%s"),
        *Launch.RunId.ToString(), *MapPackage(*World));
    ClearRun();
}

#undef LOCTEXT_NAMESPACE
