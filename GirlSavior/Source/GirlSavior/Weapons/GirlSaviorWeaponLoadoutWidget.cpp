#include "Weapons/GirlSaviorWeaponLoadoutWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camp/GirlSaviorCampPlayerController.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "GirlSaviorWeaponLoadoutWidget"

void UGirlSaviorWeaponLoadoutButton::Bind(FSimpleDelegate InAction)
{
    Action = MoveTemp(InAction);
    OnClicked.AddUniqueDynamic(this, &ThisClass::Invoke);
}

void UGirlSaviorWeaponLoadoutButton::Unbind()
{
    OnClicked.RemoveDynamic(this, &ThisClass::Invoke);
    Action.Unbind();
}

void UGirlSaviorWeaponLoadoutButton::Invoke()
{
    // The controller may close a widget from a callback, so keep a local copy.
    const FSimpleDelegate Callback = Action;
    Callback.ExecuteIfBound();
}

// Build the native fallback. Blueprint children can supply their own root and call the same intents.
TSharedRef<SWidget> UGirlSaviorWeaponLoadoutWidget::RebuildWidget()
{
    using namespace GirlSavior::CampUI;
    SetIsFocusable(true);
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }

    UWidgetTree& Tree = *WidgetTree;
    UVerticalBox* Column = nullptr;
    MountModal(Tree, MakePage(Tree, 810.f, Column));
    Column->AddChildToVerticalBox(Text(Tree, LOCTEXT("Title", "Набор оружия"), 29, Ink, true));
    Column->AddChildToVerticalBox(Text(Tree, LOCTEXT("Instructions", "Выберите оружие, затем слот."), 14, Ink))
        ->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
    CardsRow = Tree.ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(CardsRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));

    auto* SlotsRow = Tree.ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(SlotsRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
    for (int32 Index = 0; Index < FGirlSaviorWeaponLoadout::SlotCount; ++Index)
    {
        auto* SlotColumn = Tree.ConstructWidget<UVerticalBox>();
        auto* ColumnSlot = SlotsRow->AddChildToHorizontalBox(SlotColumn);
        ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ColumnSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
        const FName SlotName(*FString::Printf(TEXT("WeaponSlot%d"), Index + 1));
        auto* SlotButton = Tree.ConstructWidget<UGirlSaviorWeaponLoadoutButton>(UGirlSaviorWeaponLoadoutButton::StaticClass(), SlotName);
        StyleButton(*SlotButton, Field);
        auto* Inset = Border(Tree, FMargin(12.f, 8.f), FLinearColor::Transparent);
        auto* Label = Text(Tree, FText::GetEmpty(), 14, Ink, true, 240.f);
        Inset->SetContent(Label);
        SlotButton->SetContent(Inset);
        SlotButton->Bind(FSimpleDelegate::CreateUObject(this, &ThisClass::RequestSlot, Index));
        SlotColumn->AddChildToVerticalBox(SlotButton);
        SlotButtons.Add(SlotButton);
        SlotLabels.Add(Label);

        const FName RemoveName(*FString::Printf(TEXT("RemoveWeaponSlot%d"), Index + 1));
        auto* Remove = Tree.ConstructWidget<UGirlSaviorWeaponLoadoutButton>(UGirlSaviorWeaponLoadoutButton::StaticClass(), RemoveName);
        StyleButton(*Remove, Field);
        Remove->SetContent(Text(Tree, LOCTEXT("Remove", "Убрать"), 13, Ink, true));
        Remove->Bind(FSimpleDelegate::CreateUObject(this, &ThisClass::RequestRemoval, Index));
        SlotColumn->AddChildToVerticalBox(Remove)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
        RemoveButtons.Add(Remove);
    }
    StatusLabel = Text(Tree, FText::GetEmpty(), 14, Accent, false, 790.f, TEXT("WeaponLoadoutStatus"));
    Column->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    CancelButton = ActionButton(Tree, TEXT("WeaponLoadoutCancel"), LOCTEXT("Cancel", "Отмена"));
    ConfirmButton = ActionButton(Tree, TEXT("WeaponLoadoutConfirm"), LOCTEXT("Confirm", "Подтвердить"), true);
    CancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestBack);
    ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestConfirmation);
    AddActions(Tree, *Column, CancelButton, ConfirmButton);
    return Super::RebuildWidget();
}

