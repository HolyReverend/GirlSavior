#include "Enemies/GirlSaviorEnemyAttackComponent.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAIController.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorPursuer, Log, All);

namespace
{
constexpr int32 FanSegments = 16;
constexpr double WarningFloorOffset = 3.0;
constexpr double SwingDisplaySeconds = 0.14;
constexpr ECollisionChannel CombatOccluder = ECC_GameTraceChannel4;

// A center inside the fan or any touching edge counts as a capsule contact.
bool IntersectsDisc(TConstArrayView<FVector> Polygon, const FVector& Center, const float Radius)
{
    if (Polygon.Num() < 3)
    {
        return false;
    }
    bool bInside = false;
    const FVector2D Point(Center.X, Center.Y);
    for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
    {
        const FVector2D A(Polygon[Previous].X, Polygon[Previous].Y);
        const FVector2D B(Polygon[Index].X, Polygon[Index].Y);
        const FVector2D Edge = B - A;
        const double LengthSquared = Edge.SizeSquared();
        const double Alpha = LengthSquared > UE_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0, 1.0) : 0.0;
        if (FVector2D::DistSquared(Point, A + Alpha * Edge) <= FMath::Square(double(Radius)))
        {
            return true;
        }
        if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < A.X + Edge.X * (Point.Y - A.Y) / Edge.Y)
        {
            bInside = !bInside;
        }
    }
    return bInside;
}
}

UGirlSaviorEnemyAttackComponent::UGirlSaviorEnemyAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UGirlSaviorEnemyAttackComponent::ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter,
    const FGirlSaviorPursuerTuning& Tuning, ULineBatchComponent& WarningVisual)
{
    Enemy = CastChecked<AGirlSaviorEnemyCharacter>(GetOwner());
    OwningEncounter = &Encounter;
    Visual = &WarningVisual;
    Pursuer = Tuning;
    AddTickPrerequisiteActor(&Encounter);
    SetComponentTickEnabled(true);
}

void UGirlSaviorEnemyAttackComponent::StopForEncounter()
{
    SetComponentTickEnabled(false);
    if (AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get())
    {
        RemoveTickPrerequisiteActor(Encounter);
    }
    OwningEncounter.Reset();
    CancelAttack();
}

void UGirlSaviorEnemyAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopForEncounter();
    Super::EndPlay(EndPlayReason);
}

void UGirlSaviorEnemyAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    AGirlSaviorEnemyCharacter* Body = Enemy.Get();
    AGirlSaviorCharacter* Hero = Encounter ? Encounter->GetHero() : nullptr;
    if (!GetWorld() || !Hero || !Body || !Visual.IsValid() || !Encounter->CanEnemyEngage(*Body))
    {
        CancelAttack();
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (SwingClearAt > 0.0 && Now >= SwingClearAt)
    {
        Visual->Flush();
        SwingClearAt = 0.0;
    }
    switch (Cue.Phase)
    {
    case EGirlSaviorEnemyAttackPhase::Approach:
        TryStartAttack(*Hero, Now);
        break;
    case EGirlSaviorEnemyAttackPhase::Preparation:
        UpdatePreparation(*Hero, Now);
        break;
    case EGirlSaviorEnemyAttackPhase::Recovery:
        if (Now >= Cue.PhaseEnd)
        {
            Cue = {};
        }
        break;
    case EGirlSaviorEnemyAttackPhase::Active:
        break; // The strike resolves in the commit update.
    }
}

