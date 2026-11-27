#if WITH_EDITOR || UE_BUILD_DEVELOPMENT

#include "Camp/GirlSaviorCampPlayerController.h"
#include "Camp/GirlSaviorCampDefinition.h"
#include "Camp/GirlSaviorCampWidgets.h"
#include "Camp/GirlSaviorExpeditionTable.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Components/BoxComponent.h"
#include "Containers/Ticker.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatHUD.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Weapons/GirlSaviorWeaponLoadoutWidget.h"
#include "Weapons/GirlSaviorFirearm.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponLoadoutPIE, Log, All);

namespace
{
class FWeaponLoadoutProbe
{
public:
    FWeaponLoadoutProbe(UWorld& World, bool bInWrite, bool bInFull)
        : Instance(World.GetGameInstance()), bWrite(bInWrite), bFull(bInFull),
          bPackaged(World.WorldType == EWorldType::Game), Deadline(FPlatformTime::Seconds() + 45.0)
    {
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FWeaponLoadoutProbe::Tick);
    }

    ~FWeaponLoadoutProbe()
    {
        Detach();
    }

private:
    enum class EStep
    {
        Camp, Mission, Edit, Confirmed, Preparation, Cancel, Launch, Route, Unpossessed,
        Rebound, Aim, Shoot, Replace, Replaced, Fight, Outcome, Return,
        FullWarmup, FullSelect, FullSelected, FullFire, FullObserve
    };

