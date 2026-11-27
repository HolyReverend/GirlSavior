#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Engine/World.h"
#include "Weapons/GirlSaviorFirearm.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorFire, Log, All);

UGirlSaviorFireComponent::UGirlSaviorFireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bRunOnAnyThread = false;
}

void UGirlSaviorFireComponent::BeginPlay()
{
	Super::BeginPlay();
	// Every launch consumes the final aim sample from this frame.
	if (auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner()))
		AddTickPrerequisiteComponent(Hero->GetAimComponent());
}

void UGirlSaviorFireComponent::SetFireButtonDown(bool bDown)
{
	if (bDown && GetWorld()) Schedule.Press(GetWorld()->GetTimeSeconds());
	else Schedule.Release();
}

void UGirlSaviorFireComponent::CancelFireSeries()
{
	Schedule.Cancel();
}

// Configure the default case through the same slot path as the combat fixture.
bool UGirlSaviorFireComponent::ConfigurePistol(const UDataTable& Table, FText& OutError)
{
	TArray<FGirlSaviorResolvedWeaponSlot> Slots;
	return ResolveWeaponLoadout(Table, FGirlSaviorWeaponLoadout{}, Slots, OutError)
		&& ConfigureExpeditionWeapons(Slots, INDEX_NONE, OutError);
}

// Validate the complete snapshot before replacing live instances and input state.
bool UGirlSaviorFireComponent::ConfigureExpeditionWeapons(TConstArrayView<FGirlSaviorResolvedWeaponSlot> Slots,
	int32 PreferredIndex, FText& OutError, bool bKeepEquippedInstances)
{
	const auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	const auto* Capsule = Hero ? Hero->GetCapsuleComponent() : nullptr;
	if (!Capsule || Slots.Num() != FGirlSaviorWeaponLoadout::SlotCount)
	{
		OutError = NSLOCTEXT("GirlSaviorWeapons", "InvalidSlotCount", "Проверьте три слота оружия и капсулу героя.");
		return false;
	}

	int32 FirstSlot = INDEX_NONE;
	TSet<FName> WeaponIds;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const auto& Slot = Slots[Index];
		if (Slot.WeaponId.IsNone())
		{
			continue;
		}
		if (WeaponIds.Contains(Slot.WeaponId) || Slot.Row.ProjectileRadius >= Capsule->GetScaledCapsuleRadius())
		{
			OutError = NSLOCTEXT("GirlSaviorWeapons", "InvalidEquippedWeapon", "Проверьте набор и размер снаряда оружия.");
			return false;
		}
		if (!Slot.Row.Validate(OutError))
		{
			return false;
		}
		if (!FMath::IsFinite(Slot.Row.ShotsPerSecond * SupportRateMultiplier))
		{
			OutError = NSLOCTEXT("GirlSaviorWeapons", "InvalidEffectiveRate", "Итоговый темп стрельбы недопустим.");
			return false;
		}
		WeaponIds.Add(Slot.WeaponId);
		if (FirstSlot == INDEX_NONE)
		{
			FirstSlot = Index;
		}
	}
	if (FirstSlot == INDEX_NONE)
	{
		OutError = NSLOCTEXT("GirlSaviorWeapons", "EmptyEquipment", "Выберите хотя бы одно оружие.");
		return false;
	}

	TArray<TObjectPtr<UGirlSaviorFirearm>> NewFirearms;
	NewFirearms.SetNum(Slots.Num());
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const auto& Slot = Slots[Index];
		if (!Slot.WeaponId.IsNone())
		{
			if (bKeepEquippedInstances && GetWeaponSlotId(Index) == Slot.WeaponId)
			{
				NewFirearms[Index] = Firearms[Index];
				continue;
			}
			auto* Firearm = NewObject<UGirlSaviorFirearm>(this, Slot.Row.FirearmClass);
			Firearm->Initialize(Slot.WeaponId, Slot.Row);
			NewFirearms[Index] = Firearm;
		}
	}

	ActiveSlotIndex = NewFirearms.IsValidIndex(PreferredIndex) && NewFirearms[PreferredIndex] ? PreferredIndex : FirstSlot;
	Firearms = MoveTemp(NewFirearms);
	CancelFireSeries();
	OutError = FText::GetEmpty();
	OnWeaponChanged.Broadcast(ActiveSlotIndex);
	return true;
}

