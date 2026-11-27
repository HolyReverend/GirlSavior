#include "Camp/GirlSaviorCampPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camp/GirlSaviorCampInteractable.h"
#include "Camp/GirlSaviorCampDefinition.h"
#include "Camp/GirlSaviorCampWidgets.h"
#include "Camp/GirlSaviorExpeditionTable.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Expeditions/GirlSaviorExpeditionSessionSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "InputAction.h"
#include "SceneView.h"
#include "Support/GirlSaviorHeroineDefinition.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "TimerManager.h"
#include "Weapons/GirlSaviorWeaponLoadoutWidget.h"
#include "Weapons/GirlSaviorWeaponProfileSubsystem.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCampPlayerController"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorCamp, Log, All);

namespace
{
constexpr FName FGirlSaviorWeaponLoadout::* WeaponSlots[] = {
    &FGirlSaviorWeaponLoadout::Slot1, &FGirlSaviorWeaponLoadout::Slot2, &FGirlSaviorWeaponLoadout::Slot3
};
static_assert(UE_ARRAY_COUNT(WeaponSlots) == FGirlSaviorWeaponLoadout::SlotCount);

// Preserve empty positions and keep player-facing names in the catalog.
TArray<FText> BuildWeaponSlotLabels(const UDataTable* Table, const FGirlSaviorWeaponLoadout* Loadout)
{
    TArray<FText> Labels;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeaponSlots); ++Index)
    {
        const FName Id = Loadout ? Loadout->*WeaponSlots[Index] : NAME_None;
        const auto* Row = Table && !Id.IsNone()
            ? Table->FindRow<FGirlSaviorWeaponRow>(Id, TEXT("Camp weapon slot preview"), false) : nullptr;
        const FText Name = Row ? Row->DisplayName : LOCTEXT("EmptyWeaponSlot", "Пусто");
        Labels.Add(FText::Format(LOCTEXT("WeaponSlotLabel", "Слот {0} · {1}"), FText::AsNumber(Index + 1), Name));
    }
    return Labels;
}
}

// Use an input action edge so holding E cannot reopen a panel.
void AGirlSaviorCampPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    auto* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (EnhancedInput && InteractAction && InteractAction->ValueType == EInputActionValueType::Boolean)
    {
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::OnInteractPressed);
    }
    else
    {
        UE_LOG(LogGirlSaviorCamp, Error, TEXT("%s requires Enhanced Input and a Boolean InteractAction."), *GetName());
    }
}

// Preserve the shared pawn, movement and camera while installing Camp-only restrictions and polling.
void AGirlSaviorCampPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    SetFireAllowed(false);
    SetGameplayInputAvailable(true);

    auto* CampPawn = Cast<ACharacter>(GetPawn());
    if (!IsLocalController() || !CampPawn)
    {
        return;
    }
    if (auto* Session = GetExpeditionSession())
    {
        Session->NotifyCampReady(*GetWorld());
    }
    if (!ValidateInteractionIds())
    {
        return;
    }

    UCapsuleComponent* Capsule = CampPawn->GetCapsuleComponent();
    Capsule->SetGenerateOverlapEvents(true);
    Capsule->UpdateOverlaps();

    if (PromptWidgetClass)
    {
        PromptWidget = CreateWidget<UGirlSaviorCampPromptWidget>(this, PromptWidgetClass);
        if (PromptWidget && PromptWidget->AddToPlayerScreen())
        {
            PromptWidget->SetPrompt(FText::GetEmpty());
            GetWorldTimerManager().SetTimer(InteractionPoll, this, &ThisClass::PollInteraction, 0.1f, true);
            return;
        }
    }
    UE_LOG(LogGirlSaviorCamp, Error, TEXT("%s could not create the Camp prompt."), *GetName());
    PromptWidget = nullptr;
}

// A fresh possession must not inherit widgets, overlaps or a draft from an earlier pawn.
void AGirlSaviorCampPlayerController::OnUnPossess()
{
    ResetCamp();
    Super::OnUnPossess();
}