FVector UGirlSaviorEnemyAttackComponent::DirectionTo(const AGirlSaviorCharacter& Hero) const
{
    const FVector ToHero = (Hero.GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    return ToHero.IsNearlyZero() ? Enemy->GetActorForwardVector().GetSafeNormal2D() : ToHero;
}

bool UGirlSaviorEnemyAttackComponent::TryStartAttack(AGirlSaviorCharacter& Hero, const double Now)
{
    const double ReachWithHero = Pursuer.Reach + Hero.GetCapsuleComponent()->GetScaledCapsuleRadius();
    const AGirlSaviorEnemyCharacter* Body = Enemy.Get();
    if (FVector::DistSquared2D(Body->GetActorLocation(), Hero.GetActorLocation()) > FMath::Square(ReachWithHero)
        || !Body->GetCharacterMovement()->IsMovingOnGround())
    {
        return false;
    }

    Cue.Origin = Body->GetActorLocation();
    Cue.Direction = DirectionTo(Hero);
    BuildFloorPolygon({}, CurrentRanges);
    AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    if (!CanContact(Hero) || !Encounter->IsWarningVisible(Cue.FloorPolygon))
    {
        Cue.FloorPolygon.Reset();
        return false;
    }

    Cue.AttackId = Encounter->AllocateAttackId();
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Preparation;
    Cue.PhaseStart = Now;
    Cue.PhaseEnd = Now + Pursuer.Preparation;
    LockAt = Cue.PhaseEnd - Pursuer.LockedTail;
    if (AGirlSaviorEnemyAIController* AI = Cast<AGirlSaviorEnemyAIController>(Enemy->GetController()))
    {
        AI->PauseApproach();
    }
    Enemy->GetCharacterMovement()->StopMovementImmediately();
    Enemy->SetActorRotation(Cue.Direction.Rotation());
    ShowShape(false);
    PublishCue();
    UE_LOG(LogGirlSaviorPursuer, Log, TEXT("Pursuer attack %lld started at %.3f."), Cue.AttackId, Now);
    return true;
}

void UGirlSaviorEnemyAttackComponent::UpdateUnlockedShape(const AGirlSaviorCharacter& Hero)
{
    Cue.Origin = Enemy->GetActorLocation();
    Cue.Direction = DirectionTo(Hero);
    Enemy->SetActorRotation(Cue.Direction.Rotation());
    BuildFloorPolygon({}, CurrentRanges);
}

void UGirlSaviorEnemyAttackComponent::UpdatePreparation(AGirlSaviorCharacter& Hero, const double Now)
{
    // Lock the ray lengths as well as the direction, so opening cover cannot expand a warned attack.
    if (!Cue.bLocked)
    {
        UpdateUnlockedShape(Hero);
        if (Now >= LockAt)
        {
            Cue.bLocked = true;
            LockedRanges = CurrentRanges;
            Cue.PhaseEnd = FMath::Max(Cue.PhaseEnd, Now + Pursuer.LockedTail);
        }
        ShowShape(false);
        PublishCue();
        return; // A late frame must present the locked cue before it can commit.
    }
    if (Now >= Cue.PhaseEnd)
    {
        CommitAttack(Hero, Now);
        return;
    }
    BuildFloorPolygon(LockedRanges, CurrentRanges);
    ShowShape(false);
    PublishCue();
}

// Clip each body-height ray, then project the result onto the flat CP1 floor.
void UGirlSaviorEnemyAttackComponent::BuildFloorPolygon(TConstArrayView<float> MaximumRanges, TArray<float>& OutRanges)
{
    check(MaximumRanges.IsEmpty() || MaximumRanges.Num() == FanSegments + 1);
    const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const double FloorZ = Cue.Origin.Z - HalfHeight + WarningFloorOffset;
    Cue.FloorPolygon.Reset(FanSegments + 2);
    Cue.FloorPolygon.Emplace(Cue.Origin.X, Cue.Origin.Y, FloorZ);
    OutRanges.Reset(FanSegments + 1);

    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorPursuerFan), false, Enemy.Get());
    for (int32 Index = 0; Index <= FanSegments; ++Index)
    {
        const double Angle = FMath::Lerp(-double(Pursuer.HalfAngleDegrees),
            double(Pursuer.HalfAngleDegrees), Index / double(FanSegments));
        const FVector Ray = Cue.Direction.RotateAngleAxis(Angle, FVector::UpVector);
        float Reach = MaximumRanges.IsEmpty() ? Pursuer.Reach : MaximumRanges[Index];
        FHitResult Occluder;
        if (GetWorld()->LineTraceSingleByChannel(Occluder, Cue.Origin, Cue.Origin + Ray * Reach, CombatOccluder, Query))
        {
            Reach *= Occluder.Time;
        }
        if (Encounter->CrossesSafeArea(Cue.Origin, Cue.Origin + Ray * Reach))
        {
            float Open = 0.f;
            float Closed = Reach;
            // Narrow to the boundary without requiring knowledge of the safe volume's transform.
            for (int32 Step = 0; Step < 8; ++Step)
            {
                const float Mid = (Open + Closed) * 0.5f;
                if (Encounter->CrossesSafeArea(Cue.Origin, Cue.Origin + Ray * Mid))
                {
                    Closed = Mid;
                }
                else
                {
                    Open = Mid;
                }
            }
            Reach = Open;
        }
        OutRanges.Add(Reach);
        FVector Point = Cue.Origin + Ray * Reach;
        Point.Z = FloorZ;
        Cue.FloorPolygon.Add(Point);
    }
}

