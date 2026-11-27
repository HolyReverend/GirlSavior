#include "Weapons/GirlSaviorWeaponDefinition.h"

#include "Combat/GirlSaviorTestProjectile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "Weapons/GirlSaviorFirearm.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorWeaponDefinition, Log, All);

#define LOCTEXT_NAMESPACE "GirlSaviorWeapons"

namespace
{
bool IsPositiveFinite(float Value)
{
    return FMath::IsFinite(Value) && Value > 0.f;
}

bool IsNonnegativeFinite(float Value)
{
    return FMath::IsFinite(Value) && Value >= 0.f;
}

bool ReportLoadoutFailure(const UDataTable& Table, FName WeaponId, const FText& Error, FText& OutError)
{
    OutError = Error;
    UE_LOG(LogGirlSaviorWeaponDefinition, Warning, TEXT("%s [%s]: %s"),
        *Table.GetPathName(), *WeaponId.ToString(), *Error.ToString());
    return false;
}
}

bool FGirlSaviorWeaponRow::Validate(FText& OutError) const
{
    OutError = FText::GetEmpty();
    const auto Require = [&OutError](bool bValid, const TCHAR* Field)
    {
        if (!bValid)
        {
            OutError = FText::Format(LOCTEXT("InvalidWeaponField", "Проверьте параметр оружия «{0}»."), FText::FromString(Field));
        }
        return bValid;
    };

    // Short-circuit to expose the first invalid field and avoid dependent calculations with bad values.
    return Require(!DisplayName.ToString().TrimStartAndEnd().IsEmpty(), TEXT("DisplayName"))
        && Require(!Description.ToString().TrimStartAndEnd().IsEmpty(), TEXT("Description"))
        && Require(!DamageCause.ToString().TrimStartAndEnd().IsEmpty(), TEXT("DamageCause"))
        && Require(IsValid(Icon.Get()), TEXT("Icon"))
        && Require(FirearmClass && !FirearmClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists), TEXT("FirearmClass"))
        && Require(ProjectileClass && !ProjectileClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists), TEXT("ProjectileClass"))
        && Require(IsPositiveFinite(DamagePerProjectile), TEXT("DamagePerProjectile"))
        && Require(IsPositiveFinite(ShotsPerSecond) && IsPositiveFinite(1.f / ShotsPerSecond), TEXT("ShotsPerSecond"))
        && Require(IsPositiveFinite(ProjectileRadius), TEXT("ProjectileRadius"))
        && Require(IsPositiveFinite(ProjectileSpeed), TEXT("ProjectileSpeed"))
        && Require(IsPositiveFinite(MaxDistance) && IsPositiveFinite(4.f * (MaxDistance / ProjectileSpeed) + 0.25f), TEXT("MaxDistance"))
        && Require(IsNonnegativeFinite(FullDamageDistance) && FullDamageDistance <= MaxDistance, TEXT("FullDamageDistance"))
        && Require(ProjectileCount >= 1 && ProjectileCount <= 5, TEXT("ProjectileCount"))
        && Require(IsNonnegativeFinite(PelletStepDegrees) && (ProjectileCount - 1) * PelletStepDegrees <= 90.f, TEXT("PelletStepDegrees"))
        && Require(TightShotCount >= 0, TEXT("TightShotCount"))
        && Require(IsNonnegativeFinite(TightSpreadDegrees), TEXT("TightSpreadDegrees"))
        && Require(IsNonnegativeFinite(SustainedSpreadDegrees), TEXT("SustainedSpreadDegrees"))
        && Require(IsNonnegativeFinite(SpreadResetSeconds), TEXT("SpreadResetSeconds"))
        && Require(IsValid(Presentation.HeldMesh.Get()), TEXT("Presentation.HeldMesh"))
        && Require(!Presentation.HeldMeshScale.ContainsNaN()
            && Presentation.HeldMeshScale.GetMin() > 0.0, TEXT("Presentation.HeldMeshScale"))
        && Require(!Presentation.MuzzleOffset.ContainsNaN(), TEXT("Presentation.MuzzleOffset"))
        && Require(IsValid(Presentation.ShotSound.Get()), TEXT("Presentation.ShotSound"))
        && Require(IsPositiveFinite(Presentation.FlashScale), TEXT("Presentation.FlashScale"))
        && Require(IsPositiveFinite(Presentation.FlashSeconds), TEXT("Presentation.FlashSeconds"))
        && Require(IsNonnegativeFinite(Presentation.KickDistance), TEXT("Presentation.KickDistance"))
        && Require(IsPositiveFinite(Presentation.KickRecoverSeconds), TEXT("Presentation.KickRecoverSeconds"))
        && Require(IsPositiveFinite(Presentation.TrailLength), TEXT("Presentation.TrailLength"))
        && Require(IsPositiveFinite(Presentation.TrailWidth), TEXT("Presentation.TrailWidth"))
        && Require(IsPositiveFinite(Presentation.TrailFadeSeconds), TEXT("Presentation.TrailFadeSeconds"))
        && Require(FMath::IsFinite(Presentation.TrailColor.R) && FMath::IsFinite(Presentation.TrailColor.G)
            && FMath::IsFinite(Presentation.TrailColor.B) && FMath::IsFinite(Presentation.TrailColor.A), TEXT("Presentation.TrailColor"))
        && FirearmClass->GetDefaultObject<UGirlSaviorFirearm>()->ValidateBehavior(*this, OutError);
}

