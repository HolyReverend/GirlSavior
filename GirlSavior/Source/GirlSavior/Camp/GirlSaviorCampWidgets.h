#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GirlSaviorCampWidgets.generated.h"

class UButton;
class UBorder;
class UGirlSaviorCampDefinition;
class UGirlSaviorHeroineDefinition;
class UGirlSaviorSupportAbilityDefinition;
class UImage;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;
class UWidgetSwitcher;
class UVerticalBox;

namespace GirlSavior::CampUI
{
// Native table pages share the same palette and layout helpers.
extern const FLinearColor Field;
extern const FLinearColor Ink;
extern const FLinearColor Accent;
extern const FLinearColor Selected;
UTextBlock* Text(UWidgetTree& Tree, const FText& Value, int32 Size, const FLinearColor& Color = Ink,
    bool bBold = false, float WrapWidth = 0.f, FName Name = NAME_None);
UBorder* Border(UWidgetTree& Tree, const FMargin& Padding, const FLinearColor& Color);
void MountModal(UWidgetTree& Tree, UWidget* Content);
UBorder* MakePage(UWidgetTree& Tree, float Width, UVerticalBox*& Column);
void StyleButton(UButton& Button, const FLinearColor& Color);
UButton* ActionButton(UWidgetTree& Tree, FName Name, const FText& Label, bool bPrimary = false);
UImage* AddImage(UWidgetTree& Tree, UHorizontalBox& Row, float Size);
void AddActions(UWidgetTree& Tree, UVerticalBox& Column, UButton* Back, UButton* Forward);
}

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
    void Refresh(const UGirlSaviorCampDefinition& Catalog, TConstArrayView<FName> SelectedIds,
        bool bShowPreparation, bool bValidSelection, const FText& Status,
        TConstArrayView<FText> WeaponSlots, bool bEnableWeaponPage, bool bValidCatalog);
    void FocusAfterLaunchFailure();
    void FocusWeaponEntry(bool bShowPreparation);
    UButton* GetWeaponEntryButton(bool bShowPreparation) const;
    UButton* GetLaunchButton() const { return LaunchButton; }

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

    UFUNCTION(BlueprintCallable, Category="Camp")
    void RequestWeaponLoadout();

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    FText CurrentMissionName;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    FText CurrentMissionDescription;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    TArray<FGirlSaviorCampCardView> CurrentOffers;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    TArray<FName> CurrentSelection;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Camp")
    TArray<FText> CurrentWeaponSlots;

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
    TObjectPtr<UButton> MissionWeaponsButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> PreparationWeaponsButton;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> WeaponSlotPreviews;

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
