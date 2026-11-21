#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorVitalsSettings.generated.h"

USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorVitals
{
	GENERATED_BODY()

	// Максимум и начальный запас здоровья. При достижении нуля персонаж погибает.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0.01"))
	float MaxHealth = 100.f;

	// Начальный запас барьера. Барьер поглощает урон до того, как он уменьшит здоровье.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0"))
	float InitialBarrier = 0.f;

	// Доля поглощаемого физического урона. При 1 физический урон полностью поглощается.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0", ClampMax = "1"))
	float PhysicalResistance = 0.f;

	// Доля поглощаемого магического урона. При 1 магический урон полностью поглощается.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0", ClampMax = "1"))
	float MagicalResistance = 0.f;

	// Both editor-authored assets and inline enemy definitions follow these limits.
	bool IsConfigurationValid() const
	{
		return FMath::IsFinite(MaxHealth) && MaxHealth > 0.f
			&& FMath::IsFinite(InitialBarrier) && InitialBarrier >= 0.f
			&& FMath::IsFinite(PhysicalResistance) && PhysicalResistance >= 0.f && PhysicalResistance <= 1.f
			&& FMath::IsFinite(MagicalResistance) && MagicalResistance >= 0.f && MagicalResistance <= 1.f;
	}
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorVitalsSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	// Максимум и начальный запас здоровья. При достижении нуля персонаж погибает.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0.01"))
	float MaxHealth = 100.f;

	// Начальный запас барьера. Барьер поглощает урон до того, как он уменьшит здоровье.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0"))
	float InitialBarrier = 0.f;

	// Доля поглощаемого физического урона от 0 до 1. Применяется до барьера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0", ClampMax = "1"))
	float PhysicalResistance = 0.f;

	// Доля поглощаемого магического урона от 0 до 1. Применяется до барьера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0", ClampMax = "1"))
	float MagicalResistance = 0.f;

	// The receiver snapshots the authored values during initialization.
	FGirlSaviorVitals GetVitals() const
	{
		return {MaxHealth, InitialBarrier, PhysicalResistance, MagicalResistance};
	}
};
