#include "Camp/GirlSaviorCampWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Camp/GirlSaviorCampPlayerController.h"
#include "Camp/GirlSaviorCampPrototypeDefinition.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Support/GirlSaviorHeroineDefinition.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCampWidgets"

namespace GirlSavior::CampUI
{
const FLinearColor Backdrop(0.010f, 0.012f, 0.009f, 0.69f);
const FLinearColor Panel(0.030f, 0.038f, 0.028f, 0.99f);
const FLinearColor Field(0.061f, 0.080f, 0.058f);
const FLinearColor Ink(0.88f, 0.87f, 0.77f);
const FLinearColor Muted(0.56f, 0.61f, 0.49f);
const FLinearColor Accent(0.96f, 0.66f, 0.30f);
const FLinearColor Selected(0.16f, 0.22f, 0.11f);
const FLinearColor Primary(0.60f, 0.34f, 0.12f);

// Give every dynamic text field the same camp palette and use fixed wrapping for long descriptions.
UTextBlock* Text(UWidgetTree& Tree, const FText& Value, int32 Size, const FLinearColor& Color = Ink,
    bool bBold = false, float WrapWidth = 0.f, FName Name = NAME_None)
{
    auto* Widget = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Widget->SetText(Value);
    Widget->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
    Widget->SetColorAndOpacity(Color);
    if (WrapWidth > 0.f)
    {
        Widget->SetWrapTextAt(WrapWidth);
    }
    return Widget;
}

// All panels use the same plain brush so only color and padding distinguish them.
UBorder* Border(UWidgetTree& Tree, const FMargin& Padding, const FLinearColor& Color)
{
    auto* Widget = Tree.ConstructWidget<UBorder>();
    Widget->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
    Widget->SetBrushColor(Color);
    Widget->SetPadding(Padding);
    return Widget;
}

// Build a centered, scalable panel over an overlay that captures background clicks.
void MountModal(UWidgetTree& Tree, UWidget* Content)
{
    auto* Root = Tree.ConstructWidget<UOverlay>();
    Tree.RootWidget = Root;
    auto* BackdropSlot = Root->AddChildToOverlay(Border(Tree, FMargin(), Backdrop));
    BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
    BackdropSlot->SetVerticalAlignment(VAlign_Fill);

    auto* Scale = Tree.ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Scale->SetContent(Content);
    auto* ContentSlot = Root->AddChildToOverlay(Scale);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    ContentSlot->SetPadding(FMargin(24.f));
}

// Size each page independently so the mission and profile panels stay compact.
UBorder* MakePage(UWidgetTree& Tree, float Width, UVerticalBox*& Column)
{
    auto* Frame = Border(Tree, FMargin(1.f), FLinearColor(0.30f, 0.26f, 0.14f));
    auto* Surface = Border(Tree, FMargin(26.f, 21.f), Panel);
    Frame->SetContent(Surface);
    auto* WidthBox = Tree.ConstructWidget<USizeBox>();
    WidthBox->SetWidthOverride(Width);
    Surface->SetContent(WidthBox);
    Column = Tree.ConstructWidget<UVerticalBox>();
    WidthBox->SetContent(Column);
    return Frame;
}

// Keep primary actions and selectable cards visually distinct using their normal and hover states.
void StyleButton(UButton& Button, const FLinearColor& Color)
{
    FButtonStyle Style;
    Style.SetNormal(FSlateColorBrush(Color));
    Style.SetHovered(FSlateColorBrush(Color * FLinearColor(1.50f, 1.40f, 1.30f, 1.f)));
    Style.SetPressed(FSlateColorBrush(Color * FLinearColor(0.85f, 0.85f, 0.85f, 1.f)));
    Style.SetDisabled(FSlateColorBrush(FLinearColor(0.083f, 0.088f, 0.070f)));
    Button.SetStyle(Style);
}

// Give action buttons stable names for editor inspection and PIE tests.
UButton* ActionButton(UWidgetTree& Tree, FName Name, const FText& Label, bool bPrimary = false)
{
    auto* Button = Tree.ConstructWidget<UButton>(UButton::StaticClass(), Name);
    StyleButton(*Button, bPrimary ? Primary : Field);
    auto* Inset = Border(Tree, FMargin(14.f, 8.f), FLinearColor::Transparent);
    Inset->SetContent(Text(Tree, Label, 14, Ink, true));
    Button->SetContent(Inset);
    return Button;
}

// Create a square image slot while preserving the card's line heights.
UImage* AddImage(UWidgetTree& Tree, UHorizontalBox& Row, float Size)
{
    auto* Box = Tree.ConstructWidget<USizeBox>();
    Box->SetWidthOverride(Size);
    Box->SetHeightOverride(Size);
    auto* Image = Tree.ConstructWidget<UImage>();
    Box->SetContent(Image);
    auto* Slot = Row.AddChildToHorizontalBox(Box);
    Slot->SetVerticalAlignment(VAlign_Center);
    return Image;
}

// Lay out a button row with a spacer between the left and right actions.
void AddActions(UWidgetTree& Tree, UVerticalBox& Column, UButton* Back, UButton* Forward)
{
    auto* Row = Tree.ConstructWidget<UHorizontalBox>();
    Row->AddChildToHorizontalBox(Back);
    if (Forward)
    {
        auto* Space = Tree.ConstructWidget<USizeBox>();
        Row->AddChildToHorizontalBox(Space)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Row->AddChildToHorizontalBox(Forward);
    }
    Column.AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
}

// Instantiate the fixed cards once. Refresh only changes their contents and selected styling.
void BuildCard(UWidgetTree& Tree, UHorizontalBox& Row, FGirlSaviorCampCardWidgets& Card, int32 Index)
{
    const FName ButtonName(*FString::Printf(TEXT("CampAbilityButton%d"), Index));
    Card.Button = Tree.ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
    StyleButton(*Card.Button, Field);
    auto* CardSlot = Row.AddChildToHorizontalBox(Card.Button);
    CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CardSlot->SetPadding(FMargin(Index == 0 ? 0.f : 8.f, 0.f, 0.f, 0.f));
    auto* Inset = Border(Tree, FMargin(12.f), FLinearColor::Transparent);
    auto* ButtonSlot = CastChecked<UButtonSlot>(Card.Button->SetContent(Inset));
    ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
    ButtonSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Column = Tree.ConstructWidget<UVerticalBox>();
    Inset->SetContent(Column);

    auto* Heading = Tree.ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Heading);
    Card.Icon = AddImage(Tree, *Heading, 36.f);
    Card.Heroine = Text(Tree, FText::GetEmpty(), 14, Muted, true);
    auto* HeroineSlot = Heading->AddChildToHorizontalBox(Card.Heroine);
    HeroineSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    HeroineSlot->SetVerticalAlignment(VAlign_Center);
    Card.Title = Text(Tree, FText::GetEmpty(), 17, Ink, true, 224.f);
    Column->AddChildToVerticalBox(Card.Title)->SetPadding(FMargin(0.f, 9.f, 0.f, 0.f));
    Card.Details = Text(Tree, FText::GetEmpty(), 13, Accent, true, 224.f);
    Column->AddChildToVerticalBox(Card.Details)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
    Card.Description = Text(Tree, FText::GetEmpty(), 12, Ink, false, 224.f);
    Column->AddChildToVerticalBox(Card.Description)->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
    Column->AddChildToVerticalBox(Tree.ConstructWidget<USizeBox>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Card.Action = Text(Tree, FText::GetEmpty(), 13, Muted, true);
    Column->AddChildToVerticalBox(Card.Action)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
}

// Render a card directly from its owner and skill definitions.
void RenderCard(FGirlSaviorCampCardWidgets& Card, const FGirlSaviorCampCardView* Offer)
{
    Card.Button->SetVisibility(Offer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Card.AbilityId = Offer ? Offer->AbilityId : NAME_None;
    if (!Offer)
    {
        return;
    }
    Card.Heroine->SetText(Offer->HeroineName);
    Card.Title->SetText(Offer->Title);
    Card.Details->SetText(Offer->Details);
    Card.Description->SetText(Offer->Description);
    Card.Icon->SetBrushFromTexture(Offer->Icon);
    Card.Action->SetText(Offer->bSelected ? LOCTEXT("RemoveCard", "Убрать") : LOCTEXT("ChooseCard", "Выбрать"));
    Card.Action->SetColorAndOpacity(Offer->bSelected ? Accent : Muted);
    StyleButton(*Card.Button, Offer->bSelected ? Selected : Field);
}
}

// Position the non-interactive prompt near the lower edge without covering the hero.
TSharedRef<SWidget> UGirlSaviorCampPromptWidget::RebuildWidget()
{
    using namespace GirlSavior::CampUI;
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Root;
        auto* Plate = Border(*WidgetTree, FMargin(16.f, 8.f), Panel);
        PromptText = Text(*WidgetTree, CurrentPrompt, 15, Accent, true, 0.f, TEXT("CampPromptText"));
        Plate->SetContent(PromptText);
        auto* PromptSlot = Root->AddChildToCanvas(Plate);
        PromptSlot->SetAnchors(FAnchors(0.5f, 0.89f));
        PromptSlot->SetAlignment(FVector2D(0.5, 0.5));
        PromptSlot->SetAutoSize(true);
        SetVisibility(CurrentPrompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    return Super::RebuildWidget();
}

// Update the prompt text and ensure it never intercepts gameplay clicks.
void UGirlSaviorCampPromptWidget::SetPrompt(const FText& Prompt)
{
    CurrentPrompt = Prompt;
    if (PromptText)
    {
        PromptText->SetText(CurrentPrompt);
    }
    SetVisibility(CurrentPrompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    OnPromptChanged(CurrentPrompt);
}

// Handle Esc before a focused child button can consume it.
FReply UGirlSaviorCampModalWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Escape)
    {
        if (!KeyEvent.IsRepeat())
        {
            RequestBack();
        }
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

// Use the same back operation for a button or a key.
void UGirlSaviorCampModalWidget::RequestBack()
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->RequestBack();
    }
}

// Build both table pages and bind their commands once for this widget instance.
TSharedRef<SWidget> UGirlSaviorCampPreparationWidget::RebuildWidget()
{
    using namespace GirlSavior::CampUI;
    SetIsFocusable(true);
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }

