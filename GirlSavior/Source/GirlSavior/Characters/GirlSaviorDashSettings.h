#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorDashSettings.generated.h"

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorDashSettings : public UDataAsset
{
    GENERATED_BODY()

public:
    // Горизонтальная дальность полного рывка по свободному полу, в сантиметрах.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", Units = "cm"))
    float Distance = 300.f;

    // Длительность полного рывка. Дальность, делённая на это время, задаёт начальную скорость.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.001", Units = "s"))
    float Duration = 0.22f;

    // Окно защиты от прямых попаданий. Досрочная остановка закрывает его сразу.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "s"))
    float ProtectionDuration = 0.12f;

    // Время до следующего рывка, отсчитанное от успешного старта.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "s"))
    float Cooldown = 1.f;

    // Минимальная доступная горизонтальная длина пути для старта, включая скольжение.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01", Units = "cm"))
    float MinimumTravel = 30.f;

    // Reject settings that cannot produce a finite, reachable dash.
    bool IsValidForDash() const
    {
        return FMath::IsFinite(Distance) && Distance > 0.f
            && FMath::IsFinite(Duration) && Duration > 0.f && FMath::IsFinite(Distance / Duration)
            && FMath::IsFinite(ProtectionDuration) && ProtectionDuration >= 0.f && ProtectionDuration <= Duration
            && FMath::IsFinite(Cooldown) && Cooldown >= 0.f
            && FMath::IsFinite(MinimumTravel) && MinimumTravel > 0.f && MinimumTravel <= Distance;
    }
};
