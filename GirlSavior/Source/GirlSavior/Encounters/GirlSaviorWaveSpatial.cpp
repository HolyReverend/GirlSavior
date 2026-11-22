#include "Encounters/GirlSaviorWaveSpatial.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatRoom.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "SceneView.h"

namespace GirlSaviorWaveSpatial
{
namespace
{
// The spawn check uses the same enabled mesh silhouette as the live bounds check.
bool IsVisibleBodyMesh(const UMeshComponent& Mesh)
{
    return Mesh.GetVisibleFlag() && !Mesh.bHiddenInGame && !Mesh.IsEditorOnly();
}

FTransform GetTemplateMeshTransform(const USceneComponent& Mesh, const USceneComponent& Root)
{
    FTransform Relative = FTransform::Identity;
    for (const USceneComponent* Component = &Mesh; Component && Component != &Root; Component = Component->GetAttachParent())
    {
        Relative = Relative * Component->GetRelativeTransform();
    }
    return Relative;
}

// Require a close projection of the preferred floor point for both path endpoints.
bool ProjectFloor(const UNavigationSystemV1& Navigation, const ANavigationData& NavData,
    const FVector& Floor, float Tolerance, FNavLocation& OutPoint)
{
    return Navigation.ProjectPointToNavigation(Floor, OutPoint, FVector(Tolerance), &NavData)
        && FVector::DistSquared(OutPoint.Location, Floor) <= FMath::Square(static_cast<double>(Tolerance));
}
// All corners behind the near plane can cull a live body. A new spawn still needs a safe projection.
EVisibility ProjectBox(const FSceneViewProjectionData& View, const FBox& Bounds, int32 Margin, bool bCullBehind)
{
    if (!View.IsValidViewRectangle() || !Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN())
    {
        return EVisibility::Invalid;
    }

    const FIntRect& Rect = View.GetConstrainedViewRect();
    const FMatrix Matrix = View.ComputeViewProjectionMatrix();
    FBox2D Projected(ForceInit);
    int32 Behind = 0;
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
            (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
        const FVector4 Clip = Matrix.TransformFVector4(FVector4(Point, 1.0));
        // Unreal uses reversed Z. The near plane is Clip.Z == Clip.W.
        if (Clip.ContainsNaN())
        {
            return EVisibility::Invalid;
        }
        if (Clip.W <= UE_SMALL_NUMBER || Clip.Z > Clip.W)
        {
            ++Behind;
            continue;
        }
        const FVector2D Pixel(
            Rect.Min.X + (0.5 + 0.5 * Clip.X / Clip.W) * Rect.Width(),
            Rect.Min.Y + (0.5 - 0.5 * Clip.Y / Clip.W) * Rect.Height());
        if (Pixel.ContainsNaN())
        {
            return EVisibility::Invalid;
        }
        Projected += Pixel;
    }
    if (Behind > 0)
    {
        return bCullBehind && Behind == 8 ? EVisibility::Outside : EVisibility::Invalid;
    }
    const double Padding = FMath::Max(Margin, 0);
    const FBox2D Screen(FVector2D(Rect.Min.X - Padding, Rect.Min.Y - Padding),
        FVector2D(Rect.Max.X + Padding, Rect.Max.Y + Padding));
    return Screen.Intersect(Projected) ? EVisibility::Visible : EVisibility::Outside;
}
}

EVisibility ProjectBounds(const FSceneViewProjectionData& View, const FBox& Bounds, int32 Margin)
{
    return ProjectBox(View, Bounds, Margin, false);
}

EVisibility ProjectLiveBounds(const FSceneViewProjectionData& View, const FBox& Bounds)
{
    return ProjectBox(View, Bounds, 0, true);
}

FBox SpawnBounds(const AGirlSaviorEnemyCharacter& Defaults, const FTransform& Transform)
{
    const UGirlSaviorEnemyDefinition* Definition = Defaults.Definition;
    if (!Definition || !Defaults.GetRootComponent())
    {
        return FBox(ForceInit);
    }

    const FVector CapsuleExtent(Definition->CapsuleRadius, Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
    FBox Bounds = FBox(-CapsuleExtent, CapsuleExtent).TransformBy(Transform);
    const TInlineComponentArray<UMeshComponent*> Meshes(&Defaults);
    for (const UMeshComponent* Mesh : Meshes)
    {
        if (IsVisibleBodyMesh(*Mesh))
        {
            const FTransform MeshTransform = GetTemplateMeshTransform(*Mesh, *Defaults.GetRootComponent()) * Transform;
            Bounds += Mesh->CalcBounds(MeshTransform).GetBox();
        }
    }
    return Bounds;
}

FBox EnemyBounds(const AGirlSaviorEnemyCharacter& Enemy)
{
    FBox Bounds = Enemy.GetCapsuleComponent()->Bounds.GetBox();
    const TInlineComponentArray<UMeshComponent*> Meshes(&Enemy);
    for (const UMeshComponent* Mesh : Meshes)
    {
        if (Mesh->IsRegistered() && IsVisibleBodyMesh(*Mesh))
        {
            Bounds += Mesh->Bounds.GetBox();
        }
    }
    return Bounds;
}

EVisibility EnemyVisibility(const FSceneViewProjectionData& View, const AGirlSaviorEnemyCharacter& Enemy)
{
    return ProjectLiveBounds(View, EnemyBounds(Enemy));
}

// Check the exact authored capsule and real supporting floor before trying the navmesh.
bool IsSlotClear(const AGirlSaviorCombatEncounter& Encounter, const AGirlSaviorEnemyCharacter& Defaults,
    const FTransform& Transform, float FloorTolerance)
{
    const UGirlSaviorEnemyDefinition* Definition = Defaults.Definition;
    const UWorld* World = Encounter.GetWorld();
    if (!World || !Definition || !FMath::IsFinite(FloorTolerance) || FloorTolerance <= 0.0f
        || Transform.ContainsNaN() || !Transform.GetScale3D().Equals(FVector::OneVector)
        || !Transform.GetRotation().GetUpVector().Equals(FVector::UpVector))
    {
        return false;
    }

    const UCapsuleComponent& Capsule = *Defaults.GetCapsuleComponent();
    const float Radius = Definition->CapsuleRadius;
    const float HalfHeight = Definition->CapsuleHalfHeight;
    const FVector Center = Transform.GetLocation();
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(WaveSlot), false);
    const FCollisionResponseParams Responses(Capsule.GetCollisionResponseToChannels());
    const ECollisionChannel ObjectType = Capsule.GetCollisionObjectType();
    if (Encounter.CrossesSafeArea(Center, Center, Radius)
        || World->OverlapBlockingTestByChannel(Center, Transform.GetRotation(), ObjectType,
            FCollisionShape::MakeCapsule(Radius, HalfHeight), Query, Responses))
    {
        return false;
    }

    const FVector Floor = Center - FVector(0, 0, HalfHeight);
    const FVector TestHeight(0, 0, FloorTolerance);
    FHitResult Support;
    return World->LineTraceSingleByChannel(Support, Floor + TestHeight, Floor - TestHeight, ObjectType, Query, Responses)
        && Support.Component.IsValid() && Support.Component->GetCollisionObjectType() != ECC_Pawn
        && Defaults.GetCharacterMovement()->IsWalkable(Support);
}

bool IsSegmentOpen(const AGirlSaviorCombatEncounter& Encounter, TConstArrayView<TWeakObjectPtr<const AGirlSaviorCombatDoor>> Doors,
    const FVector& StartCenter, const FVector& EndCenter, float Radius)
{
    if (Encounter.CrossesSafeArea(StartCenter, EndCenter, Radius))
    {
        return false;
    }
    return !Doors.ContainsByPredicate([&](const auto& Door)
    {
        const auto* LiveDoor = Door.Get();
        return LiveDoor && LiveDoor->Crosses(StartCenter, EndCenter, Radius);
    });
}

bool FindApproachPath(const AGirlSaviorCombatEncounter& Encounter, TConstArrayView<TWeakObjectPtr<const AGirlSaviorCombatDoor>> Doors,
    const AGirlSaviorEnemyCharacter& Enemy, const FVector& FromCenter, const FVector& TargetCenter,
    float ProjectionTolerance, TArray<FVector>& OutCenterPath, double& OutLength)
{
    OutCenterPath.Reset();
    OutLength = 0.0;
    if (!FMath::IsFinite(ProjectionTolerance) || ProjectionTolerance <= 0.0f
        || FromCenter.ContainsNaN() || TargetCenter.ContainsNaN())
    {
        return false;
    }

    auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Encounter.GetWorld());
    const UGirlSaviorEnemyDefinition* Definition = Enemy.Definition;
    if (!Navigation || !Definition)
    {
        return false;
    }

