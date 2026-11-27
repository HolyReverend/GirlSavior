#pragma once

#include "Camp/GirlSaviorCampWidgets.h"
#include "Components/Button.h"
#include "GirlSaviorWeaponLoadoutWidget.generated.h"

class UHorizontalBox;

// Bind native focusable buttons to intents with a stable weapon or slot argument.
UCLASS()
class UGirlSaviorWeaponLoadoutButton : public UButton
{
    GENERATED_BODY()

public:
    void Bind(FSimpleDelegate InAction);
    void Unbind();

private:
    UFUNCTION()
    void Invoke();

    FSimpleDelegate Action;
};

USTRUCT(BlueprintType)
struct FGirlSaviorWeaponCardView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    FName WeaponId;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    FText Name;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    FText LockReason;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    bool bOwned = false;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    bool bSelected = false;

    UPROPERTY(BlueprintReadOnly, Category="Weapons")
    int32 DisplayOrder = 0;
};

UCLASS(Blueprintable)
class GIRLSAVIOR_API UGirlSaviorWeaponLoadoutWidget : public UGirlSaviorCampModalWidget
{
    GENERATED_BODY()

public:
    void Refresh(TConstArrayView<FGirlSaviorWeaponCardView> Cards, TConstArrayView<FText> Slots,
        TConstArrayView<FName> SlotIds, bool bHasSelectedCard, bool bCanConfirm, const FText& Status);
    void FocusInitialAction();

    UButton* GetCardButton(FName WeaponId) const;
    UButton* GetSlotButton(int32 Index) const;
    UButton* GetRemoveButton(int32 Index) const;
    UButton* GetConfirmButton() const { return ConfirmButton; }
    UButton* GetCancelButton() const { return CancelButton; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category="Weapons")
    void RequestCard(FName WeaponId);

    UFUNCTION(BlueprintCallable, Category="Weapons")
    void RequestSlot(int32 Index);

    UFUNCTION(BlueprintCallable, Category="Weapons")
    void RequestRemoval(int32 Index);

    UFUNCTION(BlueprintCallable, Category="Weapons")
    void RequestConfirmation();

    UFUNCTION(BlueprintImplementableEvent, Category="Weapons")
    void OnLoadoutRefreshed(bool bCanConfirm, const FText& Status);

    UPROPERTY(BlueprintReadOnly, Transient, Category="Weapons")
    TArray<FGirlSaviorWeaponCardView> CurrentCards;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Weapons")
    TArray<FText> CurrentSlots;

private:
    void BuildCards();

    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> CardsRow;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGirlSaviorWeaponLoadoutButton>> CardButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGirlSaviorWeaponLoadoutButton>> SlotButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGirlSaviorWeaponLoadoutButton>> RemoveButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SlotLabels;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusLabel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ConfirmButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CancelButton;
};
