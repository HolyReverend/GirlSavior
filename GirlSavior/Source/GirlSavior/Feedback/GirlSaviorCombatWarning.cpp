#include "Feedback/GirlSaviorCombatWarning.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

AGirlSaviorCombatWarning::AGirlSaviorCombatWarning()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Lines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("WarningLines"));
    SetRootComponent(Lines);
    Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Lines->SetCanEverAffectNavigation(false);
    Lines->PrimaryComponentTick.bStartWithTickEnabled = false;
}

void AGirlSaviorCombatWarning::ApplyCue(const FGirlSaviorAttackCue& Cue)
{
    Snapshot = Cue;
    SetActorTickEnabled(Snapshot.bLocked && Snapshot.Phase == EGirlSaviorEnemyAttackPhase::Preparation);
    DrawWarning(1.f);
}

void AGirlSaviorCombatWarning::PlayFinalSwing()
{
    bFinalSwing = true;
    SetActorTickEnabled(false);
    DrawWarning(1.f);
    SetLifeSpan(0.14f);
}

void AGirlSaviorCombatWarning::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float FinalFlash = Snapshot.bLocked && Snapshot.Phase == EGirlSaviorEnemyAttackPhase::Preparation
        ? FMath::Clamp(static_cast<float>((Snapshot.PhaseEnd - GetWorld()->GetTimeSeconds()) / 0.16), 0.f, 1.f) : 1.f;
    DrawWarning(1.f + 0.45f * (1.f - FinalFlash));
}

// Positions and widths come directly from the same cue used by the attack component.
void AGirlSaviorCombatWarning::DrawWarning(float Intensity)
{
    Lines->Flush();
    using ERole = EGirlSaviorEnemyRole;
    const bool bActive = Snapshot.Phase == EGirlSaviorEnemyAttackPhase::Active;
    const FLinearColor Warm(1.f, 0.42f, 0.075f);
    const FLinearColor Locked(1.f, 0.15f, 0.045f);
    const FLinearColor Bright(1.f, 0.76f, 0.42f);
    const FLinearColor Color = (bActive || bFinalSwing ? Bright : Snapshot.bLocked ? Locked : Warm) * Intensity;
    const float Width = bActive ? 8.f : Snapshot.bLocked ? 6.f : 4.f;
    TArray<FBatchedLine, TInlineAllocator<64>> Shape;
    const auto Add = [&](const FVector& From, const FVector& To, const FLinearColor& LineColor, float Thickness)
    {
        Shape.Emplace(From, To, LineColor, 0.f, Thickness, SDPG_World);
    };

    if (Snapshot.Role == ERole::Pursuer)
    {
        const auto& Points = Snapshot.FloorPolygon;
        if (Points.Num() < 3)
        {
            return;
        }
        for (int32 Index = 0, Previous = Points.Num() - 1; Index < Points.Num(); Previous = Index++)
        {
            if (!bFinalSwing || (Index > 0 && Previous > 0))
            {
                Add(Points[Previous], Points[Index], Color, Width);
            }
            if (!bActive && Index > 0 && Previous > 0)
            {
                constexpr double InnerFraction = 0.58;
                Add(FMath::Lerp(Points[0], Points[Previous], InnerFraction),
                    FMath::Lerp(Points[0], Points[Index], InnerFraction), Color * 0.56f, 2.f);
            }
        }
    }
    else if (Snapshot.Role == ERole::Shooter)
    {
        Add(Snapshot.FloorStart, Snapshot.FloorEnd, Color, Width * 0.8f);
    }
    else
    {
        const FVector Edge = FVector::CrossProduct(FVector::UpVector, Snapshot.Direction) * Snapshot.HalfWidth;
        const FLinearColor Stripe = FLinearColor(1.f, bActive ? 0.45f : 0.16f, 0.44f) * Intensity;
        Add(Snapshot.FloorStart - Edge, Snapshot.FloorEnd - Edge, Stripe, Width);
        Add(Snapshot.FloorStart + Edge, Snapshot.FloorEnd + Edge, Stripe, Width);
        for (int32 Index = 1; Index <= 4; ++Index)
        {
            const FVector Point = FMath::Lerp(Snapshot.FloorStart, Snapshot.FloorEnd, Index * 0.25);
            Add(Point - Edge, Point + Edge, Stripe * 0.68f, 2.f);
        }
    }
    Lines->DrawLines(Shape);
}
