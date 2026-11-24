#pragma once

#include "Camp/GirlSaviorCampPreparation.h"
#include "Core/GirlSaviorPlayerController.h"
#include "GirlSaviorCampPlayerController.generated.h"

class AGirlSaviorCampInteractable;
class AGirlSaviorExpeditionTable;
class UGirlSaviorCampHeroineInfoWidget;
class UGirlSaviorCampPreparationWidget;
class UGirlSaviorCampPromptWidget;
class UGirlSaviorCampDefinition;
class UGirlSaviorHeroineDefinition;
class UGirlSaviorExpeditionSessionSubsystem;
class UUserWidget;

enum class EGirlSaviorCampPage : uint8
{
    Walking,
    Missions,
    Preparation,
    Heroine
};

UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCampPlayerController : public AGirlSaviorPlayerController
{
    GENERATED_BODY()

public:
    void OpenExpeditionTable(const UGirlSaviorCampDefinition& Catalog);
    void OpenHeroineInfo(const UGirlSaviorHeroineDefinition& Heroine);
    void OpenPreparationPage();
    void ToggleAbility(FName AbilityId);
    void RequestBack();
    void RequestLaunch();

    EGirlSaviorCampPage GetCurrentPage() const { return Page; }
    const FGirlSaviorCampPreparation& GetDraft() const { return Draft; }
    const AGirlSaviorCampInteractable* GetDisplayedTarget() const { return DisplayedTarget.Get(); }
    const UUserWidget* GetModalWidget() const { return ModalWidget; }

protected:
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool IsDashAllowed() const override { return false; }

    UPROPERTY(EditDefaultsOnly, Category="Camp|Input")
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditDefaultsOnly, Category="Camp|UI")
    TSubclassOf<UGirlSaviorCampPromptWidget> PromptWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="Camp|UI")
    TSubclassOf<UGirlSaviorCampPreparationWidget> PreparationWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="Camp|UI")
    TSubclassOf<UGirlSaviorCampHeroineInfoWidget> HeroineInfoWidgetClass;

private:
    bool ValidateInteractionIds();
    AGirlSaviorCampInteractable* PickInteractionTarget() const;
    void PollInteraction();
    void SetDisplayedTarget(AGirlSaviorCampInteractable* Target);
    void OnInteractPressed();
    bool PresentModal(UUserWidget* Widget, EGirlSaviorCampPage NewPage);
    void RefreshPreparation(const FText& Status = FText::GetEmpty());
    void CloseModal();
    void ResetPanel();
    void ResetCamp();
    UGirlSaviorExpeditionSessionSubsystem* GetExpeditionSession() const;
    bool IsLaunchInProgress() const;

    UPROPERTY(Transient)
    TObjectPtr<UGirlSaviorCampPromptWidget> PromptWidget;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ModalWidget;

    TWeakObjectPtr<AGirlSaviorCampInteractable> DisplayedTarget;
    TWeakObjectPtr<AGirlSaviorExpeditionTable> ExpeditionTable;
    TWeakObjectPtr<const UGirlSaviorCampDefinition> ActiveCatalog;
    FTimerHandle InteractionPoll;
    EGirlSaviorCampPage Page = EGirlSaviorCampPage::Walking;
    FGirlSaviorCampPreparation Draft;
};