// Avoid changing viewport focus while the play world is being destroyed.
void AGirlSaviorCampPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetCamp();
    Super::EndPlay(EndPlayReason);
}

// Stable, unique IDs make the same set of candidates resolve in a predictable order.
bool AGirlSaviorCampPlayerController::ValidateInteractionIds()
{
    ExpeditionTable.Reset();
    TSet<FName> Ids;
    for (TActorIterator<AGirlSaviorCampInteractable> It(GetWorld()); It; ++It)
    {
        const FName Id = It->GetInteractionId();
        if (Id.IsNone() || Ids.Contains(Id))
        {
            UE_LOG(LogGirlSaviorCamp, Error, TEXT("%s has an empty or repeated Camp InteractionId: %s."),
                *It->GetPathName(), *Id.ToString());
            return false;
        }
        Ids.Add(Id);

        if (auto* Table = Cast<AGirlSaviorExpeditionTable>(*It))
        {
            if (ExpeditionTable.IsValid())
            {
                UE_LOG(LogGirlSaviorCamp, Error, TEXT("Camp requires one expedition table."));
                return false;
            }
            ExpeditionTable = Table;
        }
    }
    if (!ExpeditionTable.IsValid())
    {
        UE_LOG(LogGirlSaviorCamp, Error, TEXT("Camp is missing its expedition table."));
        return false;
    }
    return true;
}

// Start with overlap candidates, then use exact range, constrained projection and chest-height visibility.
AGirlSaviorCampInteractable* AGirlSaviorCampPlayerController::PickInteractionTarget() const
{
    const auto* CampPawn = Cast<ACharacter>(GetPawn());
    const ULocalPlayer* LocalPlayer = GetLocalPlayer();
    FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
    FSceneViewProjectionData Projection;
    if (!CampPawn || !Viewport || !LocalPlayer->GetProjectionData(Viewport, Projection))
    {
        return nullptr;
    }

    const FIntRect& Rect = Projection.GetConstrainedViewRect();
    if (Rect.IsEmpty())
    {
        return nullptr;
    }
    const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
    const FVector Origin = CampPawn->GetActorLocation();
    const FVector Chest = Origin + FVector(0.f, 0.f, CampPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.45f);
    TArray<AActor*> Overlaps;
    CampPawn->GetOverlappingActors(Overlaps, AGirlSaviorCampInteractable::StaticClass());

    AGirlSaviorCampInteractable* Closest = nullptr;
    double ClosestDistanceSquared = FMath::Square(AGirlSaviorCampInteractable::InteractionRange);
    for (AActor* Actor : Overlaps)
    {
        auto* Target = Cast<AGirlSaviorCampInteractable>(Actor);
        if (!IsValid(Target) || !Target->IsInteractionEnabled())
        {
            continue;
        }

        const FVector Anchor = Target->GetInteractionLocation();
        const double DistanceSquared = FVector::DistSquared(Origin, Anchor);
        if (DistanceSquared > ClosestDistanceSquared
            || (Closest && DistanceSquared == ClosestDistanceSquared
                && !Target->GetInteractionId().LexicalLess(Closest->GetInteractionId())))
        {
            continue;
        }

        FVector2D Pixel;
        if (!FSceneView::ProjectWorldToScreen(Anchor, Rect, ViewProjection, Pixel)
            || Pixel.X < Rect.Min.X || Pixel.X >= Rect.Max.X || Pixel.Y < Rect.Min.Y || Pixel.Y >= Rect.Max.Y)
        {
            continue;
        }
        FCollisionQueryParams Trace(SCENE_QUERY_STAT(CampInteraction), false, CampPawn);
        Trace.AddIgnoredActor(Target);
        if (GetWorld()->LineTraceTestByChannel(Chest, Anchor, ECC_Visibility, Trace))
        {
            continue;
        }

        Closest = Target;
        ClosestDistanceSquared = DistanceSquared;
    }
    return Closest;
}