    void Detach()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        TickHandle.Reset();
        if (auto* Fire = ObservedFire.Get())
        {
            Fire->OnWeaponShot.RemoveAll(this);
        }
        ObservedFire.Reset();
    }

    void Finish(bool bPass, const TCHAR* Label)
    {
        if (bPass)
        {
            UE_LOG(LogWeaponLoadoutPIE, Display, TEXT("%s RESULT PASS %s"), bFull ? TEXT("W4") : TEXT("W3"), Label);
        }
        else
        {
            UE_LOG(LogWeaponLoadoutPIE, Error, TEXT("%s RESULT FAIL %s"), bFull ? TEXT("W4") : TEXT("W3"), Label);
        }
        Detach();
        if (bPackaged)
        {
            FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1, TEXT("WeaponLoadoutProbe"));
        }
    }

    bool Check(bool bPass, const TCHAR* Label)
    {
        if (bPass)
        {
            UE_LOG(LogWeaponLoadoutPIE, Display, TEXT("%s PASS %s"), bFull ? TEXT("W4") : TEXT("W3"), Label);
            return true;
        }
        Finish(false, Label);
        return false;
    }

    bool Click(UButton* Button)
    {
        if (!Check(Button && Button->GetIsEnabled(), TEXT("Widget action is available")))
        {
            return false;
        }
        Button->OnClicked.Broadcast();
        return true;
    }

    static void Focus(APlayerController& Controller)
    {
        const auto* Player = Controller.GetLocalPlayer();
        const auto Viewport = Player && Player->ViewportClient ? Player->ViewportClient->GetGameViewportWidget() : nullptr;
        if (Viewport && FSlateApplication::IsInitialized())
        {
            auto& Slate = FSlateApplication::Get();
            if (const auto Window = Slate.FindWidgetWindow(Viewport.ToSharedRef()))
            {
                Window->BringToFront(true);
            }
            Slate.SetAllUserFocus(Viewport, EFocusCause::SetDirectly);
        }
    }

    static void SendUIKey(const FKey& Key)
    {
        auto& Slate = FSlateApplication::Get();
        const FKeyEvent Event(Key, Slate.GetModifierKeys(), 0, false, 0, 0);
        Slate.ProcessKeyDownEvent(Event);
        Slate.ProcessKeyUpEvent(Event);
    }

    bool IsSavedSet(const FGirlSaviorWeaponLoadout& Set) const
    {
        if (bFull)
        {
            return Set.Slot1 == TEXT("Shotgun") && Set.Slot2 == TEXT("Pistol") && Set.Slot3 == TEXT("AssaultRifle");
        }
        return Set.Slot1.IsNone() && Set.Slot2 == TEXT("AssaultRifle") && Set.Slot3 == TEXT("Shotgun");
    }

    void Tick(UWorld* World, ELevelTick, float)
    {
        if (!World || World->GetGameInstance() != Instance.Get() || !World->IsGameWorld())
        {
            return;
        }
        if (FPlatformTime::Seconds() > Deadline)
        {
            Check(false, *FString::Printf(TEXT("Timed out at step %d"), int32(Step)));
            return;
        }

        auto* PC = World->GetFirstPlayerController();
        if (!PC)
        {
            return;
        }
        if (auto* Camp = Cast<AGirlSaviorCampPlayerController>(PC))
        {
            TickCamp(*World, *Camp);
        }
        else
        {
            TickRoute(*World, *PC);
        }
    }

    void TickCamp(UWorld& World, AGirlSaviorCampPlayerController& Camp)
    {
        auto* Parent = Cast<UGirlSaviorCampPreparationWidget>(const_cast<UUserWidget*>(Camp.GetModalWidget()));
        auto* Editor = Cast<UGirlSaviorWeaponLoadoutWidget>(const_cast<UUserWidget*>(Camp.GetModalWidget()));
        switch (Step)
        {
        case EStep::Camp:
        case EStep::Return:
            if (!Camp.CanUseGameplayInput())
            {
                Focus(Camp);
                return;
            }
            if (TActorIterator<AGirlSaviorExpeditionTable> It(&World); It && It->GetCatalog())
            {
                bReturning = Step == EStep::Return;
                Camp.OpenExpeditionTable(*It->GetCatalog());
                Step = EStep::Mission;
            }
            break;
        case EStep::Mission:
            if (Parent)
            {
                auto* Entry = Parent->GetWeaponEntryButton(false);
                if (!Check(Entry && Entry->GetIsEnabled(), TEXT("The mission list exposes its weapon editor entry")))
                {
                    return;
                }
                Entry->SetUserFocus(&Camp);
                SendUIKey(EKeys::Enter);
                Step = EStep::Edit;
            }
            break;
        case EStep::Edit:
            if (!Editor)
            {
                Check(false, TEXT("Mission entry presents the weapon widget"));
                return;
            }
            if (!bWrite || bReturning)
            {
                if (!Check(IsSavedSet(Camp.GetWeaponDraft()), TEXT("The disk or return view restores the exact slot order")))
                {
                    return;
                }
                if (bReturning)
                {
                    Finish(true, bWrite ? TEXT("author and victory") : TEXT("restart and defeat"));
                    return;
                }
                if (!Click(Editor->GetCancelButton()))
                {
                    return;
                }
            }
            else
            {
                if (!Check(Camp.GetWeaponDraft().Slot1 == TEXT("Pistol") && Camp.GetWeaponDraft().Slot2.IsNone(),
                    TEXT("Fresh playtest starts with pistol alone")) || !AuthorLoadout(Camp, *Editor))
                {
                    return;
                }
            }
            Step = EStep::Confirmed;
            break;
        case EStep::Confirmed:
            if (!Check(Parent && Camp.GetCurrentPage() == EGirlSaviorCampPage::Missions
                && Parent->GetWeaponEntryButton(false)->HasUserFocus(&Camp), TEXT("The editor returns focus to its mission entry")))
            {
                return;
            }
            Camp.OpenPreparationPage();
            Camp.ToggleAbility(TEXT("MechanismOverdrive"));
            Camp.ToggleAbility(TEXT("TravelersBarrier"));
            Step = EStep::Preparation;
            break;
        case EStep::Preparation:
            if (Parent && Click(Parent->GetWeaponEntryButton(true)))
            {
                Step = EStep::Cancel;
            }
            break;
        case EStep::Cancel:
            if (!Check(Editor && IsSavedSet(Camp.GetWeaponDraft()), TEXT("Preparation opens the confirmed weapon draft"))
                || !Click(Editor->GetCardButton(TEXT("Shotgun"))) || !Click(Editor->GetSlotButton(1))
                || !Check(Camp.GetWeaponDraft().Slot2 == TEXT("Shotgun")
                    && (bFull ? Camp.GetWeaponDraft().Slot1 == TEXT("Pistol") : Camp.GetWeaponDraft().Slot3 == TEXT("AssaultRifle")),
                    TEXT("Moving an existing weapon to an occupied slot swaps the two positions")))
            {
                return;
            }
            SendUIKey(EKeys::Escape);
            Step = EStep::Launch;
            break;
        case EStep::Launch:
            if (Check(Parent && Camp.GetCurrentPage() == EGirlSaviorCampPage::Preparation
                && Camp.GetDraft().GetAbilityIds().Num() == 2 && Parent->GetWeaponEntryButton(true)->HasUserFocus(&Camp),
                TEXT("Cancel returns to preparation with support and focus preserved")) && Click(Parent->GetLaunchButton()))
            {
                Step = EStep::Route;
            }
            break;
        default:
            break;
        }
    }

    bool AuthorLoadout(const AGirlSaviorCampPlayerController& Camp, UGirlSaviorWeaponLoadoutWidget& Editor)
    {
        const bool bEdited = bFull
            ? Click(Editor.GetCardButton(TEXT("Shotgun"))) && Click(Editor.GetSlotButton(0))
                && Click(Editor.GetCardButton(TEXT("Pistol"))) && Click(Editor.GetSlotButton(1))
                && Click(Editor.GetCardButton(TEXT("AssaultRifle"))) && Click(Editor.GetSlotButton(2))
            : Click(Editor.GetCardButton(TEXT("AssaultRifle"))) && Click(Editor.GetSlotButton(1))
                && Click(Editor.GetCardButton(TEXT("Shotgun"))) && Click(Editor.GetSlotButton(2))
                && Click(Editor.GetRemoveButton(0));
        return bEdited && Check(IsSavedSet(Camp.GetWeaponDraft()), TEXT("The draft preserves the authored slot order"))
            && Click(Editor.GetConfirmButton());
    }

    void TickRoute(UWorld& World, APlayerController& PC)
    {
        TActorIterator<AGirlSaviorCombatEncounter> It(&World);
        auto* Encounter = It ? *It : nullptr;
        if (!Encounter)
        {
            return;
        }
        auto* Hero = Encounter->GetHero();
        if (bFull && Step != EStep::Fight && Step != EStep::Outcome && Step != EStep::Return)
        {
            if (Hero)
            {
                TickFullRoute(World, PC, *Encounter, *Hero);
            }
            return;
        }
        if (Step == EStep::Route && Hero)
        {
            const auto* Controller = Cast<AGirlSaviorPlayerController>(&PC);
            if (!Controller || !Controller->CanUseGameplayInput())
            {
                Focus(PC);
                return;
            }
            auto* Fire = Hero->GetFireComponent();
            if (!Check(Encounter->IsCampOrigin() && Fire->GetActiveWeaponSlot() == 1
                && Fire->GetWeaponSlotId(0).IsNone() && Fire->GetWeaponSlotId(1) == TEXT("AssaultRifle")
                && Fire->GetWeaponSlotId(2) == TEXT("Shotgun"), TEXT("The claimed run equips the first occupied slot in its snapshot"))
                || !Check(Fire->TrySelectWeaponSlot(2), TEXT("The shotgun can be selected in the safe pocket")))
            {
                return;
            }
            PreviousHero = Hero;
            PC.UnPossess();
            PC.Possess(Hero);
            const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC.GetLocalPlayer());
            const auto* Mapping = LoadObject<UInputMappingContext>(nullptr,
                TEXT("/Game/Input/IMC_ExpeditionWeapons.IMC_ExpeditionWeapons"));
            if (!Check(Subsystem && Mapping && Subsystem->HasMappingContext(Mapping),
                TEXT("Same-frame possession restores the existing weapon input mapping")))
            {
                return;
            }
            PC.UnPossess();
            Step = EStep::Unpossessed;
        }
        else if (Step == EStep::Unpossessed && !Hero && PreviousHero.IsValid())
        {
            PC.Possess(PreviousHero.Get());
            Step = EStep::Rebound;
        }
        else if (Step == EStep::Rebound && Hero == PreviousHero.Get())
        {
            if (!Check(Hero->GetFireComponent()->GetActiveWeaponSlot() == 2, TEXT("Same-pawn rebind preserves the run selection")))
            {
                return;
            }
            Hero->SetActorLocation(FVector(-650, 0, 90), false, nullptr, ETeleportType::TeleportPhysics);
            Step = EStep::Aim;
        }
        else if (Step == EStep::Aim && Hero && Encounter->GetRunState() == EGirlSaviorRunState::Active)
        {
            Hero->SetActorLocation(FVector(100, -430, 90), false, nullptr, ETeleportType::TeleportPhysics);
            Step = EStep::Shoot;
        }
        else if (Step == EStep::Shoot && Hero)
        {
            const auto* Controller = Cast<AGirlSaviorPlayerController>(&PC);
            FVector2D Screen;
            if (!Controller || !Controller->CanUseGameplayInput()
                || !PC.ProjectWorldLocationToScreen(FVector(280, -430, 90), Screen))
            {
                Focus(PC);
                return;
            }
            PC.SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
            Hero->GetFireComponent()->SetFireButtonDown(true);
            Step = EStep::Replace;
        }
        else if (Step == EStep::Replace && Hero
            && Hero->GetFireComponent()->GetNextShotGameTime() > World.GetTimeSeconds())
        {
            SavedDeadline = Hero->GetFireComponent()->GetNextShotGameTime();
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Replacement = World.SpawnActor<AGirlSaviorCharacter>(Hero->GetClass(), Hero->GetActorLocation(),
                Hero->GetActorRotation(), Spawn);
            if (!Check(IsValid(Replacement), TEXT("A replacement combat pawn can be spawned")))
            {
                return;
            }
            Hero->SetActorEnableCollision(false);
            PC.Possess(Replacement);
            Step = EStep::Replaced;
        }
        else if (Step == EStep::Replaced && Hero && Hero != PreviousHero.Get())
        {
            const auto* Fire = Hero->GetFireComponent();
            if (!Check(Fire->GetActiveWeaponSlot() == 2 && Fire->GetWeaponSlotId(0).IsNone()
                && Fire->GetWeaponSlotId(1) == TEXT("AssaultRifle") && Fire->GetWeaponSlotId(2) == TEXT("Shotgun")
                && Fire->GetNextShotGameTime() == SavedDeadline,
                TEXT("Replacement uses the frozen loadout, last active slot and unpaid deadline")))
            {
                return;
            }
            Hero->SetActorLocation(FVector(-650, 0, 90), false, nullptr, ETeleportType::TeleportPhysics);
            Step = EStep::Fight;
        }
        else if (Step == EStep::Fight && Hero && Encounter->GetRunState() == EGirlSaviorRunState::Active)
        {
            if (bWrite)
            {
                Hero->SetActorLocation(Encounter->ExitVolume->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            }
            else
            {
                FGirlSaviorDamageSpec Damage;
                Damage.Delivery = EGirlSaviorDamageDelivery::Area;
                Damage.SourceFaction = EGirlSaviorFaction::Enemy;
                Damage.Physical = 10000.f;
                Hero->GetDamageReceiver()->ResolveDamage(Damage);
            }
            Step = EStep::Outcome;
        }
        else if (Step == EStep::Outcome && Encounter->IsOutcomeVisible())
        {
            auto* HUD = Cast<AGirlSaviorCombatHUD>(PC.GetHUD());
            auto* Outcome = HUD ? HUD->GetOutcomeWidget() : nullptr;
            const auto Expected = bWrite ? EGirlSaviorRunState::Succeeded : EGirlSaviorRunState::Defeated;
            if (Check(Encounter->GetRunState() == Expected && Outcome && Outcome->IsOutcomePanelVisible(),
                TEXT("The route displays the expected outcome")) && Click(Outcome->GetRetryButton()))
            {
                Step = EStep::Return;
            }
        }
    }

    static bool SendViewportKey(APlayerController& PC, const FKey& Key, EInputEvent Event)
    {
        const auto* LocalPlayer = PC.GetLocalPlayer();
        auto* Client = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
        if (!Client || !Client->Viewport)
        {
            return false;
        }
        // This probe runs with the one local keyboard/mouse player.
        const FInputDeviceId PrimaryDevice = FInputDeviceId::CreateFromInternalId(0);
        if (!GEngine || GEngine->GetLocalPlayerFromInputDevice(Client, PrimaryDevice) != LocalPlayer)
        {
            return false;
        }
        Client->InputKey(FInputKeyEventArgs(Client->Viewport, PrimaryDevice, Key, Event, FPlatformTime::Cycles64()));
        return true;
    }

    bool CheckHUD(APlayerController& PC, const UGirlSaviorFireComponent& Fire, int32 ExpectedSlot)
    {
        const auto* HUD = Cast<AGirlSaviorCombatHUD>(PC.GetHUD());
        const auto* Vitals = HUD ? HUD->GetVitalsWidget() : nullptr;
        if (!Vitals)
        {
            return Check(false, TEXT("The combat HUD is present"));
        }
        const auto& View = Vitals->GetDisplayedWeaponView();
        if (!Check(View.ActiveIndex == ExpectedSlot && Fire.GetActiveWeaponSlot() == ExpectedSlot
            && View.Slots.Num() == FGirlSaviorWeaponLoadout::SlotCount, TEXT("Active HUD slot follows the equipped weapon")))
        {
            return false;
        }
        const FName ExpectedIds[] = {TEXT("Shotgun"), TEXT("Pistol"), TEXT("AssaultRifle")};
        for (int32 Index = 0; Index < View.Slots.Num(); ++Index)
        {
            const auto& Slot = View.Slots[Index];
            if (!Check(Slot.WeaponId == ExpectedIds[Index] && Slot.Icon && !Slot.DisplayName.IsEmpty(),
                *FString::Printf(TEXT("HUD slot %d carries the saved %s ID, name and cooked icon"), Index + 1, *ExpectedIds[Index].ToString())))
            {
                return false;
            }
        }
        return true;
    }

    void ObserveShot(const FGirlSaviorWeaponShotCue& Cue)
    {
        ++ShotCount;
        LastShotId = Cue.WeaponId;
    }

    void TickFullRoute(UWorld& World, APlayerController& PC, AGirlSaviorCombatEncounter& Encounter, AGirlSaviorCharacter& Hero)
    {
        auto& Fire = *Hero.GetFireComponent();
        const auto* Controller = Cast<AGirlSaviorPlayerController>(&PC);
        if (!Controller || !Controller->CanUseGameplayInput())
        {
            Focus(PC);
            return;
        }
        const FKey SlotKeys[] = {EKeys::One, EKeys::Two, EKeys::Three};
        switch (Step)
        {
        case EStep::Route:
            if (!Check(Encounter.IsCampOrigin(), TEXT("The cooked route claims a Camp launch")) || !CheckHUD(PC, Fire, 0))
            {
                return;
            }
            ObservedFire = &Fire;
            Fire.OnWeaponShot.AddRaw(this, &FWeaponLoadoutProbe::ObserveShot);
            // Choose a different slot before testing keys 1–3 in order.
            if (Check(SendViewportKey(PC, EKeys::Three, IE_Pressed), TEXT("The viewport accepts slot input in the safe pocket")))
            {
                Step = EStep::FullWarmup;
            }
            break;
        case EStep::FullWarmup:
            SendViewportKey(PC, EKeys::Three, IE_Released);
            if (CheckHUD(PC, Fire, 2))
            {
                Hero.SetActorLocation(FVector(-650, 0, 90), false, nullptr, ETeleportType::TeleportPhysics);
                Step = EStep::FullSelect;
            }
            break;
        case EStep::FullSelect:
            if (Encounter.GetRunState() == EGirlSaviorRunState::Active)
            {
                Hero.SetActorLocation(FVector(100, -430, 90), false, nullptr, ETeleportType::TeleportPhysics);
                if (Check(SendViewportKey(PC, SlotKeys[FullSlot], IE_Pressed), TEXT("Send numbered key down through the game viewport")))
                {
                    Step = EStep::FullSelected;
                }
            }
            break;
        case EStep::FullSelected:
            SendViewportKey(PC, SlotKeys[FullSlot], IE_Released);
            if (CheckHUD(PC, Fire, FullSlot))
            {
                Step = EStep::FullFire;
            }
            break;
        case EStep::FullFire:
        {
            FVector2D Screen;
            if (World.GetTimeSeconds() >= Fire.GetNextShotGameTime()
                && PC.ProjectWorldLocationToScreen(FVector(280, -430, 90), Screen))
            {
                PC.SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
                if (Check(SendViewportKey(PC, EKeys::LeftMouseButton, IE_Pressed), TEXT("Send LMB down through the game viewport")))
                {
                    Step = EStep::FullObserve;
                }
            }
            break;
        }
        case EStep::FullObserve:
            SendViewportKey(PC, EKeys::LeftMouseButton, IE_Released);
            if (!Check(ShotCount == FullSlot + 1 && LastShotId == Fire.GetWeaponSlotId(FullSlot),
                *FString::Printf(TEXT("Slot %d fires one committed %s discharge"), FullSlot + 1, *Fire.GetWeaponSlotId(FullSlot).ToString())))
            {
                return;
            }
            ++FullSlot;
            Step = FullSlot < UE_ARRAY_COUNT(SlotKeys) ? EStep::FullSelect : EStep::Fight;
            break;
        default:
            break;
        }
    }

    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<AGirlSaviorCharacter> PreviousHero;
    TWeakObjectPtr<UGirlSaviorFireComponent> ObservedFire;
    FDelegateHandle TickHandle;
    EStep Step = EStep::Camp;
    bool bWrite;
    bool bFull;
    bool bPackaged;
    bool bReturning = false;
    double Deadline;
    double SavedDeadline = 0.0;
    FName LastShotId;
    int32 ShotCount = 0;
    int32 FullSlot = 0;
};

