#include "Enemies/GirlSaviorEnemyAIController.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
constexpr double PathRefreshSeconds = 0.20;
constexpr double GoalMoveThreshold = 65.0;
constexpr float ThreatPositionAcceptance = 20.f;

// A projected position uses a tighter stopping radius than a normal chase.
FAIMoveRequest MakeApproachRequest(const FVector& Goal, float Acceptance, bool bPositioning)
{
    FAIMoveRequest Request(Goal);
    Request.SetUsePathfinding(true);
    Request.SetAllowPartialPath(false);
    Request.SetProjectGoalLocation(!bPositioning);
    Request.SetAcceptanceRadius(Acceptance);
    Request.SetReachTestIncludesAgentRadius(!bPositioning);
    return Request;
}
}

AGirlSaviorEnemyAIController::AGirlSaviorEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGirlSaviorEnemyAIController::ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter)
{
    OwningEncounter = &Encounter;
    NextQueryTime = 0.0;
    SetActorTickEnabled(true);
}

void AGirlSaviorEnemyAIController::PauseApproach()
{
    if (GetMoveStatus() != EPathFollowingStatus::Idle)
    {
        StopMovement();
    }
}

void AGirlSaviorEnemyAIController::StopForEncounter()
{
    PauseApproach();
    OwningEncounter.Reset();
    SetActorTickEnabled(false);
}

// Validate the remaining segments from the live body position, including paths that navigation recalculates.
bool AGirlSaviorEnemyAIController::IsPathAdmissible(const AGirlSaviorEnemyCharacter& Body,
    const FNavPathSharedPtr& Path, int32 FirstPoint) const
{
    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    if (!Encounter || !Path.IsValid() || !Path->IsValid() || Path->IsPartial())
    {
        return false;
    }
    const auto& Points = Path->GetPathPoints();
    if (Points.IsEmpty())
    {
        return false;
    }
    FVector Start = Body.GetActorLocation();
    for (int32 Index = FMath::Clamp(FirstPoint, 0, Points.Num() - 1); Index < Points.Num(); ++Index)
    {
        FVector End = Points[Index].Location;
        End.Z = Start.Z;
        if (!Body.IsInFixtureRoom(End)
            || !Encounter->CanTraverseCombatSegment(Start, End, Body.GetCapsuleComponent()->GetScaledCapsuleRadius()))
        {
            return false;
        }
        Start = End;
    }
    return true;
}

// Project four local alternatives and use the shortest complete path to a position that can threaten now.
bool AGirlSaviorEnemyAIController::FindThreatPosition(const AGirlSaviorEnemyCharacter& Body,
    const AGirlSaviorCharacter& Hero, FVector& OutGoal, bool& bOutCanThreaten) const
{
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Navigation)
    {
        return false;
    }
    const UGirlSaviorEnemyDefinition& Definition = *Body.Definition;
    const bool bShooter = Definition.Role == EGirlSaviorEnemyRole::Shooter;
    const FVector HeroLocation = Hero.GetActorLocation();
    const FVector BodyLocation = Body.GetActorLocation();
    const double DistanceToHero = FVector::Dist2D(BodyLocation, HeroLocation);
    const bool bMustBackAway = !bShooter && DistanceToHero < Definition.Charger.MinRange;
    const double PositionRadius = bShooter ? Definition.Shooter.PreferredDistance
        : FMath::Lerp(double(Definition.Charger.MinRange), double(Definition.Charger.MaxRange), 0.5);
    const FVector Away = (BodyLocation - HeroLocation).GetSafeNormal2D(UE_SMALL_NUMBER, -Body.GetActorForwardVector());
    constexpr double SampleAngles[] = {25.0, -25.0, 70.0, -70.0};
    const double HalfHeight = Body.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    double BestThreatLength = TNumericLimits<double>::Max();
    double BestStepLength = TNumericLimits<double>::Max();
    FVector BestStep = FVector::ZeroVector;
    bOutCanThreaten = false;

    for (const double Angle : SampleAngles)
    {
        const FVector Sample = HeroLocation + Away.RotateAngleAxis(Angle, FVector::UpVector) * PositionRadius;
        FNavLocation Projected;
        if (!Navigation->ProjectPointToNavigation(Sample, Projected, FVector(60, 60, HalfHeight + 35),
            &GetNavAgentPropertiesRef()))
        {
            continue;
        }
        const FVector CandidateCenter = Projected.Location + FVector(0, 0, HalfHeight);
        if (!Body.IsInFixtureRoom(CandidateCenter)
            || FVector::DistSquared2D(BodyLocation, CandidateCenter) <= FMath::Square(ThreatPositionAcceptance))
        {
            continue;
        }
        const bool bCanThreaten = Body.GetAttackComponent()->CanThreatenFrom(CandidateCenter, Hero);
        const double ProjectedRange = FVector::Dist2D(CandidateCenter, HeroLocation);
        if (!bCanThreaten && (!bMustBackAway || ProjectedRange < Definition.Charger.MinRange
            || ProjectedRange > Definition.Charger.MaxRange))
        {
            continue;
        }

        FPathFindingQuery Query;
        FNavPathSharedPtr Path;
        const FAIMoveRequest Request = MakeApproachRequest(Projected.Location, ThreatPositionAcceptance, true);
        if (!BuildPathfindingQuery(Request, Query))
        {
            continue;
        }
        FindPathForMoveRequest(Request, Query, Path);
        if (!IsPathAdmissible(Body, Path, 0))
        {
            continue;
        }
        const double Length = Path->GetLength();
        if (bCanThreaten && Length < BestThreatLength)
        {
            BestThreatLength = Length;
            OutGoal = Projected.Location;
            bOutCanThreaten = true;
        }
        if (!bCanThreaten && Length < BestStepLength)
        {
            BestStepLength = Length;
            BestStep = Projected.Location;
        }
    }
    if (bOutCanThreaten)
    {
        return true;
    }
    // A Charger inside its minimum range still needs room to back away when its warning would be offscreen.
    if (BestStepLength < TNumericLimits<double>::Max())
    {
        OutGoal = BestStep;
        return true;
    }
    return false;
}

void AGirlSaviorEnemyAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    const AGirlSaviorEnemyCharacter* Body = Cast<AGirlSaviorEnemyCharacter>(GetPawn());
    AGirlSaviorCharacter* Hero = Encounter ? Encounter->GetHero() : nullptr;
    if (!Hero || !Body || !Body->Definition || !Encounter->CanEnemyEngage(*Body)
        || Body->GetAttackComponent()->GetAttackPhase() != EGirlSaviorEnemyAttackPhase::Approach)
    {
        PauseApproach();
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextQueryTime)
    {
        return;
    }
    NextQueryTime = Now + PathRefreshSeconds;
    const bool bHeavyRole = Body->Definition->Role != EGirlSaviorEnemyRole::Pursuer;
    if (bHeavyRole && Body->GetAttackComponent()->ShouldHoldPosition(*Hero))
    {
        PauseApproach();
        return;
    }

    const FVector TargetPosition = Hero->GetActorLocation();
    const bool bChargerTooClose = Body->Definition->Role == EGirlSaviorEnemyRole::Charger
        && FVector::DistSquared2D(Body->GetActorLocation(), TargetPosition)
            < FMath::Square(Body->Definition->Charger.MinRange);
    const EPathFollowingStatus::Type Status = GetMoveStatus();
    const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
    if (Status == EPathFollowingStatus::Moving
        && (bFollowingPosition || !bChargerTooClose)
        && FVector::DistSquared2D(TargetPosition, LastTargetPosition) < FMath::Square(GoalMoveThreshold)
        && IsPathAdmissible(*Body, PathFollowing->GetPath(), static_cast<int32>(PathFollowing->GetNextPathIndex()))
        && (!bFollowingThreatPosition || Body->GetAttackComponent()->CanThreatenFrom(
            LastRequestedGoal + FVector(0, 0, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), *Hero)))
    {
        return;
    }

    FVector Goal = TargetPosition;
    bool bCanThreaten = false;
    const bool bPositioning = bHeavyRole && FindThreatPosition(*Body, *Hero, Goal, bCanThreaten);
    if (!bPositioning && bChargerTooClose)
    {
        PauseApproach();
        return;
    }

    // Heavy enemies without a usable angle keep approaching on navmesh until the view permits a fresh admission.
    const float ChaseAcceptance = Body->Definition->Role == EGirlSaviorEnemyRole::Shooter ? 35.f : 25.f;
    const FAIMoveRequest Request = MakeApproachRequest(Goal, bPositioning ? ThreatPositionAcceptance : ChaseAcceptance,
        bPositioning);
    FNavPathSharedPtr Path;
    const FPathFollowingRequestResult Result = MoveTo(Request, &Path);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful && IsPathAdmissible(*Body, Path, 0))
    {
        LastRequestedGoal = Goal;
        LastTargetPosition = TargetPosition;
        bFollowingPosition = bPositioning;
        bFollowingThreatPosition = bCanThreaten;
    }
    else
    {
        PauseApproach();
    }
}