// The sole prompt follows the current candidate while walking.
void AGirlSaviorCampPlayerController::PollInteraction()
{
    SetDisplayedTarget(Page == EGirlSaviorCampPage::Walking && CanUseGameplayInput() ? PickInteractionTarget() : nullptr);
}

// Reuse the same widget and clear the weak target when it becomes unavailable.
void AGirlSaviorCampPlayerController::SetDisplayedTarget(AGirlSaviorCampInteractable* Target)
{
    if (Target ? DisplayedTarget.Get() == Target : DisplayedTarget.IsExplicitlyNull())
    {
        return;
    }

    DisplayedTarget = Target;
    if (PromptWidget)
    {
        PromptWidget->SetPrompt(Target ? Target->GetPrompt() : FText::GetEmpty());
    }
}

// A changed target refreshes the prompt immediately. The next press confirms the new target.
void AGirlSaviorCampPlayerController::OnInteractPressed()
{
    if (Page != EGirlSaviorCampPage::Walking || !PromptWidget || !CanUseGameplayInput())
    {
        return;
    }
    AGirlSaviorCampInteractable* Target = PickInteractionTarget();
    if (Target && DisplayedTarget == Target)
    {
        Target->Interact(*this);
        return;
    }
    SetDisplayedTarget(Target);
}

// Opening the table creates one draft for the visit, initially on the mission page.
void AGirlSaviorCampPlayerController::OpenExpeditionTable(const UGirlSaviorCampDefinition& Catalog)
{
    if (Page != EGirlSaviorCampPage::Walking || !CanUseGameplayInput() || !PreparationWidgetClass)
    {
        return;
    }
    auto* Widget = CreateWidget<UGirlSaviorCampPreparationWidget>(this, PreparationWidgetClass);
    if (PresentModal(Widget, EGirlSaviorCampPage::Missions))
    {
        TableWidget = Widget;
        ActiveCatalog = &Catalog;
        const auto* Session = GetExpeditionSession();
        Draft.Begin(Catalog, Session ? Session->GetLastConfirmedAbilityIds() : TConstArrayView<FName>());
        RefreshPreparation();
    }
}

// The profile shows the skill this heroine actually offers in the table's catalog.
void AGirlSaviorCampPlayerController::OpenHeroineInfo(const UGirlSaviorHeroineDefinition& Heroine)
{
    if (Page != EGirlSaviorCampPage::Walking || !CanUseGameplayInput() || !HeroineInfoWidgetClass)
    {
        return;
    }

    const UGirlSaviorSupportAbilityDefinition* OfferedAbility = nullptr;
    const AGirlSaviorExpeditionTable* Table = ExpeditionTable.Get();
    const UGirlSaviorCampDefinition* Catalog = Table ? Table->GetCatalog() : nullptr;
    if (Catalog)
    {
        for (const UGirlSaviorSupportAbilityDefinition* Ability : Heroine.Abilities)
        {
            if (IsValid(Ability) && Catalog->FindOffer(Ability->AbilityId).Heroine == &Heroine)
            {
                OfferedAbility = Ability;
                break;
            }
        }
    }

    auto* Widget = CreateWidget<UGirlSaviorCampHeroineInfoWidget>(this, HeroineInfoWidgetClass);
    if (PresentModal(Widget, EGirlSaviorCampPage::Heroine))
    {
        Widget->SetHeroine(Heroine, OfferedAbility);
    }
}

// Stop inherited movement immediately and give the unpaused modal exclusive input.
bool AGirlSaviorCampPlayerController::PresentModal(UUserWidget* Widget, EGirlSaviorCampPage NewPage)
{
    if (!Widget || !Widget->AddToPlayerScreen(10))
    {
        return false;
    }
    ModalWidget = Widget;
    Page = NewPage;
    SetDisplayedTarget(nullptr);
    SetGameplayInputAvailable(false);
    FlushPressedKeys();
    APawn* ControlledPawn = GetPawn();
    if (UPawnMovementComponent* Movement = ControlledPawn ? ControlledPawn->GetMovementComponent() : nullptr)
    {
        Movement->StopMovementImmediately();
    }

    bShowMouseCursor = true;
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetWidgetToFocus(Widget->TakeWidget());
    SetInputMode(InputMode);
    Widget->SetUserFocus(this);
    return true;
}

