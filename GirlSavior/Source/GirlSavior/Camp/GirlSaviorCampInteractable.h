#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCampInteractable.generated.h"

class AGirlSaviorCampPlayerController;
class USceneComponent;
class USphereComponent;

UCLASS(Abstract, Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCampInteractable : public AActor
{
    GENERATED_BODY()

public:
    static constexpr float InteractionRange = 220.f;

    AGirlSaviorCampInteractable();
    FName GetInteractionId() const { return InteractionId; }
    FVector GetInteractionLocation() const;
    virtual bool IsInteractionEnabled() const { return bInteractionEnabled; }
    virtual FText GetPrompt() const;
    virtual void Interact(AGirlSaviorCampPlayerController& Controller) const
        PURE_VIRTUAL(AGirlSaviorCampInteractable::Interact, );

protected:
    virtual void PostInitializeComponents() override;
    virtual FText GetInteractionName() const PURE_VIRTUAL(AGirlSaviorCampInteractable::GetInteractionName, return FText::GetEmpty(););

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camp")
    TObjectPtr<USceneComponent> InteractionAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camp")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Camp")
    FName InteractionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
    bool bInteractionEnabled = true;
};