TUniquePtr<FWeaponLoadoutProbe> Probe;
FTSTicker::FDelegateHandle PendingWeaponLoadoutProbe;

bool IsWeaponProbeCamp(const UWorld* World)
{
    return World && World->IsGameWorld() && World->GetGameInstance() && World->GetMapName().EndsWith(TEXT("CampPrototype"));
}

void StartWeaponLoadoutProbe(UWorld* World, bool bWrite, bool bFull)
{
    FTSTicker::RemoveTicker(PendingWeaponLoadoutProbe);
    PendingWeaponLoadoutProbe.Reset();
    Probe.Reset();
    if (IsWeaponProbeCamp(World))
    {
        Probe = MakeUnique<FWeaponLoadoutProbe>(*World, bWrite, bFull);
        return;
    }
    const double Deadline = FPlatformTime::Seconds() + 25.0;
    PendingWeaponLoadoutProbe = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline, bWrite, bFull](float)
    {
        if (GEngine)
        {
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                if (UWorld* Candidate = Context.World(); IsWeaponProbeCamp(Candidate))
                {
                    Probe = MakeUnique<FWeaponLoadoutProbe>(*Candidate, bWrite, bFull);
                    return false;
                }
            }
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            UE_LOG(LogWeaponLoadoutPIE, Error, TEXT("%s RESULT FAIL Timed out waiting for CampPrototype"), bFull ? TEXT("W4") : TEXT("W3"));
            if (!GIsEditor)
            {
                FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("WeaponLoadoutProbeStartup"));
            }
            return false;
        }
        return true;
    }));
}

FAutoConsoleCommandWithWorldAndArgs RunWeaponLoadoutProbe(TEXT("GirlSavior.TestWeaponLoadout"),
    TEXT("Run an isolated Camp loadout round trip. Use Write, Read, FullWrite, or FullRead."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        FString NamespaceValue;
        FGuid Namespace;
        if (FParse::Value(FCommandLine::Get(), TEXT("GirlSaviorWeaponTestProfile="), NamespaceValue) &&
            FGuid::ParseExact(NamespaceValue, EGuidFormats::Digits, Namespace) && Namespace.IsValid() &&
            Args.Num() == 1 && (Args[0] == TEXT("Write") || Args[0] == TEXT("Read")
                || Args[0] == TEXT("FullWrite") || Args[0] == TEXT("FullRead")))
        {
            StartWeaponLoadoutProbe(World, Args[0].EndsWith(TEXT("Write")), Args[0].StartsWith(TEXT("Full")));
            return;
        }
        UE_LOG(LogWeaponLoadoutPIE, Error, TEXT("Weapon loadout probe requires a valid isolated GUID and probe mode."));
        if (!GIsEditor)
        {
            FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("WeaponLoadoutProbeArguments"));
        }
    }));
}

#endif