// Keep the parent widget and support draft alive while the weapon page owns focus.
void AGirlSaviorCampPlayerController::OpenWeaponLoadout()
{
    const auto* Catalog = ActiveCatalog.Get();
    auto* Profile = GetWeaponProfile();
    if ((Page != EGirlSaviorCampPage::Missions && Page != EGirlSaviorCampPage::Preparation)
        || !Catalog || !TableWidget || IsLaunchInProgress())
    {
        return;
    }
    FText Error;
    if (!Catalog->WeaponTable || !Profile || !Profile->EnsureLoaded(*Catalog->WeaponTable, Error))
    {
        RefreshPreparation(Error.IsEmpty() ? LOCTEXT("WeaponCatalogUnavailable", "Данные оружия недоступны.") : Error);
        return;
    }

    UClass* WidgetClass = WeaponLoadoutWidgetClass.Get();
    auto* Widget = CreateWidget<UGirlSaviorWeaponLoadoutWidget>(this,
        WidgetClass ? WidgetClass : UGirlSaviorWeaponLoadoutWidget::StaticClass());
    const EGirlSaviorCampPage ReturnPage = Page;
    if (PresentModal(Widget, EGirlSaviorCampPage::Weapons))
    {
        WeaponReturnPage = ReturnPage;
        TableWidget->SetVisibility(ESlateVisibility::Collapsed);
        WeaponDraft = Profile->GetConfirmedLoadout();
        SelectedWeapon = NAME_None;
        RefreshWeaponLoadout();
        Widget->FocusInitialAction();
    }
}

// Validate a card intent by stable ID before the player can assign it to a slot.
void AGirlSaviorCampPlayerController::SelectWeaponCard(FName WeaponId)
{
    const auto* Catalog = ActiveCatalog.Get();
    const auto* Profile = GetWeaponProfile();
    if (Page != EGirlSaviorCampPage::Weapons || !Catalog || !Catalog->WeaponTable || !Profile || IsLaunchInProgress())
    {
        return;
    }
    if (!Profile->GetUnlockedWeaponIds().Contains(WeaponId)
        || !Catalog->WeaponTable->FindRow<FGirlSaviorWeaponRow>(WeaponId, TEXT("Camp weapon selection"), false))
    {
        RefreshWeaponLoadout(LOCTEXT("WeaponUnavailable", "Это оружие недоступно."));
        return;
    }
    SelectedWeapon = WeaponId;
    RefreshWeaponLoadout();
}

// Move or swap an equipped weapon; a newly selected weapon replaces the destination.
void AGirlSaviorCampPlayerController::PlaceSelectedWeapon(int32 SlotIndex)
{
    if (Page != EGirlSaviorCampPage::Weapons || SlotIndex < 0
        || SlotIndex >= UE_ARRAY_COUNT(WeaponSlots) || SelectedWeapon.IsNone() || IsLaunchInProgress())
    {
        return;
    }
    FName& Destination = WeaponDraft.*WeaponSlots[SlotIndex];
    if (Destination == SelectedWeapon)
    {
        return;
    }
    for (const auto Slot : WeaponSlots)
    {
        FName& Occupant = WeaponDraft.*Slot;
        if (Occupant == SelectedWeapon)
        {
            Swap(Occupant, Destination);
            RefreshWeaponLoadout();
            return;
        }
    }
    Destination = SelectedWeapon;
    RefreshWeaponLoadout();
}

// Allow temporary empty drafts; only confirmation requires an occupied slot.
void AGirlSaviorCampPlayerController::RemoveWeapon(int32 SlotIndex)
{
    if (Page == EGirlSaviorCampPage::Weapons && SlotIndex >= 0
        && SlotIndex < UE_ARRAY_COUNT(WeaponSlots) && !IsLaunchInProgress())
    {
        WeaponDraft.*WeaponSlots[SlotIndex] = NAME_None;
        RefreshWeaponLoadout();
        if (auto* Widget = Cast<UGirlSaviorWeaponLoadoutWidget>(ModalWidget))
        {
            Widget->FocusInitialAction();
        }
    }
}

