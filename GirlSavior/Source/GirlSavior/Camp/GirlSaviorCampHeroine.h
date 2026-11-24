#pragma once

#include "Camp/GirlSaviorCampInteractable.h"
#include "GirlSaviorCampHeroine.generated.h"

class UGirlSaviorHeroineDefinition;

UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCampHeroine : public AGirlSaviorCampInteractable
{
    GENERATED_BODY()

public:
    virtual bool IsInteractionEnabled() const override;
    virtual void Interact(AGirlSaviorCampPlayerController& Controller) const override;

protected:
    virtual FText GetInteractionName() const override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Camp")
    TObjectPtr<UGirlSaviorHeroineDefinition> HeroineDefinition;
};
