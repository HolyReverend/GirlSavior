#include "Enemies/GirlSaviorEnemyAIController.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"

namespace
{
constexpr double PathRefreshSeconds = 0.20;
constexpr double GoalMoveThreshold = 65.0;
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

// Check the current path as well as newly requested paths in case navigation updated it.
bool AGirlSaviorEnemyAIController::PathCrossesSafeArea(const FNavPathSharedPtr& Path) const
{
    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    const AGirlSaviorEnemyCharacter* Body = Cast<AGirlSaviorEnemyCharacter>(GetPawn());
    if (!Encounter || !Body || !Path.IsValid() || Path->IsPartial())
    {
        return true;
    }
    const auto& Points = Path->GetPathPoints();
    if (Points.IsEmpty())
    {
        return true;
    }
    const int32 First = FMath::Min(static_cast<int32>(GetPathFollowingComponent()->GetNextPathIndex()), Points.Num() - 1);
    FVector Start = Body->GetActorLocation();
    for (int32 Index = First; Index < Points.Num(); ++Index)
    {
        FVector End = Points[Index].Location;
        End.Z = Start.Z;
        if (Encounter->CrossesSafeArea(Start, End, Body->Definition->CapsuleRadius))
        {
            return true;
        }
        Start = End;
    }
    return false;
}

void AGirlSaviorEnemyAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    const AGirlSaviorEnemyCharacter* Body = Cast<AGirlSaviorEnemyCharacter>(GetPawn());
    AGirlSaviorCharacter* Hero = Encounter ? Encounter->GetHero() : nullptr;
    if (!Hero || !Body || !Encounter->CanEnemyEngage(*Body)
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
    const FVector Goal = Hero->GetActorLocation();
    const EPathFollowingStatus::Type Status = GetMoveStatus();
    if (Status == EPathFollowingStatus::Moving
        && FVector::DistSquared2D(Goal, LastRequestedGoal) < FMath::Square(GoalMoveThreshold)
        && !PathCrossesSafeArea(GetPathFollowingComponent()->GetPath()))
    {
        return;
    }

    // Use a direct chase request only when navmesh has a complete route outside the pocket.
    FAIMoveRequest Request(Goal);
    Request.SetUsePathfinding(true);
    Request.SetAllowPartialPath(false);
    Request.SetProjectGoalLocation(true);
    Request.SetAcceptanceRadius(25.f);
    Request.SetReachTestIncludesAgentRadius(true);

    FNavPathSharedPtr Path;
    const FPathFollowingRequestResult Result = MoveTo(Request, &Path);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        if (PathCrossesSafeArea(Path))
        {
            PauseApproach();
            return;
        }
        LastRequestedGoal = Goal;
    }
}
