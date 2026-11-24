#include "Camp/GirlSaviorCampInteractable.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"

// The sphere only identifies candidates. Range, projection and line of sight use the same anchor.
AGirlSaviorCampInteractable::AGirlSaviorCampInteractable()
{
    InteractionAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionAnchor"));
    SetRootComponent(InteractionAnchor);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(InteractionAnchor);
    InteractionSphere->InitSphereRadius(InteractionRange);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionSphere->SetGenerateOverlapEvents(true);
    InteractionSphere->SetCanEverAffectNavigation(false);
}

// Keep the overlap broad phase at least as wide as the explicit interaction range.
void AGirlSaviorCampInteractable::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (InteractionSphere->GetScaledSphereRadius() < InteractionRange && InteractionSphere->GetShapeScale() > 0.f)
    {
        InteractionSphere->SetSphereRadius(InteractionRange / InteractionSphere->GetShapeScale());
    }
}

// Keep the prompt and trace endpoint independent of the visual mesh placement.
FVector AGirlSaviorCampInteractable::GetInteractionLocation() const
{
    return InteractionAnchor->GetComponentLocation();
}

// All Camp targets share the current interaction key and a localized target name.
FText AGirlSaviorCampInteractable::GetPrompt() const
{
    return FText::Format(NSLOCTEXT("GirlSaviorCampInteraction", "Prompt", "E — {0}"), GetInteractionName());
}
