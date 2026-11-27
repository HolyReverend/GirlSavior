#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/GirlSaviorFireSchedule.h"
#include "GirlSaviorFireComponent.generated.h"

class AGirlSaviorTestProjectile;
class AGirlSaviorCombatEncounter;
class UDataTable;
class UGirlSaviorFirearm;
class UTexture2D;
struct FGirlSaviorWeaponRow;
struct FGirlSaviorWeaponShotCue;
struct FGirlSaviorResolvedWeaponSlot;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorShotCue, FVector, Position, FVector, Direction);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorWeaponChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorWeaponDischarged, const FGirlSaviorWeaponShotCue&);

USTRUCT()
struct GIRLSAVIOR_API FGirlSaviorWeaponSlotView
{
	GENERATED_BODY()

	UPROPERTY()
	FName WeaponId;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	TObjectPtr<UTexture2D> Icon;
};

USTRUCT()
struct GIRLSAVIOR_API FGirlSaviorWeaponView
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGirlSaviorWeaponSlotView> Slots;

	UPROPERTY()
	int32 ActiveIndex = INDEX_NONE;
};

UCLASS(ClassGroup=(Prototype), meta=(BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorFireComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UGirlSaviorFireComponent();
	// The controller validates each down edge; up edges must always reach the series.
	void SetFireButtonDown(bool bDown);
	UFUNCTION(BlueprintCallable, Category="Prototype|Fire")
	void CancelFireSeries();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype|Fire", meta=(ClampMin="0.001"))
	float ShotsPerSecond = 4.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fire")
	TSubclassOf<AGirlSaviorTestProjectile> ProjectileClass;

	// Expedition pawns require a valid table and encounter before firing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons")
	bool bUsesExpeditionWeapons = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UDataTable> DefaultWeaponTable;

	bool ConfigurePistol(const UDataTable& Table, FText& OutError);
	const FGirlSaviorWeaponRow* GetConfiguredWeaponDefinition() const;
	bool ConfigureExpeditionWeapons(TConstArrayView<FGirlSaviorResolvedWeaponSlot> Slots, int32 PreferredIndex, FText& OutError,
		bool bKeepEquippedInstances = false);
	bool ResolveDefaultWeapons(bool bUseTestLoadout, TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError) const;
	bool SetCombatEncounter(AGirlSaviorCombatEncounter* InEncounter, TConstArrayView<FGirlSaviorResolvedWeaponSlot> Slots = {},
		int32 PreferredIndex = INDEX_NONE, double Deadline = 0.0);
	void SuspendCombatEncounter();
	bool HasBoundWeaponInput() const;
	double GetNextShotGameTime() const { return Schedule.GetNextShotGameTime(); }
	bool TrySelectWeaponSlot(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category="Weapons")
	int32 GetActiveWeaponSlot() const { return ActiveSlotIndex; }
	UFUNCTION(BlueprintPure, Category="Weapons")
	FName GetWeaponSlotId(int32 SlotIndex) const;
	FGirlSaviorWeaponView GetWeaponView() const;
	bool SetSupportRateMultiplier(float Multiplier);
	float GetEffectiveShotsPerSecond() const;

	UPROPERTY(BlueprintAssignable, Category="Prototype|Fire")
	FGirlSaviorShotCue OnShotCue;
	FGirlSaviorWeaponChanged OnWeaponChanged;
	FGirlSaviorWeaponDischarged OnWeaponShot;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
private:
	UGirlSaviorFirearm* GetActiveFirearm() const;
	FGirlSaviorFireSchedule Schedule;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGirlSaviorFirearm>> Firearms;
	int32 ActiveSlotIndex = INDEX_NONE;
	TWeakObjectPtr<AGirlSaviorCombatEncounter> CombatEncounter;
	TWeakObjectPtr<AGirlSaviorCombatEncounter> PreparedWeaponEncounter;
	float SupportRateMultiplier = 1.f;
};
