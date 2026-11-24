#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Camp/GirlSaviorCampInteractable.h"
#include "Camp/GirlSaviorCampPlayerController.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#include <algorithm>
#include <initializer_list>

namespace GirlSavior::CampPIE
{
DEFINE_LOG_CATEGORY_STATIC(LogCampPIE, Log, All);

class FProbe
{
public:
    // Work on the current play world only, and look up the saved interactables by their stable IDs.
    explicit FProbe(UWorld& PlayWorld)
        : World(&PlayWorld)
    {
        Controller = Cast<AGirlSaviorCampPlayerController>(PlayWorld.GetFirstPlayerController());
        Hero = Controller.IsValid() ? Cast<AGirlSaviorCharacter>(Controller->GetPawn()) : nullptr;
        Camera = Controller.IsValid() ? Cast<AGirlSaviorPlayerCameraManager>(Controller->PlayerCameraManager) : nullptr;
        for (TActorIterator<AGirlSaviorCampInteractable> It(&PlayWorld); It; ++It)
        {
            Targets.Add(It->GetInteractionId(), *It);
        }

        if (!Controller.IsValid() || !Hero.IsValid() || !Camera.IsValid() || !FindTarget(TEXT("ExpeditionTable"))
            || !FindTarget(TEXT("Liora")) || !FindTarget(TEXT("Ida")) || !FindTarget(TEXT("Saira")))
        {
            UE_LOG(LogCampPIE, Error, TEXT("Camp PIE probe requires its local pawn, camera, table and three heroines."));
            return;
        }

        FocusPlayViewport();
        BuildSteps();
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FProbe::Tick);
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FProbe::CleanupWorld);
    }

    ~FProbe() { Stop(); }

