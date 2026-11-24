#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorSupportAbilityDefinition.generated.h"

class UGirlSaviorSupportEffectDefinition;
class UTexture2D;

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorSupportAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // Постоянный идентификатор умения для списка предложений, выбора и передачи в вылазку.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Support")
    FName AbilityId;

    // Заголовок карточки умения.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Support")
    FText DisplayName;

    // Текст карточки об условиях срабатывания и назначении умения.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Support", meta=(MultiLine))
    FText Description;

    // Значок, по которому игрок узнаёт умение в списке и выбранном слоте.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Support")
    TObjectPtr<UTexture2D> Icon;

    // Встроенные механики умения. Их значения определяют эффект в бою и числа на карточке.
    UPROPERTY(EditAnywhere, Instanced, Category="Support")
    TArray<TObjectPtr<UGirlSaviorSupportEffectDefinition>> Effects;

    bool ValidateIdentity(FText& OutError) const;
    bool Validate(FText& OutError) const;
    static bool ValidatePair(const UGirlSaviorSupportAbilityDefinition& First,
        const UGirlSaviorSupportAbilityDefinition& Second, FText& OutError);

    // Render current authored values each time a card refreshes.
    UFUNCTION(BlueprintPure, Category="Support")
    FText BuildDetailsText() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
