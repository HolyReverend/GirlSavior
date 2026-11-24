#include "Encounters/GirlSaviorCombatWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCombatWidgets"

namespace
{
const FLinearColor PanelColor(0.026f, 0.034f, 0.043f, 0.95f);
const FLinearColor TextColor(0.89f, 0.92f, 0.94f);
const FLinearColor MutedColor(0.55f, 0.61f, 0.65f);
const FLinearColor WarmColor(0.95f, 0.59f, 0.30f);

UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, int32 Size,
    const FLinearColor& Color = TextColor, bool bBold = false)
{
    auto* Widget = Tree.ConstructWidget<UTextBlock>();
    Widget->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
    Widget->SetColorAndOpacity(Color);
    Widget->SetText(Text);
    return Widget;
}

UBorder* MakePanel(UWidgetTree& Tree, const FMargin& Padding, const FLinearColor& Color = PanelColor)
{
    auto* Widget = Tree.ConstructWidget<UBorder>();
    Widget->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
    Widget->SetBrushColor(Color);
    Widget->SetPadding(Padding);
    return Widget;
}

// Meter values and their fill come from the same snapshot.
void AddMeter(UWidgetTree& Tree, UVerticalBox& Column, const FText& Label, const FLinearColor& Color,
    TObjectPtr<UTextBlock>& Value, TObjectPtr<UProgressBar>& Bar)
{
    auto* Row = Tree.ConstructWidget<UHorizontalBox>();
    auto* LabelSlot = Row->AddChildToHorizontalBox(MakeText(Tree, Label, 13, MutedColor, true));
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Value = MakeText(Tree, FText::GetEmpty(), 15, TextColor, true);
    Row->AddChildToHorizontalBox(Value.Get());
    Column.AddChildToVerticalBox(Row);

    auto* Track = Tree.ConstructWidget<USizeBox>();
    Track->SetHeightOverride(5.f);
    Bar = Tree.ConstructWidget<UProgressBar>();
    FProgressBarStyle Style;
    Style.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.09f, 0.11f, 0.14f)));
    Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
    Bar->SetWidgetStyle(Style);
    Bar->SetFillColorAndOpacity(Color);
    Track->AddChild(Bar.Get());
    Column.AddChildToVerticalBox(Track)->SetPadding(FMargin(0, 5, 0, 0));
}

void UpdateMeter(UTextBlock& Text, UProgressBar& Bar, double Value, double Maximum)
{
    FNumberFormattingOptions Format;
    Format.SetMaximumFractionalDigits(2);
    Text.SetText(FText::Format(LOCTEXT("MeterValue", "{0} / {1}"),
        FText::AsNumber(Value, &Format), FText::AsNumber(Maximum, &Format)));
    Bar.SetPercent(Maximum > 0.0 ? static_cast<float>(FMath::Clamp(Value / Maximum, 0.0, 1.0)) : 0.f);
}

UCanvasPanelSlot* PlacePanel(UCanvasPanel& Canvas, UWidget* Panel, FVector2D Anchor, FVector2D Alignment)
{
    auto* Slot = Canvas.AddChildToCanvas(Panel);
    Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    Slot->SetAlignment(Alignment);
    Slot->SetAutoSize(true);
    return Slot;
}
}

TSharedRef<SWidget> UGirlSaviorCombatVitalsWidget::RebuildWidget()
{
    if (WidgetTree && !HealthText)
    {
        auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Root;
        auto* Panel = MakePanel(*WidgetTree, FMargin(14, 11));
        PlacePanel(*Root, Panel, FVector2D::ZeroVector, FVector2D::ZeroVector)->SetPosition(FVector2D(18, 16));
        auto* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetWidthOverride(214.f);
        Panel->AddChild(Width);
        auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        Width->AddChild(Column);

        AddMeter(*WidgetTree, *Column, LOCTEXT("Health", "Здоровье"), HealthColor, HealthText, HealthBar);
        BarrierGroup = WidgetTree->ConstructWidget<UVerticalBox>();
        Column->AddChildToVerticalBox(BarrierGroup.Get())->SetPadding(FMargin(0, 9, 0, 0));
        AddMeter(*WidgetTree, *BarrierGroup, LOCTEXT("Barrier", "Барьер"), BarrierColor, BarrierText, BarrierBar);
        DashText = MakeText(*WidgetTree, DashStatus, 12, DashColor);
        Column->AddChildToVerticalBox(DashText.Get())->SetPadding(FMargin(0, 10, 0, 0));
        RefreshVitals();
    }
    return Super::RebuildWidget();
}