    const UCapsuleComponent& Capsule = *Enemy.GetCapsuleComponent();
    const float Radius = Enemy.IsTemplate() ? Definition->CapsuleRadius : Capsule.GetScaledCapsuleRadius();
    const float HalfHeight = Enemy.IsTemplate() ? Definition->CapsuleHalfHeight : Capsule.GetScaledCapsuleHalfHeight();
    const AGirlSaviorCharacter* Hero = Encounter.GetHero();
    const float TargetHalfHeight = Hero ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : HalfHeight;
    const FVector CenterOffset(0, 0, HalfHeight);
    const FVector FromFloor = FromCenter - CenterOffset;
    const FVector TargetFloor = TargetCenter - FVector(0, 0, TargetHalfHeight);
    FNavAgentProperties Agent = Enemy.GetNavAgentPropertiesRef();
    Agent.AgentRadius = Radius;
    Agent.AgentHeight = HalfHeight * 2.0f;
    const ANavigationData* NavData = Navigation->GetNavDataForProps(Agent, FromFloor);
    FNavLocation Start;
    FNavLocation End;
    if (!NavData || !ProjectFloor(*Navigation, *NavData, FromFloor, ProjectionTolerance, Start)
        || !ProjectFloor(*Navigation, *NavData, TargetFloor, ProjectionTolerance, End))
    {
        return false;
    }

