#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorHeroineDefinition.generated.h"

class UGirlSaviorSupportAbilityDefinition;
class UTexture2D;

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorHeroineDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // Постоянный идентификатор героини для каталога и привязки её умений.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine")
    FName HeroineId;

    // Имя героини для подписи в лагере, профиля и карточки умения.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine")
    FText DisplayName;

    // Краткое описание героини для панели знакомства.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine", meta=(MultiLine))
    FText Description;

    // Краткая подпись о том, какую поддержку даёт героиня.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine")
    FText SupportRole;

    // Необязательный портрет для панели знакомства.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine")
    TObjectPtr<UTexture2D> Portrait;

    // Умения, принадлежащие героине. Каталог лагеря определяет, какие из них доступны для выбора.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Heroine")
    TArray<TObjectPtr<UGirlSaviorSupportAbilityDefinition>> Abilities;

    bool Validate(FText& OutError) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
