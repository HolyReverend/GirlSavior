#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Expeditions/GirlSaviorExpeditionLaunch.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GirlSaviorExpeditionSessionSubsystem.generated.h"

class UGirlSaviorCampDefinition;
class UGirlSaviorWeaponProfileSubsystem;
struct FGirlSaviorCampPreparation;

enum class EGirlSaviorExpeditionClaim : uint8
{
    Standalone,
    Claimed,
    Rejected,
    Duplicate
};

// Owns the local session handoff and remembers only the most recently committed pair.
UCLASS()
class GIRLSAVIOR_API UGirlSaviorExpeditionSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool RequestLaunch(UWorld& World, const UGirlSaviorCampDefinition& Catalog,
        const FGirlSaviorCampPreparation& Draft, FText& OutError);
    EGirlSaviorExpeditionClaim ClaimLaunch(UWorld& World, const FString& Options,
        FGirlSaviorExpeditionLaunch& OutLaunch, FText& OutError);
    bool CommitLaunch(UWorld& World, FGuid RunId, FText& OutError);
    bool EnterRecovery(UWorld& World, FGuid RunId, const FText& Error);
    bool CanReturn(const UWorld& World) const;
    bool RequestReturn(UWorld& World, FText& OutError);
    void NotifyCampReady(UWorld& World);

    TConstArrayView<FName> GetLastConfirmedAbilityIds() const { return LastConfirmedAbilityIds; }
    FGuid GetActiveRunId(const UWorld& World) const;
    bool IsLaunchPendingFrom(const UWorld& World) const;
    bool IsReturnPendingFrom(const UWorld& World) const;

#if WITH_EDITOR
    // Replace only the next dispatch in this instance to exercise failures without editing map packages.
    using FDispatchForTest = TFunction<bool(UWorld&, const TSoftObjectPtr<UWorld>&, const FString&)>;
    void SetNextDispatchForTest(FDispatchForTest Dispatch) { NextDispatchForTest = MoveTemp(Dispatch); }
    void SetWeaponProfileForTest(UGirlSaviorWeaponProfileSubsystem* Profile);
#endif

private:
    enum class EPhase : uint8
    {
        Idle,
        LaunchPending,
        InExpedition,
        Recovery,
        ReturnPending
    };

    bool OwnsWorld(const UWorld& World) const;
    bool Dispatch(UWorld& World, const TSoftObjectPtr<UWorld>& Map, const FString& Options);
    void ClearRun();
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
    void HandlePostLoadMap(UWorld* World);

    UPROPERTY(Transient)
    FGirlSaviorExpeditionLaunch Launch;

    UPROPERTY(Transient)
    TArray<FName> LastConfirmedAbilityIds;

    EPhase Phase = EPhase::Idle;
    TWeakObjectPtr<UWorld> OriginWorld;
    TWeakObjectPtr<UWorld> ExpeditionWorld;
    bool bTravelFailed = false;

#if WITH_EDITOR
    FDispatchForTest NextDispatchForTest;
    TWeakObjectPtr<UGirlSaviorWeaponProfileSubsystem> WeaponProfileForTest;
#endif
};