    UWidgetTree& Tree = *WidgetTree;
    PageSwitcher = Tree.ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("CampPageSwitcher"));
    MountModal(Tree, PageSwitcher);

    UVerticalBox* MissionColumn = nullptr;
    PageSwitcher->AddChild(MakePage(Tree, 450.f, MissionColumn));
    MissionColumn->AddChildToVerticalBox(Text(Tree, LOCTEXT("TableTitle", "Стол экспедиций"), 29, Ink, true));
    auto* MissionPlate = Border(Tree, FMargin(15.f, 12.f), Field);
    MissionColumn->AddChildToVerticalBox(MissionPlate)->SetPadding(FMargin(0.f, 13.f, 0.f, 0.f));
    auto* MissionBody = Tree.ConstructWidget<UVerticalBox>();
    MissionPlate->SetContent(MissionBody);
    MissionName = Text(Tree, FText::GetEmpty(), 19, Accent, true, 390.f, TEXT("CampMissionName"));
    MissionDescription = Text(Tree, FText::GetEmpty(), 14, Ink, false, 390.f);
    MissionBody->AddChildToVerticalBox(MissionName);
    MissionBody->AddChildToVerticalBox(MissionDescription)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
    MissionStatus = Text(Tree, FText::GetEmpty(), 13, Accent, false, 430.f, TEXT("CampMissionStatus"));
    MissionColumn->AddChildToVerticalBox(MissionStatus)->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
    MissionStatus->SetVisibility(ESlateVisibility::Collapsed);
    ConfigureButton = ActionButton(Tree, TEXT("MissionConfigureButton"), LOCTEXT("Configure", "Настроить вылазку"), true);
    auto* MissionBack = ActionButton(Tree, TEXT("MissionBackButton"), LOCTEXT("Back", "Назад"));
    ConfigureButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestPreparationPage);
    MissionBack->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestBack);
    AddActions(Tree, *MissionColumn, MissionBack, ConfigureButton);

    UVerticalBox* PreparationColumn = nullptr;
    PageSwitcher->AddChild(MakePage(Tree, 810.f, PreparationColumn));
    PreparationColumn->AddChildToVerticalBox(Text(Tree, LOCTEXT("PreparationTitle", "Подготовка вылазки"), 29, Ink, true));
    auto* PistolPlate = Border(Tree, FMargin(12.f, 7.f), Field);
    PistolPlate->SetContent(Text(Tree, LOCTEXT("FixedPistol", "Оружие · слот 1 · Пистолет"), 14, Ink, true));
    PreparationColumn->AddChildToVerticalBox(PistolPlate)->SetPadding(FMargin(0.f, 11.f, 0.f, 0.f));

    auto* CardsRow = Tree.ConstructWidget<UHorizontalBox>();
    PreparationColumn->AddChildToVerticalBox(CardsRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
    CardWidgets.SetNum(3);
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        BuildCard(Tree, *CardsRow, CardWidgets[Index], Index);
    }
    CardWidgets[0].Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFirstCard);
    CardWidgets[1].Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondCard);
    CardWidgets[2].Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleThirdCard);

    auto* SlotsRow = Tree.ConstructWidget<UHorizontalBox>();
    PreparationColumn->AddChildToVerticalBox(SlotsRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
    SelectionSlots.SetNum(2);
    for (int32 Index = 0; Index < SelectionSlots.Num(); ++Index)
    {
        auto* SlotPlate = Border(Tree, FMargin(12.f, 7.f), Field);
        const FName SlotName(*FString::Printf(TEXT("CampSlot%d"), Index));
        SelectionSlots[Index] = Text(Tree, FText::GetEmpty(), 13, Ink, true, 248.f, SlotName);
        SlotPlate->SetContent(SelectionSlots[Index]);
        auto* SelectionPlateSlot = SlotsRow->AddChildToHorizontalBox(SlotPlate);
        SelectionPlateSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
        SelectionPlateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    SelectionCount = Text(Tree, FText::GetEmpty(), 14, Accent, true, 0.f, TEXT("CampSelectionCount"));
    SlotsRow->AddChildToHorizontalBox(SelectionCount)->SetVerticalAlignment(VAlign_Center);
    SelectionStatus = Text(Tree, FText::GetEmpty(), 13, Accent, false, 790.f, TEXT("CampSelectionStatus"));
    auto* StatusBox = Tree.ConstructWidget<USizeBox>();
    StatusBox->SetMinDesiredHeight(17.f);
    StatusBox->SetContent(SelectionStatus);
    PreparationColumn->AddChildToVerticalBox(StatusBox)->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
    auto* PreparationBack = ActionButton(Tree, TEXT("PreparationBackButton"), LOCTEXT("BackToMissions", "К выбору вылазки"));
    LaunchButton = ActionButton(Tree, TEXT("LaunchButton"), LOCTEXT("Launch", "Отправиться"), true);
    PreparationBack->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestBack);
    LaunchButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestLaunch);
    LaunchButton->SetIsEnabled(false);
    AddActions(Tree, *PreparationColumn, PreparationBack, LaunchButton);
    return Super::RebuildWidget();
}