bool UGirlSaviorEnemyAttackComponent::CanContact(const AGirlSaviorCharacter& Hero) const
{
    const UCapsuleComponent* HeroCapsule = Hero.GetCapsuleComponent();
    const FVector Center = HeroCapsule->GetComponentLocation();
    const float VerticalExtent = HeroCapsule->GetScaledCapsuleHalfHeight()
        + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (FMath::Abs(Center.Z - Cue.Origin.Z) > VerticalExtent
        || !IntersectsDisc(Cue.FloorPolygon, Center, HeroCapsule->GetScaledCapsuleRadius())
        || OwningEncounter->CrossesSafeArea(Cue.Origin, Center))
    {
        return false;
    }
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorPursuerContact), false, Enemy.Get());
    return !GetWorld()->LineTraceTestByChannel(Cue.Origin, Center, CombatOccluder, Query);
}

void UGirlSaviorEnemyAttackComponent::CommitAttack(AGirlSaviorCharacter& Hero, const double Now)
{
    BuildFloorPolygon(LockedRanges, CurrentRanges);
    if (!OwningEncounter->IsWarningVisible(Cue.FloorPolygon))
    {
        CancelAttack();
        return;
    }

    const int64 AttackId = Cue.AttackId;
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Active;
    Cue.PhaseStart = Now;
    Cue.PhaseEnd = Now;
    SwingClearAt = Now + SwingDisplaySeconds;
    ShowShape(true);
    PublishCue();
    if (Cue.AttackId != AttackId)
    {
        return;
    }

    EGirlSaviorDamageOutcome Outcome = EGirlSaviorDamageOutcome::Rejected;
    if (CanContact(Hero))
    {
        if (UGirlSaviorDamageReceiverComponent* Receiver = Hero.GetDamageReceiver())
        {
            FGirlSaviorDamageSpec Spec;
            Spec.Physical = Pursuer.PhysicalDamage;
            Spec.Delivery = EGirlSaviorDamageDelivery::DirectMelee;
            Spec.SourceFaction = EGirlSaviorFaction::Enemy;
            Spec.SourceId = TEXT("Enemy.Pursuer.Melee");
            Spec.Cause = NSLOCTEXT("GirlSaviorCombat", "PursuerMelee", "Удар преследователя");
            Spec.Instigator = Enemy.Get();
            Spec.Causer = Enemy.Get();
            Spec.AttackId = AttackId;
            Spec.Origin = Cue.Origin;
            const FVector ToHero = (Hero.GetActorLocation() - Cue.Origin).GetSafeNormal2D();
            Spec.Contact = Hero.GetActorLocation() - ToHero * Hero.GetCapsuleComponent()->GetScaledCapsuleRadius();
            Spec.ContactNormal = -ToHero;
            Spec.bHasContact = true;
            Outcome = Receiver->ResolveDamage(Spec).Outcome;
        }
    }
    UE_LOG(LogGirlSaviorPursuer, Log, TEXT("Pursuer attack %lld swung at %.3f (outcome %d)."),
        AttackId, Now, static_cast<int32>(Outcome));
    if (Cue.AttackId != AttackId)
    {
        return; // A lethal hit may already have stopped the encounter and cancelled the cue.
    }
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Recovery;
    Cue.PhaseStart = Now;
    Cue.PhaseEnd = Now + Pursuer.Recovery;
    Cue.FloorPolygon.Reset();
    PublishCue();
    NotifyAttackEnded(EGirlSaviorAttackEndReason::Completed);
}

