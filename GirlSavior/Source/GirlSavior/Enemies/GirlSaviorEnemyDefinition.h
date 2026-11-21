#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorEnemyDefinition.generated.h"

UENUM(BlueprintType)
enum class EGirlSaviorEnemyRole : uint8
{
    Pursuer
};

USTRUCT(BlueprintType)
struct FGirlSaviorPursuerTuning
{
    GENERATED_BODY()

    // Длина сектора от центра Преследователя до внешней дуги.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float Reach = 170.f;

    // Угол от направления удара до одной стороны сектора.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "Degrees"))
    float HalfAngleDegrees = 45.f;

    // Полная длительность замаха. В конце этой фазы происходит удар.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Preparation = 0.60f;

    // Последняя часть замаха, в течение которой направление сектора уже зафиксировано.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float LockedTail = 0.30f;

    // Пауза после взмаха, прежде чем Преследователь снова может идти и атаковать.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Recovery = 0.75f;

    // Физический урон одного успешного удара до защиты и сопротивления героя.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float PhysicalDamage = 10.f;
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorEnemyDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UGirlSaviorEnemyDefinition();

    // Роль определяет поведение сближения и тип атаки противника.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    EGirlSaviorEnemyRole Role = EGirlSaviorEnemyRole::Pursuer;

    // Здоровье, барьер и сопротивления, с которыми враг начинает бой.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGirlSaviorVitals Vitals;

    // Скорость обычного шага врага при сближении с героем.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (Units = "cm/s"))
    float WalkSpeed = 380.f;

    // Радиус тела для проходов, столкновений и расчёта опоры капсулы.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (Units = "cm"))
    float CapsuleRadius = 34.f;

    // Половина полной высоты тела. Влияет на столкновения и высоту проверок атаки.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (Units = "cm"))
    float CapsuleHalfHeight = 88.f;

    // Дальность, угол, фазы замаха и урон сектора Преследователя.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGirlSaviorPursuerTuning Pursuer;

    bool IsValidForSpawn() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