private:
    using FStep = TFunction<void()>;

    // Explicit console runs focus the PIE viewport before exercising the real input gate.
    void FocusPlayViewport() const
    {
        const ULocalPlayer* Player = Controller->GetLocalPlayer();
        const auto Viewport = Player && Player->ViewportClient ? Player->ViewportClient->GetGameViewportWidget() : nullptr;
        if (Viewport)
        {
            auto& Slate = FSlateApplication::Get();
            if (const auto Window = Slate.FindWidgetWindow(Viewport.ToSharedRef()))
            {
                Window->BringToFront(true);
            }
            Slate.SetUserFocus(Slate.GetUserIndexForKeyboard(), Viewport, EFocusCause::SetDirectly);
        }
    }

    // Use short holds with separate release frames, as Enhanced Input samples keys on tick.
    void Later(FStep Step, float Wait = 0.20f)
    {
        Steps.Add(MoveTemp(Step));
        Waits.Add(Wait);
    }

    void PressE()
    {
        Later([this] { Send(EKeys::E, IE_Pressed); });
        Later([this] { Send(EKeys::E, IE_Released); });
    }

    // Route Escape through Slate to exercise the preview handler on the modal's focused widget.
    void Escape() const
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        const FKeyEvent Event(EKeys::Escape, Slate.GetModifierKeys(), 0, false, 0, 0);
        Slate.ProcessKeyDownEvent(Event);
        Slate.ProcessKeyUpEvent(Event);
    }

    // Send game keys with the actual owning viewport, as physical fire admission also checks it.
    void Send(const FKey& Key, EInputEvent Event) const
    {
        if (Controller.IsValid())
        {
            const ULocalPlayer* Player = Controller->GetLocalPlayer();
            FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
            Controller->InputKey(FInputKeyEventArgs(Viewport, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
        }
    }

    AGirlSaviorCampInteractable* FindTarget(FName Id) const
    {
        const auto* Target = Targets.Find(Id);
        return Target ? Target->Get() : nullptr;
    }

    FName PromptId() const
    {
        const auto* Target = Controller->GetDisplayedTarget();
        return Target ? Target->GetInteractionId() : NAME_None;
    }

    UWidget* Widget(FName Name) const
    {
        const UUserWidget* Modal = Controller->GetModalWidget();
        return Modal ? Modal->GetWidgetFromName(Name) : nullptr;
    }

    // Check the visible button binding and eligibility together.
    void Click(FName Name)
    {
        auto* Button = Cast<UButton>(Widget(Name));
        if (Check(Button && Button->GetIsEnabled(), FString::Printf(TEXT("Button %s is available"), *Name.ToString())))
        {
            Button->OnClicked.Broadcast();
        }
    }

    // Exercise Slate hit-testing on a real button in the PIE viewport.
    void ClickWithPointer(FName Name)
    {
        auto* Button = Cast<UButton>(Widget(Name));
        const auto WidgetSlate = Button ? Button->GetCachedWidget() : nullptr;
        auto& Slate = FSlateApplication::Get();
        const auto Window = WidgetSlate ? Slate.FindWidgetWindow(WidgetSlate.ToSharedRef()) : nullptr;
        if (Check(Button && Button->GetIsEnabled() && Window, TEXT("Mission setup button is rendered and enabled")))
        {
            const FGeometry& Geometry = Button->GetCachedGeometry();
            const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
            const FVector2D InitialCursor = Slate.GetCursorPos();
            Slate.SetCursorPos(Center);
            const uint32 UserIndex = Slate.GetUserIndexForKeyboard();
            const uint32 PointerIndex = FSlateApplication::CursorPointerIndex;
            const FModifierKeysState Modifiers = Slate.GetModifierKeys();
            // Slate buttons require a hovered pointer on release. A programmatic move needs its own event.
            Slate.ProcessMouseMoveEvent(FPointerEvent(UserIndex, PointerIndex,
                Center, InitialCursor, {}, EKeys::Invalid, 0.f, Modifiers));
            Check(WidgetSlate->IsHovered(), TEXT("PIE pointer hits the mission setup button"));
            Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), FPointerEvent(UserIndex, PointerIndex,
                Center, Center, {EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, Modifiers));
            Slate.ProcessMouseButtonUpEvent(FPointerEvent(UserIndex, PointerIndex,
                Center, Center, {}, EKeys::LeftMouseButton, 0.f, Modifiers));
        }
    }

    bool LaunchEnabled() const
    {
        const auto* Launch = Cast<UButton>(Widget(TEXT("LaunchButton")));
        return Launch && Launch->GetIsEnabled();
    }

    bool PageIs(EGirlSaviorCampPage Page) const
    {
        return Controller->GetCurrentPage() == Page;
    }

    bool HasDraft(std::initializer_list<FName> Ids) const
    {
        const TConstArrayView<FName> Actual = Controller->GetDraft().GetAbilityIds();
        return Actual.Num() == static_cast<int32>(Ids.size()) &&
            std::equal(Actual.begin(), Actual.end(), Ids.begin());
    }

    bool TextContains(FName Name, const TCHAR* Text) const
    {
        const auto* Label = Cast<UTextBlock>(Widget(Name));
        return Label && Label->GetText().ToString().Contains(Text);
    }

    // Move the hero without saving any fixture changes for the controlled interaction cases.
    void Place(const FVector& Position) const
    {
        Hero->GetCharacterMovement()->StopMovementImmediately();
        Hero->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Camera->RequestFollowSnap();
    }

    // Drive the normal screen-relative mappings, slowing near waypoints by releasing keys.
    void WalkTo(const FVector& Destination, FName WantedPrompt = NAME_None, bool bTestCombatKeys = false)
    {
        WalkDestination = Destination;
        WalkPrompt = WantedPrompt;
        bWalking = true;
        bCombatKeysHeld = bTestCombatKeys;
        WalkElapsed = 0.f;
        UpdateWalkKeys();
        if (bCombatKeysHeld)
        {
            Send(EKeys::LeftShift, IE_Pressed);
            Send(EKeys::LeftMouseButton, IE_Pressed);
        }
    }

    void UpdateWalkKeys()
    {
        const FVector Direction = (WalkDestination - Hero->GetActorLocation()).GetSafeNormal2D();
        const double Across = FVector::DotProduct(Direction, Camera->ToWorldMoveIntent(FVector2D(1, 0)));
        const double Along = FVector::DotProduct(Direction, Camera->ToWorldMoveIntent(FVector2D(0, 1)));
        TArray<FKey> NewKeys;
        if (FMath::Abs(Across) > 0.38)
        {
            NewKeys.Add(Across > 0 ? EKeys::D : EKeys::A);
        }
        if (FMath::Abs(Along) > 0.38)
        {
            NewKeys.Add(Along > 0 ? EKeys::W : EKeys::S);
        }
        for (const auto& Key : WalkKeys)
        {
            if (!NewKeys.Contains(Key))
            {
                Send(Key, IE_Released);
            }
        }
        for (const auto& Key : NewKeys)
        {
            if (!WalkKeys.Contains(Key))
            {
                Send(Key, IE_Pressed);
            }
        }
        WalkKeys = MoveTemp(NewKeys);
    }

    void EndWalk()
    {
        for (const auto& Key : WalkKeys)
        {
            Send(Key, IE_Released);
        }
        WalkKeys.Reset();
        if (bCombatKeysHeld)
        {
            Send(EKeys::LeftShift, IE_Released);
            Send(EKeys::LeftMouseButton, IE_Released);
            bCombatKeysHeld = false;
        }
        bWalking = false;
    }

    // A query-only box tests chest-to-anchor visibility independently of overlap and movement.
    void AddVisibilityBlocker()
    {
        Blocker = World->SpawnActor<AActor>();
        if (!Blocker.IsValid())
        {
            return;
        }
        auto* Box = NewObject<UBoxComponent>(Blocker.Get());
        Blocker->SetRootComponent(Box);
        const FVector Anchor = FindTarget(TEXT("ExpeditionTable"))->GetInteractionLocation();
        Box->SetWorldLocation((Hero->GetActorLocation() + Anchor) * 0.5 + FVector(0, 0, 18));
        Box->SetBoxExtent(FVector(25, 25, 60));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->RegisterComponent();
    }

    void RemoveVisibilityBlocker()
    {
        if (Blocker.IsValid())
        {
            Blocker->Destroy();
        }
        Blocker.Reset();
    }

    // Walk into each heroine's prompt and verify the profile matches the nearby target.
    void AddHeroineSteps(FName Id, const TCHAR* LocalName)
    {
        Later([this, Id]
        {
            const FVector Anchor = FindTarget(Id)->GetInteractionLocation();
            WalkTo(Anchor, Id);
        });
        Later([this, Id]
        {
            Check(PromptId() == Id, FString::Printf(TEXT("WASD reaches the heroine prompt: %s"), *Id.ToString()));
        });
        PressE();
        Later([this, Id, LocalName]
        {
            Check(PageIs(EGirlSaviorCampPage::Heroine) && TextContains(TEXT("CampHeroineName"), LocalName),
                FString::Printf(TEXT("E opens the corresponding heroine profile: %s"), *Id.ToString()));
            Check(PromptId().IsNone() && !Controller->GetGameplayInputAvailable(), TEXT("Heroine profile owns focus"));
            Click(TEXT("HeroineBackButton"));
        });
        Later([this] { Check(PageIs(EGirlSaviorCampPage::Walking), TEXT("Heroine Back returns to walking")); });
    }

    // Use the actual save's spawn and UI bindings before any isolated positioning cases.
    void BuildSteps()
    {
        Later([this]
        {
            SpawnPosition = Hero->GetActorLocation();
            Check(World->GetMapName().Contains(TEXT("CampPrototype"))
                && Controller->GetClass()->GetName().Contains(TEXT("BP_CampPlayerController"))
                && Hero->GetClass()->GetName().Contains(TEXT("BP_Character"))
                && Camera->GetClass()->GetName().Contains(TEXT("BP_CameraManager")),
                TEXT("Camp uses its controller and the shared pawn and camera"));
            Check(FMath::IsNearlyEqual(FVector::Dist2D(SpawnPosition,
                FindTarget(TEXT("ExpeditionTable"))->GetInteractionLocation()), 300.0, 30.0),
                TEXT("Spawn is about 300 cm from the expedition table"));
            if (!Check(Controller->CanUseGameplayInput(), TEXT("PIE game viewport has gameplay focus")))
            {
                Finish();
                return;
            }
            Check(PromptId().IsNone(), TEXT("Spawn is outside interaction range"));
            FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(
                FPaths::ProjectSavedDir() / TEXT("Camp2/CampWalk.png")), true, false);
        }, 0.5f);
        PressE();
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Walking), TEXT("E away from interactables does nothing"));
            WalkTo(FindTarget(TEXT("ExpeditionTable"))->GetInteractionLocation(), TEXT("ExpeditionTable"), true);
        });
        Later([this]
        {
            const auto* Movement = CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
            Check(!Movement->IsDashing() && Movement->GetDashCooldownRemaining() == 0,
                TEXT("Left Shift while walking does not start or spend a dash"));
            Check(!Controller->IsFireAllowed() && !TActorIterator<AGirlSaviorTestProjectile>(World.Get()),
                TEXT("LMB is blocked and spawns no projectile"));
            Check(FVector::Dist2D(SpawnPosition, Hero->GetActorLocation()) > 60 && PromptId() == TEXT("ExpeditionTable"),
                TEXT("WASD walks from spawn into the table prompt"));
        });
        PressE();
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Missions) && TextContains(TEXT("CampMissionName"), TEXT("Боевой маршрут")),
                TEXT("E opens the table mission list"));
            Check(PromptId().IsNone() && !Controller->GetGameplayInputAvailable() && !Controller->IsPaused(),
                TEXT("Modal hides prompt and suspends gameplay without pausing"));
            ClickWithPointer(TEXT("MissionConfigureButton"));
            const auto* Switcher = Cast<UWidgetSwitcher>(Widget(TEXT("CampPageSwitcher")));
            if (!Check(PageIs(EGirlSaviorCampPage::Preparation) && Switcher && Switcher->GetActiveWidgetIndex() == 1,
                TEXT("PIE mouse click switches to the separate preparation page")))
            {
                Finish();
                return;
            }
            Check(HasDraft({}) && !LaunchEnabled(), TEXT("Empty draft cannot launch"));
            ModalPosition = Hero->GetActorLocation();
            Send(EKeys::W, IE_Pressed);
        });
        Later([this]
        {
            Send(EKeys::W, IE_Released);
            Check(FVector::Dist2D(ModalPosition, Hero->GetActorLocation()) <= 1.0
                && Hero->GetVelocity().IsNearlyZero(), TEXT("WASD cannot move the hero while the modal is open"));
            Click(TEXT("CampAbilityButton0"));
            Check(HasDraft({TEXT("StaticTrace")}) && !LaunchEnabled(), TEXT("One card cannot launch"));
            Click(TEXT("CampAbilityButton1"));
            Check(HasDraft({TEXT("StaticTrace"), TEXT("MechanismOverdrive")}) && LaunchEnabled(),
                TEXT("Two cards enable the launch control"));
            Click(TEXT("CampAbilityButton2"));
            Check(HasDraft({TEXT("StaticTrace"), TEXT("MechanismOverdrive")})
                && TextContains(TEXT("CampSelectionStatus"), TEXT("Уберите одно умение")), TEXT("A third card is refused"));
            Click(TEXT("CampAbilityButton0"));
            Check(HasDraft({TEXT("MechanismOverdrive")}) && !LaunchEnabled(), TEXT("Selecting an occupied card removes it"));
            Click(TEXT("CampAbilityButton2"));
            Check(HasDraft({TEXT("MechanismOverdrive"), TEXT("TravelersBarrier")}) && LaunchEnabled(),
                TEXT("Removing a card lets the player select another"));
            FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(
                FPaths::ProjectSavedDir() / TEXT("Camp2/CampPreparation.png")), true, false);
        });
        Later([this]
        {
            Check(LaunchEnabled(), TEXT("A complete draft is ready for the separate travel acceptance test"));
            FSlateApplication& Slate = FSlateApplication::Get();
            Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, Slate.GetModifierKeys(), 0, false, 0, 0));
            Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, Slate.GetModifierKeys(), 0, true, 0, 0));
            Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::Escape, Slate.GetModifierKeys(), 0, false, 0, 0));
        });
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Missions)
                && HasDraft({TEXT("MechanismOverdrive"), TEXT("TravelersBarrier")}), TEXT("Escape returns to missions and repeats preserve the draft"));
            Click(TEXT("MissionConfigureButton"));
            Check(HasDraft({TEXT("MechanismOverdrive"), TEXT("TravelersBarrier")}) && LaunchEnabled(),
                TEXT("Re-entering preparation shows the same draft"));
            Click(TEXT("PreparationBackButton"));
            Escape();
        });
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Walking) && Controller->CanUseGameplayInput(),
                TEXT("Escape closes the table and restores gameplay focus"));
            RestoredPosition = Hero->GetActorLocation();
            Send(EKeys::S, IE_Pressed);
        });
        Later([this]
        {
            Send(EKeys::S, IE_Released);
            Check(FVector::Dist2D(RestoredPosition, Hero->GetActorLocation()) > 5.0,
                TEXT("Movement resumes with a fresh key press"));
            Place(FindTarget(TEXT("ExpeditionTable"))->GetInteractionLocation() + FVector(-115, -65, 0));
        });
        PressE();
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Missions) && HasDraft({}), TEXT("Reopening the table starts a fresh draft"));
            Click(TEXT("MissionBackButton"));
        });
        Later([this] { AddVisibilityBlocker(); });
        PressE();
        Later([this]
        {
            Check(Blocker.IsValid() && PromptId().IsNone() && PageIs(EGirlSaviorCampPage::Walking),
                TEXT("An overlap behind visibility cover shows no prompt and does not interact"));
            RemoveVisibilityBlocker();

            auto* Liora = FindTarget(TEXT("Liora"));
            SavedLioraPosition = Liora->GetActorLocation();
            Liora->SetActorLocation(Hero->GetActorLocation() + FVector(0, 100, 0), false, nullptr, ETeleportType::TeleportPhysics);
            bMovedLiora = true;
        });
        Later([this]
        {
            Check(PromptId() == TEXT("Liora"), TEXT("The closer of two overlapping targets has the prompt"));
        });
        PressE();
        Later([this]
        {
            Check(PageIs(EGirlSaviorCampPage::Heroine) && TextContains(TEXT("CampHeroineName"), TEXT("Лиора")),
                TEXT("E opens the same competing target shown in the prompt"));
            Escape();
            RestoreLiora();
        });
        AddHeroineSteps(TEXT("Liora"), TEXT("Лиора"));
        // Pass along the west side to clear the table's blocking footprint.
        Later([this] { WalkTo(FVector(-650, -610, 90)); });
        Later([this] { WalkTo(FVector(-640, 190, 90)); });
        AddHeroineSteps(TEXT("Ida"), TEXT("Ида"));
        AddHeroineSteps(TEXT("Saira"), TEXT("Сайра"));
    }

    // Advance one action per delay so input, Slate and the 10 Hz poll have time to settle.
    void Tick(UWorld* TickWorld, ELevelTick, float DeltaSeconds)
    {
        if (TickWorld != World.Get())
        {
            return;
        }
        if (!Hero.IsValid() || !Controller.IsValid())
        {
            Check(false, TEXT("Probe lost its possessed Camp hero"));
            Finish();
            return;
        }
        if (bWalking)
        {
            WalkElapsed += DeltaSeconds;
            if ((!WalkPrompt.IsNone() && WalkElapsed > 0.12f && PromptId() == WalkPrompt)
                || (WalkPrompt.IsNone() && FVector::DistSquared2D(WalkDestination, Hero->GetActorLocation()) < FMath::Square(40.0))
                || WalkElapsed > 4.0f)
            {
                EndWalk();
            }
            else
            {
                UpdateWalkKeys();
            }
            return;
        }
        StepElapsed += DeltaSeconds;
        if (StepElapsed < Waits[StepIndex])
        {
            return;
        }
        StepElapsed = 0.0f;
        Steps[StepIndex++]();
        if (StepIndex == Steps.Num())
        {
            Finish();
        }
    }

    bool Check(bool bPassed, const FString& Message)
    {
        ++CheckCount;
        if (bPassed)
        {
            UE_LOG(LogCampPIE, Display, TEXT("PASS %s"), *Message);
        }
        else
        {
            ++FailureCount;
            UE_LOG(LogCampPIE, Error, TEXT("FAIL %s"), *Message);
        }
        return bPassed;
    }

    void Finish()
    {
        UE_LOG(LogCampPIE, Display, TEXT("Camp PIE completed: %d checks, %d failures."), CheckCount, FailureCount);
        Stop();
    }

    void RestoreLiora()
    {
        if (bMovedLiora)
        {
            if (auto* Liora = FindTarget(TEXT("Liora")))
            {
                Liora->SetActorLocation(SavedLioraPosition, false, nullptr, ETeleportType::TeleportPhysics);
            }
            bMovedLiora = false;
        }
    }

    // Restore the temporary competing actor and remove the test collider from the play world.
    void Stop()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        if (bWalking)
        {
            EndWalk();
        }
        RestoreLiora();
        RemoveVisibilityBlocker();
    }

    void CleanupWorld(UWorld* Cleanup, bool, bool)
    {
        if (Cleanup == World.Get())
        {
            Stop();
        }
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGirlSaviorCampPlayerController> Controller;
    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TWeakObjectPtr<AGirlSaviorPlayerCameraManager> Camera;
    TWeakObjectPtr<AActor> Blocker;
    TMap<FName, TWeakObjectPtr<AGirlSaviorCampInteractable>> Targets;
    TArray<FStep> Steps;
    TArray<float> Waits;
    TArray<FKey> WalkKeys;
    FVector SpawnPosition = FVector::ZeroVector;
    FVector ModalPosition = FVector::ZeroVector;
    FVector RestoredPosition = FVector::ZeroVector;
    FVector SavedLioraPosition = FVector::ZeroVector;
    FVector WalkDestination = FVector::ZeroVector;
    FName WalkPrompt;
    FDelegateHandle TickHandle;
    FDelegateHandle CleanupHandle;
    int32 StepIndex = 0;
    int32 CheckCount = 0;
    int32 FailureCount = 0;
    float StepElapsed = 0.f;
    float WalkElapsed = 0.f;
    bool bWalking = false;
    bool bCombatKeysHeld = false;
    bool bMovedLiora = false;
};

TUniquePtr<FProbe> ActiveCampProbe;
FAutoConsoleCommandWithWorldAndArgs TestCampCommand(TEXT("GirlSavior.TestCamp"),
    TEXT("Verify CAMP 2 in a fresh focused CampPrototype PIE session."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE)
        {
            ActiveCampProbe.Reset();
            ActiveCampProbe = MakeUnique<FProbe>(*World);
        }
    }));
}
#endif