// Render ordered offers and selected slots from the controller's validated draft view.
void UGirlSaviorCampPreparationWidget::Refresh(const UGirlSaviorCampPrototypeDefinition& Catalog,
    TConstArrayView<FName> SelectedIds, bool bShowPreparation, bool bValidSelection, const FText& Status)
{
    using namespace GirlSavior::CampUI;
    CurrentMissionName = Catalog.MissionName;
    CurrentMissionDescription = Catalog.MissionDescription;
    CurrentSelection.Reset(SelectedIds.Num());
    CurrentSelection.Append(SelectedIds.GetData(), SelectedIds.Num());
    CurrentOffers.Reset(Catalog.AvailableAbilityIds.Num());
    for (const FName AbilityId : Catalog.AvailableAbilityIds)
    {
        const FGirlSaviorSupportOffer Offer = Catalog.FindOffer(AbilityId);
        if (Offer.Ability && Offer.Heroine)
        {
            CurrentOffers.Add({AbilityId, Offer.Heroine->DisplayName, Offer.Ability->DisplayName,
                Offer.Ability->BuildDetailsText(), Offer.Ability->Description, Offer.Ability->Icon, SelectedIds.Contains(AbilityId)});
        }
    }
    if (!PageSwitcher)
    {
        OnPreparationRefreshed(bShowPreparation, bValidSelection, Status);
        return;
    }

    PageSwitcher->SetActiveWidgetIndex(bShowPreparation ? 1 : 0);
    MissionName->SetText(CurrentMissionName);
    MissionDescription->SetText(CurrentMissionDescription);
    const bool bMissionError = !bShowPreparation && !Status.IsEmpty();
    MissionStatus->SetText(Status);
    MissionStatus->SetVisibility(bMissionError ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ConfigureButton->SetIsEnabled(!bMissionError);
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        RenderCard(CardWidgets[Index], CurrentOffers.IsValidIndex(Index) ? &CurrentOffers[Index] : nullptr);
    }

    for (int32 Index = 0; Index < SelectionSlots.Num(); ++Index)
    {
        const auto Offer = SelectedIds.IsValidIndex(Index) ? Catalog.FindOffer(SelectedIds[Index]) : FGirlSaviorSupportOffer{};
        const FText SlotName = Offer.Ability ? Offer.Ability->DisplayName : LOCTEXT("EmptySlot", "Пусто");
        SelectionSlots[Index]->SetText(FText::Format(LOCTEXT("SlotText", "Слот {0} · {1}"), FText::AsNumber(Index + 1), SlotName));
    }
    SelectionCount->SetText(FText::Format(LOCTEXT("SelectedCount", "Выбрано {0} из 2"), FText::AsNumber(SelectedIds.Num())));
    SelectionStatus->SetText(Status);
    LaunchButton->SetIsEnabled(bValidSelection);
    OnPreparationRefreshed(bShowPreparation, bValidSelection, Status);
}