// Keep the draft open on validation or storage failure so confirmation can be retried.
void AGirlSaviorCampPlayerController::ConfirmWeapons()
{
    const auto* Catalog = ActiveCatalog.Get();
    auto* Profile = GetWeaponProfile();
    if (Page != EGirlSaviorCampPage::Weapons || !Catalog || !Catalog->WeaponTable || !Profile || IsLaunchInProgress())
    {
        return;
    }
    FText Error;
    if (Profile->TryConfirmLoadout(*Catalog->WeaponTable, WeaponDraft, Error))
    {
        CloseWeaponLoadout();
    }
    else
    {
        RefreshWeaponLoadout(Error.IsEmpty() ? LOCTEXT("WeaponConfirmFailed", "Не удалось сохранить набор оружия. Попробуйте ещё раз.") : Error);
    }
}

// Render stable catalog order, ownership, and the controller's current weapon draft.
void AGirlSaviorCampPlayerController::RefreshWeaponLoadout(const FText& Status)
{
    const auto* Catalog = ActiveCatalog.Get();
    const auto* Profile = GetWeaponProfile();
    auto* Widget = Cast<UGirlSaviorWeaponLoadoutWidget>(ModalWidget);
    if (!Catalog || !Catalog->WeaponTable || !Profile || !Widget)
    {
        return;
    }
    const UDataTable& Table = *Catalog->WeaponTable;
    const TConstArrayView<FName> Unlocked = Profile->GetUnlockedWeaponIds();
    TArray<FGirlSaviorWeaponCardView> Cards;
    for (const FName Id : Table.GetRowNames())
    {
        if (const auto* Row = Table.FindRow<FGirlSaviorWeaponRow>(Id, TEXT("Camp weapon cards")))
        {
            const bool bOwned = Unlocked.Contains(Id);
            Cards.Add({Id, Row->DisplayName, Row->Description,
                bOwned ? FText::GetEmpty() : LOCTEXT("WeaponLocked", "Оружие ещё не получено."),
                Row->Icon, bOwned, Id == SelectedWeapon, Row->DisplayOrder});
        }
    }
    Cards.Sort([](const FGirlSaviorWeaponCardView& A, const FGirlSaviorWeaponCardView& B)
    {
        return A.DisplayOrder == B.DisplayOrder ? A.WeaponId.LexicalLess(B.WeaponId) : A.DisplayOrder < B.DisplayOrder;
    });

    const FName SlotIds[] = {WeaponDraft.Slot1, WeaponDraft.Slot2, WeaponDraft.Slot3};
    TArray<FGirlSaviorResolvedWeaponSlot> Resolved;
    FText Error;
    const bool bEmpty = WeaponDraft.Slot1.IsNone() && WeaponDraft.Slot2.IsNone() && WeaponDraft.Slot3.IsNone();
    const bool bCanConfirm = !bEmpty && Profile->ValidateLoadout(Table, WeaponDraft, Resolved, Error);
    if (bEmpty)
    {
        Error = LOCTEXT("ChooseWeapon", "Добавьте хотя бы одно оружие.");
    }
    Widget->Refresh(Cards, BuildWeaponSlotLabels(&Table, &WeaponDraft), SlotIds, !SelectedWeapon.IsNone(),
        bCanConfirm, Status.IsEmpty() ? Error : Status);
}

// Return to the exact view and action that opened the page.
void AGirlSaviorCampPlayerController::CloseWeaponLoadout()
{
    if (ModalWidget)
    {
        ModalWidget->RemoveFromParent();
    }
    ModalWidget = TableWidget;
    Page = WeaponReturnPage;
    WeaponDraft = {};
    SelectedWeapon = NAME_None;
    if (TableWidget)
    {
        TableWidget->SetVisibility(ESlateVisibility::Visible);
        RefreshPreparation();
        FInputModeUIOnly InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetWidgetToFocus(TableWidget->TakeWidget());
        SetInputMode(InputMode);
        TableWidget->FocusWeaponEntry(Page == EGirlSaviorCampPage::Preparation);
    }
}