#if WITH_EDITOR
EDataValidationResult FGirlSaviorWeaponRow::IsDataValid(FDataValidationContext& Context) const
{
    FText Error;
    if (Validate(Error))
    {
        return EDataValidationResult::Valid;
    }

    Context.AddError(Error);
    return EDataValidationResult::Invalid;
}
#endif

bool ResolveWeaponLoadout(const UDataTable& Table, const FGirlSaviorWeaponLoadout& Loadout,
    TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError)
{
    if (Table.GetRowStruct() != FGirlSaviorWeaponRow::StaticStruct())
    {
        return ReportLoadoutFailure(Table, NAME_None,
            LOCTEXT("WrongRowType", "Таблица оружия использует неверный тип строки."), OutError);
    }

    TArray<FGirlSaviorResolvedWeaponSlot> Resolved;
    Resolved.SetNum(FGirlSaviorWeaponLoadout::SlotCount);
    TSet<FName> UsedIds;
    const FName Ids[] = { Loadout.Slot1, Loadout.Slot2, Loadout.Slot3 };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
    {
        const FName WeaponId = Ids[Index];
        if (WeaponId.IsNone())
        {
            continue;
        }
        const FText WeaponLabel = FText::FromName(WeaponId);
        if (UsedIds.Contains(WeaponId))
        {
            return ReportLoadoutFailure(Table, WeaponId,
                FText::Format(LOCTEXT("DuplicateWeapon", "Оружие «{0}» выбрано дважды."), WeaponLabel), OutError);
        }

        const auto* Row = Table.FindRow<FGirlSaviorWeaponRow>(WeaponId, TEXT("ResolveWeaponLoadout"), false);
        if (!Row)
        {
            return ReportLoadoutFailure(Table, WeaponId,
                FText::Format(LOCTEXT("MissingWeapon", "Оружие «{0}» отсутствует в таблице."), WeaponLabel), OutError);
        }
        FText RowError;
        if (!Row->Validate(RowError))
        {
            return ReportLoadoutFailure(Table, WeaponId,
                FText::Format(LOCTEXT("InvalidWeapon", "Строка оружия «{0}». {1}"), WeaponLabel, RowError), OutError);
        }

        UsedIds.Add(WeaponId);
        Resolved[Index].WeaponId = WeaponId;
        Resolved[Index].Row = *Row;
    }

    if (UsedIds.IsEmpty())
    {
        return ReportLoadoutFailure(Table, NAME_None, LOCTEXT("EmptyLoadout", "Выберите хотя бы одно оружие."), OutError);
    }

    OutSlots = MoveTemp(Resolved);
    OutError = FText::GetEmpty();
    return true;
}

#undef LOCTEXT_NAMESPACE
