#if !UE_BUILD_SHIPPING

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDevice.h"

// Fill empty run slots from the catalog without changing the confirmed Camp profile.
bool AGirlSaviorCombatEncounter::GiveAllWeaponsForCheat(FText& OutError)
{
    auto* Pawn = Hero.Get();
    auto* Fire = Pawn ? Pawn->GetFireComponent() : nullptr;
    if (!Fire || !Fire->HasBoundWeaponInput() || !Pawn->GetDamageReceiver()->IsAlive())
    {
        OutError = NSLOCTEXT("GirlSaviorWeaponCheats", "NoHero", "This command requires a living hero in a combat expedition.");
        return false;
    }

    const UDataTable* Table = Fire->DefaultWeaponTable;
    if (!Table || Table->GetRowStruct() != FGirlSaviorWeaponRow::StaticStruct() || Table->GetRowMap().IsEmpty())
    {
        OutError = NSLOCTEXT("GirlSaviorWeaponCheats", "NoTable", "The weapon catalog is unavailable.");
        return false;
    }

    TArray<FGirlSaviorResolvedWeaponSlot> Missing;
    for (const FName Id : Table->GetRowNames())
    {
        const bool bEquipped = RunWeaponSlots.ContainsByPredicate([Id](const auto& Slot) { return Slot.WeaponId == Id; });
        if (!bEquipped)
        {
            const auto* Row = Table->FindRow<FGirlSaviorWeaponRow>(Id, TEXT("sv_giveallweapon"), false);
            if (!Row)
            {
                OutError = NSLOCTEXT("GirlSaviorWeaponCheats", "BadRow", "A catalog weapon could not be loaded.");
                return false;
            }
            Missing.Add({Id, *Row});
        }
    }
    if (Missing.IsEmpty())
    {
        return true;
    }
    Missing.Sort([](const auto& A, const auto& B)
    {
        return A.Row.DisplayOrder == B.Row.DisplayOrder
            ? A.WeaponId.LexicalLess(B.WeaponId) : A.Row.DisplayOrder < B.Row.DisplayOrder;
    });

    auto UpdatedSlots = RunWeaponSlots;
    int32 NextMissing = 0;
    for (auto& Slot : UpdatedSlots)
    {
        if (Slot.WeaponId.IsNone() && Missing.IsValidIndex(NextMissing))
        {
            Slot = MoveTemp(Missing[NextMissing++]);
        }
    }
    if (NextMissing != Missing.Num())
    {
        OutError = NSLOCTEXT("GirlSaviorWeaponCheats", "NotEnoughSlots", "The expedition has too few slots for all catalog weapons.");
        return false;
    }
    if (!Fire->ConfigureExpeditionWeapons(UpdatedSlots, Fire->GetActiveWeaponSlot(), OutError, true))
    {
        return false;
    }
    RunWeaponSlots = MoveTemp(UpdatedSlots);
    return true;
}

namespace
{
// Restrict the console command to the locally played combat world.
void GiveAllWeapons(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
    if (!Args.IsEmpty())
    {
        Output.Log(TEXT("Usage: sv_giveallweapon"));
        return;
    }
    if (World && World->IsGameWorld() && World->GetNetMode() == NM_Standalone)
    {
        const auto* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
        for (TActorIterator<AGirlSaviorCombatEncounter> It(World); Pawn && It; ++It)
        {
            if (It->GetHero() == Pawn)
            {
                FText Error;
                if (It->GiveAllWeaponsForCheat(Error))
                {
                    Output.Log(TEXT("All weapons are equipped for this expedition. Use 1, 2, or 3 to switch."));
                }
                else
                {
                    Output.Log(Error.ToString());
                }
                return;
            }
        }
    }
    Output.Log(TEXT("sv_giveallweapon is available in a local combat expedition."));
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice GiveAllWeaponsCommand(
    TEXT("sv_giveallweapon"),
    TEXT("Fill the local hero's empty expedition slots with all weapons from the catalog."),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&GiveAllWeapons), ECVF_Cheat);
}

#endif
