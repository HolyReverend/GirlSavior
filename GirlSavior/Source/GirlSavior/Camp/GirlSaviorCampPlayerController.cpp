#include "Camp/GirlSaviorCampPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camp/GirlSaviorCampInteractable.h"
#include "Camp/GirlSaviorCampPrototypeDefinition.h"
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

#define LOCTEXT_NAMESPACE "GirlSaviorCampPlayerController"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorCamp, Log, All);

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
void AGirlSaviorCampPlayerController::OpenExpeditionTable(const UGirlSaviorCampPrototypeDefinition& Catalog)
{
    if (Page != EGirlSaviorCampPage::Walking || !CanUseGameplayInput() || !PreparationWidgetClass)
    {
        return;
    }
    auto* Widget = CreateWidget<UGirlSaviorCampPreparationWidget>(this, PreparationWidgetClass);
    if (PresentModal(Widget, EGirlSaviorCampPage::Missions))
    {
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
    const UGirlSaviorCampPrototypeDefinition* Catalog = Table ? Table->GetCatalog() : nullptr;
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
    if (UPawnMovementComponent* Movement = GetPawn()->GetMovementComponent())
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
    const bool bCanLaunch = bOnPreparation && Draft.ResolveForLaunch(*Catalog, First, Second, Error) && !IsLaunchInProgress();
    FText VisibleStatus = Status;
    if (!bOnPreparation && !Catalog->ValidateCatalog(Error))
    {
        UE_LOG(LogGirlSaviorCamp, Warning, TEXT("%s"), *Error.ToString());
        VisibleStatus = LOCTEXT("InvalidCatalog", "Данные вылазки недоступны.");
    }
    else if (bOnPreparation && Draft.GetAbilityIds().Num() == FGirlSaviorCampPreparation::SlotCount
        && !bCanLaunch && VisibleStatus.IsEmpty() && !IsLaunchInProgress())
    {
        UE_LOG(LogGirlSaviorCamp, Warning, TEXT("%s"), *Error.ToString());
        VisibleStatus = LOCTEXT("InvalidPreparation", "Проверьте карты вылазки и выбранные умения.");
    }
    Widget->Refresh(*Catalog, Draft.GetAbilityIds(), bOnPreparation, bCanLaunch, VisibleStatus);
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
    if (Page == EGirlSaviorCampPage::Preparation && ModalWidget)
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
    if (ModalWidget)
    {
        ModalWidget->RemoveFromParent();
        ModalWidget = nullptr;
    }
    ActiveCatalog.Reset();
    Draft = {};
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