// Keep the same draft and modal when moving from missions to preparation.
void AGirlSaviorCampPlayerController::OpenPreparationPage()
{
    const auto* Catalog = ActiveCatalog.Get();
    if (Page == EGirlSaviorCampPage::Missions && Catalog && ModalWidget && !IsLaunchInProgress())
    {
        FText Error;
        if (Catalog->ValidateCatalog(Error))
        {
            Page = EGirlSaviorCampPage::Preparation;
        }
        RefreshPreparation();
        ModalWidget->SetUserFocus(this);
    }
}

// Apply a toggle by stable ID and keep any rejected selection feedback on the same page.
void AGirlSaviorCampPlayerController::ToggleAbility(FName AbilityId)
{
    const auto* Catalog = ActiveCatalog.Get();
    if (Page != EGirlSaviorCampPage::Preparation || !Catalog || IsLaunchInProgress())
    {
        return;
    }

    const EGirlSaviorCampSelectionChange Change = Draft.ToggleAbility(*Catalog, AbilityId);
    FText Status;
    if (Change == EGirlSaviorCampSelectionChange::Full)
    {
        Status = LOCTEXT("SelectionFull", "Уберите одно умение, чтобы выбрать другое.");
    }
    else if (Change == EGirlSaviorCampSelectionChange::Unavailable)
    {
        Status = LOCTEXT("AbilityUnavailable", "Это умение недоступно.");
    }
    RefreshPreparation(Status);
}

// Refresh only the presentation. There is one native eligibility rule for both the UI and the action.
void AGirlSaviorCampPlayerController::RefreshPreparation(const FText& Status)
{
    const auto* Catalog = ActiveCatalog.Get();
    auto* Widget = Cast<UGirlSaviorCampPreparationWidget>(ModalWidget);
    if (!Catalog || !Widget)
    {
        return;
    }

    FGirlSaviorSupportOffer First;
    FGirlSaviorSupportOffer Second;
    FText Error;
    const bool bOnPreparation = Page == EGirlSaviorCampPage::Preparation;
    const bool bLaunchInProgress = IsLaunchInProgress();
    const bool bValidCatalog = bOnPreparation || Catalog->ValidateCatalog(Error);
    const bool bValidSupport = bOnPreparation && !bLaunchInProgress && Draft.ResolveForLaunch(*Catalog, First, Second, Error);
    FText ProfileError;
    auto* Profile = GetWeaponProfile();
    const bool bProfileAvailable = Catalog->WeaponTable && Profile && Profile->EnsureLoaded(*Catalog->WeaponTable, ProfileError);
    FText VisibleStatus = Status;
    if (!bValidCatalog)
    {
        UE_LOG(LogGirlSaviorCamp, Warning, TEXT("%s"), *Error.ToString());
        VisibleStatus = LOCTEXT("InvalidCatalog", "Данные вылазки недоступны.");
    }
    else if (bOnPreparation && Draft.GetAbilityIds().Num() == FGirlSaviorCampPreparation::SlotCount
        && !bValidSupport && VisibleStatus.IsEmpty() && !bLaunchInProgress)
    {
        UE_LOG(LogGirlSaviorCamp, Warning, TEXT("%s"), *Error.ToString());
        VisibleStatus = LOCTEXT("InvalidPreparation", "Проверьте карты вылазки и выбранные умения.");
    }
    if (!bProfileAvailable && VisibleStatus.IsEmpty())
    {
        VisibleStatus = ProfileError.IsEmpty() ? LOCTEXT("WeaponCatalogUnavailable", "Данные оружия недоступны.") : ProfileError;
    }
    const auto Labels = BuildWeaponSlotLabels(Catalog->WeaponTable.Get(),
        bProfileAvailable ? &Profile->GetConfirmedLoadout() : nullptr);
    Widget->Refresh(*Catalog, Draft.GetAbilityIds(), bOnPreparation, bValidSupport && bProfileAvailable,
        VisibleStatus, Labels, bProfileAvailable && !bLaunchInProgress, bValidCatalog && !bLaunchInProgress);
}

