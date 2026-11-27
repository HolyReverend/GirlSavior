#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "GirlSaviorCombatWidgets.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

UCLASS()
class GIRLSAVIOR_API UGirlSaviorCombatVitalsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetVitals(double Health, double HealthLimit, double Barrier, double InitialBarrier);
    void SetDash(const FText& Status, const FLinearColor& Color);
    void SetWeaponView(const FGirlSaviorWeaponView& InView);
    const FGirlSaviorWeaponView& GetDisplayedWeaponView() const { return DisplayedWeaponView; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    double GetDisplayedHealth() const { return DisplayedHealth; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    double GetDisplayedMaxHealth() const { return DisplayedMaxHealth; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    double GetDisplayedBarrier() const { return DisplayedBarrier; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    bool IsBarrierVisible() const { return DisplayedMaxBarrier > 0.0; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    FText GetDisplayedDashText() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor HealthColor = FLinearColor(0.80f, 0.24f, 0.24f);

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    FLinearColor BarrierColor = FLinearColor(0.23f, 0.73f, 0.81f);

private:
    void RefreshVitals();
    void BuildWeaponStrip(UCanvasPanel& Root);
    void RefreshWeaponStrip();

    UPROPERTY(Transient)
    FGirlSaviorWeaponView DisplayedWeaponView;

    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> WeaponStrip;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> WeaponSlotFrames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> WeaponActiveMarkers;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> WeaponSlotNames;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UImage>> WeaponSlotIcons;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BarrierText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DashText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> BarrierBar;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> BarrierGroup;

    double DisplayedHealth = 0.0;
    double DisplayedMaxHealth = 0.0;
    double DisplayedBarrier = 0.0;
    double DisplayedMaxBarrier = 0.0;
    FText DashStatus;
    FLinearColor DashColor = FLinearColor::White;
};

UCLASS()
class GIRLSAVIOR_API UGirlSaviorCombatOutcomeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ShowDeathNotice(const FText& Cause);
    void ShowResult(bool bDefeated, const FText& Cause, const FText& Action);
    void ShowRecovery(const FText& Message, const FText& Action);
    void SetActionFailure(const FText& Message);
    void ClearResult();
    UButton* GetRetryButton() const { return RetryButton; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    bool IsDeathNoticeVisible() const;

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    bool IsOutcomePanelVisible() const;

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    FText GetDisplayedTitle() const;

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    FText GetDisplayedCause() const;
    FText GetDisplayedActionLabel() const;
    FText GetActionStatus() const;

    FSimpleMulticastDelegate OnRetryRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UFUNCTION()
    void HandleRetryClicked();
    void PresentResult(const FText& Title, const FText& Detail, const FText& Action);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> DeathNotice;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> NoticeCause;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> ResultBackdrop;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> ResultPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResultTitle;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResultCause;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResultActionStatus;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResultActionLabel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> RetryButton;
};
