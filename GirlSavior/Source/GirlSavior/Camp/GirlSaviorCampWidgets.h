#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GirlSaviorCampWidgets.generated.h"

class UButton;
class UGirlSaviorCampPrototypeDefinition;
class UGirlSaviorHeroineDefinition;
class UGirlSaviorSupportAbilityDefinition;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidgetSwitcher;

UCLASS(Blueprintable)
class GIRLSAVIOR_API UGirlSaviorCampPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Empty text hides the prompt while leaving gameplay hit testing alone.
    void SetPrompt(const FText& Prompt);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UFUNCTION(BlueprintImplementableEvent, Category="Camp")
    void OnPromptChanged(const FText& Prompt);

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PromptText;

    FText CurrentPrompt;
};

UCLASS(Abstract, Blueprintable)
class GIRLSAVIOR_API UGirlSaviorCampModalWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

    UFUNCTION(BlueprintCallable, Category="Camp")
    void RequestBack();
};

USTRUCT()
struct FGirlSaviorCampCardWidgets
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<UButton> Button;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Icon;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Heroine;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Title;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Details;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Description;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Action;

    FName AbilityId;
};

USTRUCT(BlueprintType)
struct FGirlSaviorCampCardView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    FName AbilityId;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    FText HeroineName;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    FText Title;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    FText Details;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category="Camp")
    bool bSelected = false;
};

UCLASS(Blueprintable)
class GIRLSAVIOR_API UGirlSaviorCampPreparationWidget : public UGirlSaviorCampModalWidget
{
    GENERATED_BODY()

public:
    // Render the controller's current page and draft from the read-only catalog.
    void Refresh(const UGirlSaviorCampPrototypeDefinition& Catalog, TConstArrayView<FName> SelectedIds,
        bool bShowPreparation, bool bValidSelection, const FText& Status);
    void FocusAfterLaunchFailure();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UFUNCTION(BlueprintImplementableEvent, Category="Camp")
    void OnPreparationRefreshed(bool bShowPreparation, bool bValidSelection, const FText& Status);

    UFUNCTION(BlueprintCallable, Category="Camp")
    void RequestPreparationPage();

    UFUNCTION(BlueprintCallable, Category="Camp")
    void RequestAbility(FName AbilityId);

    UFUNCTION(BlueprintCallable, Category="Camp")
    void RequestLaunch();

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    FText CurrentMissionName;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    FText CurrentMissionDescription;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    TArray<FGirlSaviorCampCardView> CurrentOffers;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    TArray<FName> CurrentSelection;

private:
    void RequestCard(int32 Index);

    UFUNCTION()
    void HandleFirstCard();

    UFUNCTION()
    void HandleSecondCard();

    UFUNCTION()
    void HandleThirdCard();

    UPROPERTY(Transient)
    TObjectPtr<UWidgetSwitcher> PageSwitcher;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionName;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionDescription;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MissionStatus;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ConfigureButton;

    UPROPERTY(Transient)
    TArray<FGirlSaviorCampCardWidgets> CardWidgets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SelectionSlots;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SelectionCount;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SelectionStatus;

    UPROPERTY(Transient)
    TObjectPtr<UButton> LaunchButton;
};

UCLASS(Blueprintable)
class GIRLSAVIOR_API UGirlSaviorCampHeroineInfoWidget : public UGirlSaviorCampModalWidget
{
    GENERATED_BODY()

public:
    // A heroine profile is read-only. Selection belongs to expedition preparation.
    void SetHeroine(const UGirlSaviorHeroineDefinition& Heroine, const UGirlSaviorSupportAbilityDefinition* Ability);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UFUNCTION(BlueprintImplementableEvent, Category="Camp")
    void OnHeroineShown(const UGirlSaviorHeroineDefinition* Heroine, const UGirlSaviorSupportAbilityDefinition* Ability);

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Name;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Role;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Description;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Portrait;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Skill;
};