// The session validates and snapshots the draft before scheduling absolute map travel.
void AGirlSaviorCampPlayerController::RequestLaunch()
{
    const auto* Catalog = ActiveCatalog.Get();
    if (Page != EGirlSaviorCampPage::Preparation || !Catalog || IsLaunchInProgress())
    {
        return;
    }

    FText Error;
    auto* Session = GetExpeditionSession();
    if (!Session || !Session->RequestLaunch(*GetWorld(), *Catalog, Draft, Error))
    {
        RefreshPreparation(Error.IsEmpty() ? LOCTEXT("InvalidLaunch", "Не удалось отправиться в вылазку.") : Error);
        if (auto* Preparation = Cast<UGirlSaviorCampPreparationWidget>(ModalWidget))
        {
            Preparation->FocusAfterLaunchFailure();
        }
        return;
    }
    RefreshPreparation(LOCTEXT("TravelPending", "Загрузка боевого маршрута…"));
}

UGirlSaviorExpeditionSessionSubsystem* AGirlSaviorCampPlayerController::GetExpeditionSession() const
{
    return UGameInstance::GetSubsystem<UGirlSaviorExpeditionSessionSubsystem>(GetGameInstance());
}

UGirlSaviorWeaponProfileSubsystem* AGirlSaviorCampPlayerController::GetWeaponProfile() const
{
    return UGameInstance::GetSubsystem<UGirlSaviorWeaponProfileSubsystem>(GetGameInstance());
}

// Keep button, keyboard and Blueprint requests behind the same pending-travel gate.
bool AGirlSaviorCampPlayerController::IsLaunchInProgress() const
{
    const auto* Session = GetExpeditionSession();
    return Session && Session->IsLaunchPendingFrom(*GetWorld());
}

// Back from preparation preserves the draft. Back from a top-level panel ends the visit.
void AGirlSaviorCampPlayerController::RequestBack()
{
    if (IsLaunchInProgress())
    {
        return;
    }
    if (Page == EGirlSaviorCampPage::Weapons)
    {
        CloseWeaponLoadout();
    }
    else if (Page == EGirlSaviorCampPage::Preparation && ModalWidget)
    {
        Page = EGirlSaviorCampPage::Missions;
        RefreshPreparation();
        ModalWidget->SetUserFocus(this);
    }
    else if (Page != EGirlSaviorCampPage::Walking)
    {
        CloseModal();
    }
}

// Restore gameplay only after clearing held keys and returning keyboard focus to the viewport.
void AGirlSaviorCampPlayerController::CloseModal()
{
    ResetPanel();
    FlushPressedKeys();
    RestoreGameplayInputMode();
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
    SetGameplayInputAvailable(true);
    PollInteraction();
}

// Dismiss the modal and discard visit state without touching the input mode.
void AGirlSaviorCampPlayerController::ResetPanel()
{
    if (TableWidget && TableWidget.Get() != ModalWidget.Get())
    {
        TableWidget->RemoveFromParent();
    }
    TableWidget = nullptr;
    if (ModalWidget)
    {
        ModalWidget->RemoveFromParent();
        ModalWidget = nullptr;
    }
    ActiveCatalog.Reset();
    Draft = {};
    WeaponDraft = {};
    SelectedWeapon = NAME_None;
    Page = EGirlSaviorCampPage::Walking;
}

// Clear timers and widget references when leaving or replacing the Camp pawn.
void AGirlSaviorCampPlayerController::ResetCamp()
{
    GetWorldTimerManager().ClearTimer(InteractionPoll);
    ResetPanel();
    DisplayedTarget.Reset();
    ExpeditionTable.Reset();
    if (PromptWidget)
    {
        PromptWidget->RemoveFromParent();
        PromptWidget = nullptr;
    }
}

#undef LOCTEXT_NAMESPACE