// The catalog remains fixed while the controller edits a draft.
void UGirlSaviorWeaponLoadoutWidget::BuildCards()
{
    using namespace GirlSavior::CampUI;
    UWidgetTree& Tree = *WidgetTree;
    for (const auto& Card : CurrentCards)
    {
        const FName WidgetName(*FString::Printf(TEXT("WeaponCard_%s"), *Card.WeaponId.ToString()));
        auto* Button = Tree.ConstructWidget<UGirlSaviorWeaponLoadoutButton>(UGirlSaviorWeaponLoadoutButton::StaticClass(), WidgetName);
        auto* Body = Tree.ConstructWidget<UVerticalBox>();
        auto* Inset = Border(Tree, FMargin(12.f, 10.f), FLinearColor::Transparent);
        Inset->SetContent(Body);
        Button->SetContent(Inset);
        auto* Header = Tree.ConstructWidget<UHorizontalBox>();
        Body->AddChildToVerticalBox(Header);
        auto* Icon = AddImage(Tree, *Header, 43.f);
        Icon->SetBrushFromTexture(Card.Icon);
        Header->AddChildToHorizontalBox(Text(Tree, Card.Name, 16, Ink, true, 170.f))
            ->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
        Body->AddChildToVerticalBox(Text(Tree, Card.Description, 13, Ink, false, 225.f))
            ->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
        if (!Card.LockReason.IsEmpty())
        {
            Body->AddChildToVerticalBox(Text(Tree, Card.LockReason, 13, Accent, true, 225.f))
                ->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
        }
        Button->Bind(FSimpleDelegate::CreateUObject(this, &ThisClass::RequestCard, Card.WeaponId));
        auto* CardSlot = CardsRow->AddChildToHorizontalBox(Button);
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CardSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
        CardButtons.Add(Card.WeaponId, Button);
    }
}

// A redraw changes only presentation. It never writes a profile or replaces an active focus target.
void UGirlSaviorWeaponLoadoutWidget::Refresh(TConstArrayView<FGirlSaviorWeaponCardView> Cards,
    TConstArrayView<FText> Slots, TConstArrayView<FName> SlotIds, bool bHasSelectedCard, bool bCanConfirm, const FText& Status)
{
    using namespace GirlSavior::CampUI;
    CurrentCards.Reset(Cards.Num());
    CurrentCards.Append(Cards.GetData(), Cards.Num());
    CurrentSlots.Reset(Slots.Num());
    CurrentSlots.Append(Slots.GetData(), Slots.Num());
    if (CardsRow)
    {
        if (CardButtons.IsEmpty())
        {
            BuildCards();
        }
        for (const auto& Card : CurrentCards)
        {
            if (auto* Button = GetCardButton(Card.WeaponId))
            {
                StyleButton(*Button, Card.bSelected ? Selected : Field);
                Button->SetIsEnabled(Card.bOwned);
            }
        }
        for (int32 Index = 0; Index < SlotButtons.Num(); ++Index)
        {
            SlotLabels[Index]->SetText(Slots[Index]);
            SlotButtons[Index]->SetIsEnabled(bHasSelectedCard);
            RemoveButtons[Index]->SetIsEnabled(!SlotIds[Index].IsNone());
        }
        ConfirmButton->SetIsEnabled(bCanConfirm);
        StatusLabel->SetText(Status);
    }
    OnLoadoutRefreshed(bCanConfirm, Status);
}

void UGirlSaviorWeaponLoadoutWidget::NativeDestruct()
{
    for (auto& Entry : CardButtons)
    {
        Entry.Value->Unbind();
    }
    for (auto* Group : {&SlotButtons, &RemoveButtons})
    {
        for (auto& Button : *Group)
        {
            Button->Unbind();
        }
    }
    if (ConfirmButton && CancelButton)
    {
        ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestConfirmation);
        CancelButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestBack);
    }
    Super::NativeDestruct();
}

void UGirlSaviorWeaponLoadoutWidget::FocusInitialAction()
{
    if (auto* Controller = GetOwningPlayer())
    {
        for (const auto& Card : CurrentCards)
        {
            if (auto* Button = GetCardButton(Card.WeaponId); Button && Button->GetIsEnabled())
            {
                Button->SetUserFocus(Controller);
                return;
            }
        }
        (CancelButton ? static_cast<UWidget*>(CancelButton.Get()) : this)->SetUserFocus(Controller);
    }
}

UButton* UGirlSaviorWeaponLoadoutWidget::GetCardButton(FName WeaponId) const
{
    const auto* Button = CardButtons.Find(WeaponId);
    return Button ? Button->Get() : nullptr;
}

UButton* UGirlSaviorWeaponLoadoutWidget::GetSlotButton(int32 Index) const
{
    return SlotButtons.IsValidIndex(Index) ? SlotButtons[Index].Get() : nullptr;
}

UButton* UGirlSaviorWeaponLoadoutWidget::GetRemoveButton(int32 Index) const
{
    return RemoveButtons.IsValidIndex(Index) ? RemoveButtons[Index].Get() : nullptr;
}

void UGirlSaviorWeaponLoadoutWidget::RequestCard(FName WeaponId)
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->SelectWeaponCard(WeaponId);
    }
}

void UGirlSaviorWeaponLoadoutWidget::RequestSlot(int32 Index)
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->PlaceSelectedWeapon(Index);
    }
}

void UGirlSaviorWeaponLoadoutWidget::RequestRemoval(int32 Index)
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->RemoveWeapon(Index);
    }
}

void UGirlSaviorWeaponLoadoutWidget::RequestConfirmation()
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->ConfirmWeapons();
    }
}

#undef LOCTEXT_NAMESPACE