void UGirlSaviorCombatVitalsWidget::SetVitals(double Health, double HealthLimit, double Barrier, double InitialBarrier)
{
    DisplayedHealth = Health;
    DisplayedMaxHealth = HealthLimit;
    DisplayedBarrier = Barrier;
    DisplayedMaxBarrier = InitialBarrier;
    RefreshVitals();
}

void UGirlSaviorCombatVitalsWidget::RefreshVitals()
{
    if (HealthText)
    {
        UpdateMeter(*HealthText, *HealthBar, DisplayedHealth, DisplayedMaxHealth);
        UpdateMeter(*BarrierText, *BarrierBar, DisplayedBarrier, DisplayedMaxBarrier);
        BarrierGroup->SetVisibility(IsBarrierVisible() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UGirlSaviorCombatVitalsWidget::SetDash(const FText& Status, const FLinearColor& Color)
{
    DashStatus = Status;
    DashColor = Color;
    if (DashText)
    {
        DashText->SetText(Status);
        DashText->SetColorAndOpacity(Color);
    }
}

FText UGirlSaviorCombatVitalsWidget::GetDisplayedDashText() const
{
    return DashStatus;
}

TSharedRef<SWidget> UGirlSaviorCombatOutcomeWidget::RebuildWidget()
{
    if (WidgetTree && !DeathNotice)
    {
        auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Root;

        // The early, noninteractive notice leaves the fall and the center of the playfield visible.
        DeathNotice = MakePanel(*WidgetTree, FMargin(22, 13));
        PlacePanel(*Root, DeathNotice.Get(), FVector2D(0.5, 0.18), FVector2D(0.5, 0.0));
        auto* NoticeColumn = WidgetTree->ConstructWidget<UVerticalBox>();
        DeathNotice->AddChild(NoticeColumn);
        auto* NoticeTitle = MakeText(*WidgetTree, LOCTEXT("Defeat", "Герой погиб"), 24, TextColor, true);
        NoticeTitle->SetJustification(ETextJustify::Center);
        NoticeColumn->AddChildToVerticalBox(NoticeTitle);
        NoticeCause = MakeText(*WidgetTree, FText::GetEmpty(), 14, WarmColor);
        NoticeCause->SetJustification(ETextJustify::Center);
        NoticeColumn->AddChildToVerticalBox(NoticeCause.Get())->SetPadding(FMargin(0, 4, 0, 0));

        ResultBackdrop = MakePanel(*WidgetTree, FMargin(), FLinearColor(0, 0, 0, 0.54f));
        auto* BackdropSlot = Root->AddChildToCanvas(ResultBackdrop.Get());
        BackdropSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        BackdropSlot->SetOffsets(FMargin());

        ResultPanel = MakePanel(*WidgetTree, FMargin(26, 22));
        PlacePanel(*Root, ResultPanel.Get(), FVector2D(0.5, 0.5), FVector2D(0.5, 0.5));
        auto* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetWidthOverride(308.f);
        ResultPanel->AddChild(Width);
        auto* ResultColumn = WidgetTree->ConstructWidget<UVerticalBox>();
        Width->AddChild(ResultColumn);

        ResultTitle = MakeText(*WidgetTree, FText::GetEmpty(), 26, TextColor, true);
        ResultColumn->AddChildToVerticalBox(ResultTitle.Get());
        ResultCause = MakeText(*WidgetTree, FText::GetEmpty(), 15, WarmColor);
        ResultCause->SetAutoWrapText(true);
        ResultColumn->AddChildToVerticalBox(ResultCause.Get())->SetPadding(FMargin(0, 6, 0, 0));
        ResultActionStatus = MakeText(*WidgetTree, FText::GetEmpty(), 14, WarmColor);
        ResultActionStatus->SetAutoWrapText(true);
        ResultColumn->AddChildToVerticalBox(ResultActionStatus.Get())->SetPadding(FMargin(0, 6, 0, 0));

        RetryButton = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle ButtonStyle;
        ButtonStyle.SetNormal(FSlateColorBrush(FLinearColor(0.80f, 0.48f, 0.22f)));
        ButtonStyle.SetHovered(FSlateColorBrush(FLinearColor(0.95f, 0.63f, 0.30f)));
        ButtonStyle.SetPressed(FSlateColorBrush(FLinearColor(0.69f, 0.37f, 0.16f)));
        RetryButton->SetStyle(ButtonStyle);
        auto* ButtonLabel = MakePanel(*WidgetTree, FMargin(14, 6), FLinearColor::Transparent);
        ResultActionLabel = MakeText(*WidgetTree, FText::GetEmpty(), 15, PanelColor, true);
        ButtonLabel->AddChild(ResultActionLabel.Get());
        RetryButton->AddChild(ButtonLabel);
        RetryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
        auto* RetrySlot = ResultColumn->AddChildToVerticalBox(RetryButton.Get());
        RetrySlot->SetPadding(FMargin(0, 17, 0, 0));
        RetrySlot->SetHorizontalAlignment(HAlign_Left);
        ClearResult();
    }
    return Super::RebuildWidget();
}

void UGirlSaviorCombatOutcomeWidget::ClearResult()
{
    if (DeathNotice)
    {
        DeathNotice->SetVisibility(ESlateVisibility::Collapsed);
        ResultBackdrop->SetVisibility(ESlateVisibility::Collapsed);
        ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
        ResultActionStatus->SetVisibility(ESlateVisibility::Collapsed);
        RetryButton->SetIsEnabled(true);
    }
}

void UGirlSaviorCombatOutcomeWidget::ShowDeathNotice(const FText& Cause)
{
    ClearResult();
    NoticeCause->SetText(Cause);
    DeathNotice->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// The same panel presents victories, defeats and interrupted Camp launches.
void UGirlSaviorCombatOutcomeWidget::PresentResult(const FText& Title, const FText& Detail, const FText& Action)
{
    ClearResult();
    ResultTitle->SetText(Title);
    ResultCause->SetText(Detail);
    ResultCause->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    ResultActionLabel->SetText(Action);
    RetryButton->SetVisibility(Action.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ResultBackdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
    ResultPanel->SetVisibility(ESlateVisibility::Visible);
}

void UGirlSaviorCombatOutcomeWidget::ShowResult(bool bDefeated, const FText& Cause, const FText& Action)
{
    PresentResult(bDefeated ? LOCTEXT("Defeat", "Герой погиб") : LOCTEXT("Success", "Выход достигнут"), Cause, Action);
}

void UGirlSaviorCombatOutcomeWidget::ShowRecovery(const FText& Message, const FText& Action)
{
    PresentResult(LOCTEXT("RecoveryTitle", "Вылазка прервана"), Message, Action);
}

// Keep the result and its cause visible when an action can be retried from the origin.
void UGirlSaviorCombatOutcomeWidget::SetActionFailure(const FText& Message)
{
    ResultActionStatus->SetText(Message);
    ResultActionStatus->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    RetryButton->SetIsEnabled(true);
}

void UGirlSaviorCombatOutcomeWidget::HandleRetryClicked()
{
    if (IsOutcomePanelVisible() && RetryButton->GetVisibility() == ESlateVisibility::Visible && RetryButton->GetIsEnabled())
    {
        RetryButton->SetIsEnabled(false);
        OnRetryRequested.Broadcast();
    }
}

bool UGirlSaviorCombatOutcomeWidget::IsDeathNoticeVisible() const
{
    return DeathNotice && DeathNotice->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGirlSaviorCombatOutcomeWidget::IsOutcomePanelVisible() const
{
    return ResultPanel && ResultPanel->GetVisibility() == ESlateVisibility::Visible;
}

FText UGirlSaviorCombatOutcomeWidget::GetDisplayedTitle() const
{
    if (IsDeathNoticeVisible())
    {
        return LOCTEXT("Defeat", "Герой погиб");
    }
    return IsOutcomePanelVisible() ? ResultTitle->GetText() : FText::GetEmpty();
}

FText UGirlSaviorCombatOutcomeWidget::GetDisplayedCause() const
{
    if (IsDeathNoticeVisible())
    {
        return NoticeCause->GetText();
    }
    return IsOutcomePanelVisible() ? ResultCause->GetText() : FText::GetEmpty();
}

FText UGirlSaviorCombatOutcomeWidget::GetDisplayedActionLabel() const
{
    return IsOutcomePanelVisible() ? ResultActionLabel->GetText() : FText::GetEmpty();
}

FText UGirlSaviorCombatOutcomeWidget::GetActionStatus() const
{
    return IsOutcomePanelVisible() && ResultActionStatus->GetVisibility() != ESlateVisibility::Collapsed
        ? ResultActionStatus->GetText() : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