    FPathFindingQuery Query(&Encounter, *NavData, Start.Location, End.Location);
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Result = Navigation->FindPathSync(Agent, Query);
    if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
    {
        return false;
    }

    OutCenterPath.Add(FromCenter);
    for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
    {
        const FVector Next = Point.Location + CenterOffset;
        const FVector Previous = OutCenterPath.Last();
        if (!IsSegmentOpen(Encounter, Doors, Previous, Next, Radius))
        {
            OutCenterPath.Reset();
            OutLength = 0.0;
            return false;
        }
        OutCenterPath.Add(Next);
        OutLength += FVector::Dist(Previous, Next);
    }
    return true;
}

// Sample from the exact live center. Invalid projection is unknown, so the caller uses normal timing.
TOptional<double> FirstVisibleETA(const FSceneViewProjectionData& View, const FBox& CurrentBounds,
    TConstArrayView<FVector> CenterPath, float WalkSpeed, float SampleSpacing)
{
    if (CenterPath.IsEmpty() || !FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f
        || !FMath::IsFinite(SampleSpacing) || SampleSpacing <= 0.0f)
    {
        return {};
    }
    const EVisibility CurrentVisibility = ProjectLiveBounds(View, CurrentBounds);
    if (CurrentVisibility == EVisibility::Invalid)
    {
        return {};
    }
    if (CurrentVisibility == EVisibility::Visible)
    {
        return 0.0;
    }

    double Traveled = 0.0;
    int32 SamplesRemaining = 256;
    for (int32 Segment = 1; Segment < CenterPath.Num(); ++Segment)
    {
        const FVector Start = CenterPath[Segment - 1];
        const FVector End = CenterPath[Segment];
        const double Length = FVector::Dist(Start, End);
        const double RequiredSamples = FMath::CeilToDouble(Length / SampleSpacing);
        if (!FMath::IsFinite(RequiredSamples) || RequiredSamples > SamplesRemaining)
        {
            return {};
        }
        const int32 SampleCount = static_cast<int32>(RequiredSamples);
        SamplesRemaining -= SampleCount;
        for (int32 Index = 1; Index <= SampleCount; ++Index)
        {
            const double Alpha = static_cast<double>(Index) / SampleCount;
            const FVector Center = FMath::Lerp(Start, End, Alpha);
            const EVisibility Visibility = ProjectLiveBounds(View, CurrentBounds.ShiftBy(Center - CenterPath[0]));
            if (Visibility == EVisibility::Invalid)
            {
                return {};
            }
            if (Visibility == EVisibility::Visible)
            {
                // Use the earlier end of the sampled span to avoid a redundant group at the ETA boundary.
                return (Traveled + (Alpha - 1.0 / SampleCount) * Length) / WalkSpeed;
            }
        }
        Traveled += Length;
    }
    return TNumericLimits<double>::Max();
}
}