void UGirlSaviorEnemyAttackComponent::CancelAttack()
{
    if (Cue.Phase == EGirlSaviorEnemyAttackPhase::Approach && SwingClearAt <= 0.0)
    {
        return;
    }
    if (ULineBatchComponent* Batch = Visual.Get())
    {
        Batch->Flush();
    }
    SwingClearAt = 0.0;
    const int64 AttackId = Cue.AttackId;
    Cue = {};
    if (AttackId != 0)
    {
        OnAttackEnded.Broadcast(AttackId, EGirlSaviorAttackEndReason::Cancelled);
    }
}

void UGirlSaviorEnemyAttackComponent::NotifyAttackEnded(EGirlSaviorAttackEndReason Reason)
{
    const int64 AttackId = Cue.AttackId;
    Cue.AttackId = 0;
    if (AttackId != 0)
    {
        OnAttackEnded.Broadcast(AttackId, Reason);
    }
}

void UGirlSaviorEnemyAttackComponent::PublishCue()
{
    const FGirlSaviorAttackCue Snapshot = Cue;
    OnAttackCue.Broadcast(Snapshot);
}

// World-depth batched lines make the same clipped shape visible in normal runtime and PIE.
void UGirlSaviorEnemyAttackComponent::ShowShape(const bool bSwing) const
{
    ULineBatchComponent* Batch = Visual.Get();
    Batch->Flush();
    const FLinearColor Outline = bSwing ? FLinearColor(1.f, 0.84f, 0.55f) : Cue.bLocked
        ? FLinearColor(1.f, 0.12f, 0.035f) : FLinearColor(1.f, 0.53f, 0.085f);
    const float Thickness = bSwing ? 9.f : Cue.bLocked ? 6.f : 4.f;
    TArray<FBatchedLine, TInlineAllocator<64>> Lines;
    const FVector& Base = Cue.FloorPolygon[0];
    const int32 Count = Cue.FloorPolygon.Num();
    for (int32 Previous = Count - 1, Index = 0; Index < Count; Previous = Index++)
    {
        Lines.Emplace(Cue.FloorPolygon[Previous], Cue.FloorPolygon[Index], Outline, 0.f, Thickness, SDPG_World);
        if (!bSwing && Previous > 0 && Index > 0)
        {
            for (const float Fraction : {0.45f, 0.72f})
            {
                Lines.Emplace(FMath::Lerp(Base, Cue.FloorPolygon[Previous], Fraction),
                    FMath::Lerp(Base, Cue.FloorPolygon[Index], Fraction), Outline * 0.65f, 0.f, 2.f, SDPG_World);
            }
        }
    }
    Batch->DrawLines(Lines);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorPursuerSectorTest, "GirlSavior.Combat.Pursuer.Sector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorPursuerSectorTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> ClippedFan = {
        FVector(0, 0, 3), FVector(90, -65, 3), FVector(170, 0, 3), FVector(90, 65, 3)
    };
    TestTrue(TEXT("A capsule center inside the fan is hit"), IntersectsDisc(ClippedFan, FVector(100, 0, 90), 34.f));
    TestTrue(TEXT("A capsule edge meeting the fan is hit"), IntersectsDisc(ClippedFan, FVector(198, 0, 90), 34.f));
    TestFalse(TEXT("Walking sideways beyond the warning avoids the swing"),
        IntersectsDisc(ClippedFan, FVector(100, 110, 90), 34.f));
    TestFalse(TEXT("Clipped sides never use the unclipped attack radius"),
        IntersectsDisc(ClippedFan, FVector(120, -95, 90), 15.f));
    return true;
}
#endif