// A recoverable request can be retried with the keyboard without navigating again.
void UGirlSaviorCampPreparationWidget::FocusAfterLaunchFailure()
{
    if (auto* Controller = GetOwningPlayer())
    {
        if (LaunchButton && LaunchButton->GetIsEnabled())
        {
            LaunchButton->SetUserFocus(Controller);
        }
        else if (!HasFocusedDescendants())
        {
            SetUserFocus(Controller);
        }
    }
}

// Request a page transition without mutating the draft from the widget.
void UGirlSaviorCampPreparationWidget::RequestPreparationPage()
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->OpenPreparationPage();
    }
}

// Pass the stable ability ID back to the controller for admission.
void UGirlSaviorCampPreparationWidget::RequestAbility(FName AbilityId)
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->ToggleAbility(AbilityId);
    }
}

// Let the controller run its final validation and report the result.
void UGirlSaviorCampPreparationWidget::RequestLaunch()
{
    if (auto* Controller = GetOwningPlayer<AGirlSaviorCampPlayerController>())
    {
        Controller->RequestLaunch();
    }
}

// A card's fixed position is mapped to the current authored offer at click time.
void UGirlSaviorCampPreparationWidget::RequestCard(int32 Index)
{
    if (CardWidgets.IsValidIndex(Index) && !CardWidgets[Index].AbilityId.IsNone())
    {
        RequestAbility(CardWidgets[Index].AbilityId);
    }
}

