#include "Encounters/GirlSaviorCombatRoom.h"

#include "Combat/GirlSaviorCombatChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavAreas/NavArea_Null.h"
#include "UObject/ConstructorHelpers.h"

void UGirlSaviorDoorNavModifier::CalculateBounds() const
{
    Bounds = FBox(ForceInit);
    ComponentBounds.Reset();
    const auto* Door = Cast<AGirlSaviorCombatDoor>(GetOwner());
    if (!Door || !Door->Bounds)
    {
        return;
    }

    const UBoxComponent& Box = *Door->Bounds;
    const FQuat Rotation = Box.GetComponentQuat();
    const FVector WorldCenter = Box.GetComponentLocation();
    Bounds = Box.CalcBounds(Box.GetComponentTransform()).GetBox();
    ComponentBounds.Emplace(FBox::BuildAABB(Rotation.UnrotateVector(WorldCenter), Box.GetScaledBoxExtent()), Rotation);
}

AGirlSaviorCombatDoor::AGirlSaviorCombatDoor()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBounds"));
    SetRootComponent(Bounds);
    Bounds->SetMobility(EComponentMobility::Movable);
    Bounds->InitBoxExtent(FVector(20.0, 170.0, 150.0));
    Bounds->SetCollisionObjectType(ECC_WorldDynamic);
    Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Bounds->SetCollisionResponseToChannel(GirlSaviorCombatChannels::HeroProjectile, ECR_Block);
    Bounds->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Block);
    Bounds->SetCollisionResponseToChannel(GirlSaviorCombatChannels::Occluder, ECR_Block);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetCanEverAffectNavigation(false);
    Bounds->CanCharacterStepUpOn = ECB_No;

    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorVisual"));
    Visual->SetupAttachment(Bounds);
    Visual->SetMobility(EComponentMobility::Movable);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Visual->SetStaticMesh(Cube.Object);
    }

    NavModifier = CreateDefaultSubobject<UGirlSaviorDoorNavModifier>(TEXT("ClosedNavArea"));
    NavModifier->SetAreaClass(UNavArea_Null::StaticClass());
    NavModifier->SetNavigationRelevancy(false);
}

void AGirlSaviorCombatDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    SetOpen(bStartsOpen);
}

void AGirlSaviorCombatDoor::BeginPlay()
{
    Super::BeginPlay();
    SetOpen(bStartsOpen);
}

// Block immediately. The nav modifier then catches up without rebaking the floor.
void AGirlSaviorCombatDoor::SetOpen(bool bNewOpen)
{
    bOpen = bNewOpen;
    Visual->SetRelativeScale3D(Bounds->GetUnscaledBoxExtent() / 50.0);
    Bounds->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
    Visual->SetVisibility(!bOpen, true);
    NavModifier->SetNavigationRelevancy(!bOpen);
}

// Use the rotated authored volume even when a route still uses a stale nav tile.
bool AGirlSaviorCombatDoor::Crosses(const FVector& Start, const FVector& End, float Radius) const
{
    if (bOpen)
    {
        return false;
    }
    const FTransform& Transform = Bounds->GetComponentTransform();
    const FVector LocalStart = Transform.InverseTransformPositionNoScale(Start);
    const FVector LocalEnd = Transform.InverseTransformPositionNoScale(End);
    const FVector Extent = Bounds->GetScaledBoxExtent() + FVector(FMath::Max(0.f, Radius));
    const FBox Box(-Extent, Extent);
    return Box.IsInsideOrOn(LocalStart) || Box.IsInsideOrOn(LocalEnd)
        || FMath::LineBoxIntersection(Box, LocalStart, LocalEnd, LocalEnd - LocalStart);
}

AGirlSaviorCombatRoom::AGirlSaviorCombatRoom()
{
    Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomVolume"));
    SetRootComponent(Volume);
    Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
    Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Volume->SetGenerateOverlapEvents(true);
    Volume->SetCanEverAffectNavigation(false);
}

// Resolve overlaps and threshold ties using the pawn center, independent of capsule size.
bool AGirlSaviorCombatRoom::ContainsPoint(const FVector& WorldPoint) const
{
    const FVector Extent = Volume->GetScaledBoxExtent();
    const FVector Local = Volume->GetComponentTransform().InverseTransformPositionNoScale(WorldPoint);
    return FBox(-Extent, Extent).IsInsideOrOn(Local);
}

bool AGirlSaviorCombatRoom::IsAdjacentTo(const AGirlSaviorCombatRoom& Other) const
{
    return Neighbors.ContainsByPredicate([&Other](const FGirlSaviorRoomLink& Link) { return Link.Room == &Other; });
}

bool AGirlSaviorCombatRoom::HasOpenLinkTo(const AGirlSaviorCombatRoom& Other) const
{
    return Neighbors.ContainsByPredicate([&Other](const FGirlSaviorRoomLink& Link)
    {
        return Link.Room == &Other && (!Link.Door || Link.Door->IsOpen());
    });
}
