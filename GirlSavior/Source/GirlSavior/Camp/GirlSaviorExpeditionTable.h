#pragma once

#include "Camp/GirlSaviorCampInteractable.h"
#include "GirlSaviorExpeditionTable.generated.h"

class UGirlSaviorCampPrototypeDefinition;

UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorExpeditionTable : public AGirlSaviorCampInteractable
{
    GENERATED_BODY()

public:
    const UGirlSaviorCampPrototypeDefinition* GetCatalog() const { return Catalog; }
    virtual bool IsInteractionEnabled() const override;
    virtual void Interact(AGirlSaviorCampPlayerController& Controller) const override;

protected:
    virtual FText GetInteractionName() const override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Camp")
    TObjectPtr<UGirlSaviorCampPrototypeDefinition> Catalog;
};