UGirlSaviorFirearm* UGirlSaviorFireComponent::GetActiveFirearm() const
{
	return Firearms.IsValidIndex(ActiveSlotIndex) ? Firearms[ActiveSlotIndex].Get() : nullptr;
}

FName UGirlSaviorFireComponent::GetWeaponSlotId(int32 SlotIndex) const
{
	return Firearms.IsValidIndex(SlotIndex) && Firearms[SlotIndex] ? Firearms[SlotIndex]->GetWeaponId() : NAME_None;
}

// A presentation snapshot keeps the position of every slot and exposes no mutable firearm.
FGirlSaviorWeaponView UGirlSaviorFireComponent::GetWeaponView() const
{
	FGirlSaviorWeaponView View;
	View.Slots.SetNum(FGirlSaviorWeaponLoadout::SlotCount);
	View.ActiveIndex = ActiveSlotIndex;
	for (int32 Index = 0; Index < FMath::Min(Firearms.Num(), View.Slots.Num()); ++Index)
	{
		if (const auto* Firearm = Firearms[Index].Get())
		{
			FGirlSaviorWeaponSlotView& Slot = View.Slots[Index];
			const auto& Definition = Firearm->GetDefinition();
			Slot.WeaponId = Firearm->GetWeaponId();
			Slot.DisplayName = Definition.DisplayName;
			Slot.Icon = Definition.Icon;
		}
	}
	return View;
}

const FGirlSaviorWeaponRow* UGirlSaviorFireComponent::GetConfiguredWeaponDefinition() const
{
	const auto* Firearm = GetActiveFirearm();
	return Firearm ? &Firearm->GetDefinition() : nullptr;
}

bool UGirlSaviorFireComponent::HasBoundWeaponInput() const
{
	const auto* Encounter = CombatEncounter.Get();
	return Encounter && Encounter->GetHero() == GetOwner()
		&& (Encounter->GetRunState() == EGirlSaviorRunState::Waiting || Encounter->GetRunState() == EGirlSaviorRunState::Active);
}

// A successful switch clears the press, including a ready tap in the current frame.
bool UGirlSaviorFireComponent::TrySelectWeaponSlot(int32 SlotIndex)
{
	const auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	const auto* Controller = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
	if (SlotIndex == ActiveSlotIndex || GetWeaponSlotId(SlotIndex).IsNone() || !HasBoundWeaponInput() || !Controller
		|| !Hero->GetDamageReceiver()->IsAlive() || !Controller->CanUseGameplayInput())
	{
		return false;
	}

	CancelFireSeries();
	ActiveSlotIndex = SlotIndex;
	OnWeaponChanged.Broadcast(ActiveSlotIndex);
	return true;
}

// Read the combat pawn's authored defaults for direct-entry maps.
bool UGirlSaviorFireComponent::ResolveDefaultWeapons(bool bUseTestLoadout,
	TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError) const
{
	if (!bUsesExpeditionWeapons || !IsValid(DefaultWeaponTable))
	{
		OutError = NSLOCTEXT("GirlSaviorWeapons", "MissingDefaultTable", "Таблица оружия боевого персонажа недоступна.");
		return false;
	}
	FGirlSaviorWeaponLoadout Loadout;
	if (bUseTestLoadout)
	{
		Loadout.Slot2 = TEXT("Shotgun");
		Loadout.Slot3 = TEXT("AssaultRifle");
	}
	return ResolveWeaponLoadout(*DefaultWeaponTable, Loadout, OutSlots, OutError);
}

