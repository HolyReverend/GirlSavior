#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorCombatPistolSettings.generated.h"

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorCombatPistolSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	// Физический урон одного снаряда до сопротивления и барьера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pistol", meta = (ClampMin = "0.001"))
	float Damage = 10.f;

	// Максимальное число выстрелов в секунду при удержании кнопки.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pistol", meta = (ClampMin = "0.001"))
	float ShotsPerSecond = 4.f;

	// Радиус сферы столкновения в сантиметрах. Определяет зазор, нужный для пролёта.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pistol", meta = (ClampMin = "0.01", Units = "cm"))
	float ProjectileRadius = 6.f;

	// Скорость полёта снаряда в сантиметрах в секунду.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pistol", meta = (ClampMin = "0.01", Units = "cm/s"))
	float ProjectileSpeed = 2000.f;

	// Максимальный путь по прямой в сантиметрах. В конце пути снаряд исчезает без попадания.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pistol", meta = (ClampMin = "0.01", Units = "cm"))
	float MaxDistance = 1400.f;

	// Validate authored data before it can drive cadence or collision.
	bool IsConfigurationValid() const
	{
		return FMath::IsFinite(Damage) && Damage > 0.f
			&& FMath::IsFinite(ShotsPerSecond) && ShotsPerSecond > 0.f
			&& FMath::IsFinite(ProjectileRadius) && ProjectileRadius > 0.f
			&& FMath::IsFinite(ProjectileSpeed) && ProjectileSpeed > 0.f
			&& FMath::IsFinite(MaxDistance) && MaxDistance > 0.f
			&& FMath::IsFinite(4.f * (MaxDistance / ProjectileSpeed) + 0.25f);
	}
};