// Wiring the individual buttons once avoids rebinding on every refresh.
void UGirlSaviorCampPreparationWidget::HandleFirstCard()
{
    RequestCard(0);
}

void UGirlSaviorCampPreparationWidget::HandleSecondCard()
{
    RequestCard(1);
}

void UGirlSaviorCampPreparationWidget::HandleThirdCard()
{
    RequestCard(2);
}

// The profile keeps the same focus and back behavior as the table without any loadout actions.
TSharedRef<SWidget> UGirlSaviorCampHeroineInfoWidget::RebuildWidget()
{
    using namespace GirlSavior::CampUI;
    SetIsFocusable(true);
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }

    UWidgetTree& Tree = *WidgetTree;
    UVerticalBox* Column = nullptr;
    MountModal(Tree, MakePage(Tree, 455.f, Column));
    auto* Heading = Tree.ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Heading);
    Portrait = AddImage(Tree, *Heading, 54.f);
    auto* Headline = Tree.ConstructWidget<UVerticalBox>();
    auto* HeadingSlot = Heading->AddChildToHorizontalBox(Headline);
    HeadingSlot->SetVerticalAlignment(VAlign_Center);
    Name = Text(Tree, FText::GetEmpty(), 27, Ink, true, 0.f, TEXT("CampHeroineName"));
    Role = Text(Tree, FText::GetEmpty(), 14, Accent, true);
    Headline->AddChildToVerticalBox(Name);
    Headline->AddChildToVerticalBox(Role)->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
    Description = Text(Tree, FText::GetEmpty(), 16, Ink, false, 450.f);
    Column->AddChildToVerticalBox(Description)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    auto* SkillPlate = Border(Tree, FMargin(12.f, 9.f), Field);
    Skill = Text(Tree, FText::GetEmpty(), 14, Accent, true, 420.f);
    SkillPlate->SetContent(Skill);
    Column->AddChildToVerticalBox(SkillPlate)->SetPadding(FMargin(0.f, 13.f, 0.f, 0.f));
    auto* Back = ActionButton(Tree, TEXT("HeroineBackButton"), LOCTEXT("Back", "Назад"));
    Back->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestBack);
    AddActions(Tree, *Column, Back, nullptr);
    return Super::RebuildWidget();
}

// Show the authored portrait when provided and use the active skill icon for placeholder profiles.
void UGirlSaviorCampHeroineInfoWidget::SetHeroine(const UGirlSaviorHeroineDefinition& Heroine,
    const UGirlSaviorSupportAbilityDefinition* Ability)
{
    if (!Name)
    {
        OnHeroineShown(&Heroine, Ability);
        return;
    }
    Name->SetText(Heroine.DisplayName);
    Role->SetText(Heroine.SupportRole);
    Description->SetText(Heroine.Description);
    UTexture2D* Icon = Heroine.Portrait ? Heroine.Portrait.Get() : (Ability ? Ability->Icon.Get() : nullptr);
    Portrait->SetBrushFromTexture(Icon);
    Portrait->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    Skill->SetText(Ability
        ? FText::Format(LOCTEXT("HeroineSkill", "{0}\n{1}"), Ability->DisplayName, Ability->BuildDetailsText())
        : LOCTEXT("NoHeroineSkill", "Нет доступного умения"));
    OnHeroineShown(&Heroine, Ability);
}

#undef LOCTEXT_NAMESPACE