// Restore same-pawn instances only while the same encounter owns the run.
bool UGirlSaviorFireComponent::SetCombatEncounter(AGirlSaviorCombatEncounter* InEncounter,
	TConstArrayView<FGirlSaviorResolvedWeaponSlot> Slots, int32 PreferredIndex, double Deadline)
{
	if (!InEncounter)
	{
		SuspendCombatEncounter();
		PreparedWeaponEncounter.Reset();
		Firearms.Reset();
		ActiveSlotIndex = INDEX_NONE;
		Schedule = {};
		return true;
	}
	FText Error;
	if (!bUsesExpeditionWeapons || (PreparedWeaponEncounter.Get() != InEncounter
		&& !ConfigureExpeditionWeapons(Slots, PreferredIndex, Error)))
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: could not bind expedition weapons: %s"),
			*GetPathName(), *Error.ToString());
		return false;
	}
	PreparedWeaponEncounter = InEncounter;
	CombatEncounter = InEncounter;
	Schedule.PreserveDeadline(Deadline);
	CancelFireSeries();
	return true;
}

// Clear the input gate and held press during a transient possession gap.
void UGirlSaviorFireComponent::SuspendCombatEncounter()
{
	CombatEncounter.Reset();
	SupportRateMultiplier = 1.f;
	CancelFireSeries();
}

// Change future shot intervals without resetting the existing scheduled deadline.
bool UGirlSaviorFireComponent::SetSupportRateMultiplier(float Multiplier)
{
	if (!FMath::IsFinite(Multiplier) || Multiplier < 1.f || (Multiplier != 1.f && !GetActiveFirearm()))
	{
		return false;
	}
	for (const auto& Firearm : Firearms)
	{
		if (Firearm && !FMath::IsFinite(Firearm->GetDefinition().ShotsPerSecond * Multiplier))
		{
			return false;
		}
	}
	SupportRateMultiplier = Multiplier;
	return true;
}

// The legacy fixture retains its independent prototype firing rate.
float UGirlSaviorFireComponent::GetEffectiveShotsPerSecond() const
{
	const auto* Definition = GetConfiguredWeaponDefinition();
	return Definition ? Definition->ShotsPerSecond * SupportRateMultiplier : (bUsesExpeditionWeapons ? 0.f : ShotsPerSecond);
}

void UGirlSaviorFireComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	auto* PC = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
	auto* Aim = Hero ? Hero->GetAimComponent() : nullptr;
	auto* Encounter = CombatEncounter.Get();
	const bool bCombatMode = bUsesExpeditionWeapons;
	auto* Firearm = GetActiveFirearm();
	if (!PC || !PC->CanUseGameplayInput() || !PC->IsFireAllowed() || !Aim || !Aim->IsCursorInsideGameView()
		|| (bCombatMode && (!Firearm || !Encounter || Encounter->GetRunState() != EGirlSaviorRunState::Active)))
	{
		CancelFireSeries();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Schedule.ConsumeDue(Now)) return;
	const float Rate = GetEffectiveShotsPerSecond();
	if (!FMath::IsFinite(Rate) || Rate <= 0.f
		|| (!bCombatMode && (!ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract))))
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: firing needs a finite positive rate and a concrete projectile class."), *GetPathName());
		CancelFireSeries();
		return;
	}
	const FVector Position = Hero->GetCapsuleComponent()->GetComponentLocation();
	const FVector Direction = Aim->GetAimDirection();
	bool bLaunched;
	FGirlSaviorWeaponShotCue Cue;
	if (bCombatMode)
	{
		const FGirlSaviorWeaponFireContext Context{Hero, Encounter, Position, Direction, Now};
		bLaunched = Firearm->TryFire(Context, Cue);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = Hero;
		Params.Instigator = Hero;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		bLaunched = GetWorld()->SpawnActor<AGirlSaviorTestProjectile>(ProjectileClass, Position,
			FRotator(0., Direction.Rotation().Yaw, 0.), Params) != nullptr;
	}
	if (!bLaunched)
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: projectile spawn failed."), *GetPathName());
		CancelFireSeries();
		return;
	}
	Schedule.Launched(Now, Rate);
	UE_LOG(LogGirlSaviorFire, Display, TEXT("Shot time=%.6f position=%s direction=%s rate=%.3f"),
		Now, *Position.ToString(), *Direction.ToString(), Rate);
	if (bCombatMode)
	{
		OnWeaponShot.Broadcast(Cue);
	}
	// Maintain the original Blueprint event on Demo and the standalone fixture without a presenter.
	if (!Encounter || !Encounter->HasFeedbackPresenter())
	{
		OnShotCue.Broadcast(Position, Direction);
	}
}
