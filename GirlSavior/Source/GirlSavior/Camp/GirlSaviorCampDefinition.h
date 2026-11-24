#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorCampDefinition.generated.h"

class UGirlSaviorHeroineDefinition;
class UGirlSaviorSupportAbilityDefinition;
class UWorld;

// A catalog match carries the owner and the card data together.
struct FGirlSaviorSupportOffer
{
    const UGirlSaviorHeroineDefinition* Heroine = nullptr;
    const UGirlSaviorSupportAbilityDefinition* Ability = nullptr;
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorCampDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // Героини лагеря. Через их профили каталог находит владельца каждого умения.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
    TArray<TObjectPtr<UGirlSaviorHeroineDefinition>> Roster;

    // Идентификаторы доступных умений в порядке карточек на экране подготовки.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camp")
    TArray<FName> AvailableAbilityIds;

    // Постоянный идентификатор тестовой вылазки для проверки выбора и передачи в боевой маршрут.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
    FName MissionId;

    // Название вылазки в списке миссий.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
    FText MissionName;

    // Краткая цель вылазки для списка миссий.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(MultiLine))
    FText MissionDescription;

    // Карта, которую игра открывает после подтверждения вылазки.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
    TSoftObjectPtr<UWorld> ExpeditionMap;

    // Карта лагеря, куда герой возвращается после исхода вылазки.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission")
    TSoftObjectPtr<UWorld> CampMap;

    // Permit checking authored content during Camp 1, before either map is assigned.
    bool ValidateCatalog(FText& OutError) const;
    bool Validate(FText& OutError) const;

    // Read an offered card after validating the catalog.
    FGirlSaviorSupportOffer FindOffer(FName AbilityId) const;

    // Resolve both IDs atomically in selection order after full launch validation.
    bool ResolveLaunchPair(FName FirstId, FName SecondId,
        FGirlSaviorSupportOffer& OutFirst, FGirlSaviorSupportOffer& OutSecond, FText& OutError) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
