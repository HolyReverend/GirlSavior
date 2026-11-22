#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorEnemyDefinition.generated.h"

class AGirlSaviorTestProjectile;

UENUM(BlueprintType)
enum class EGirlSaviorEnemyRole : uint8
{
    Pursuer,
    Shooter,
    Charger
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

USTRUCT(BlueprintType)
struct FGirlSaviorShooterTuning
{
    GENERATED_BODY()

    // Максимальная дистанция между центрами Стрелка и героя для начала прицеливания.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float Range = 900.f;

    // Дистанция до героя, на которой Стрелок ищет запасную огневую позицию.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float PreferredDistance = 700.f;

    // Длительность прицеливания перед выстрелом, включая время с зафиксированной линией.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Preparation = 0.85f;

    // Минимальное время показа зафиксированной линии перед выстрелом.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float LockedTail = 0.35f;

    // Пауза после выстрела, прежде чем Стрелок снова сможет идти и прицеливаться.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Recovery = 0.8f;

    // Физический урон одного попадания снаряда до защиты героя.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float PhysicalDamage = 12.f;

    // Радиус сферы столкновения снаряда. Влияет на зазор при прицеливании и полёте.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float ProjectileRadius = 12.f;

    // Скорость полёта снаряда вдоль зафиксированной линии.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm/s"))
    float ProjectileSpeed = 1100.f;

    // Максимальный путь снаряда до исчезновения при отсутствии препятствий.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float ProjectileRange = 1100.f;

    // Класс снаряда со своим визуальным представлением и эффектом попадания.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TSubclassOf<AGirlSaviorTestProjectile> ProjectileClass;
};

USTRUCT(BlueprintType)
struct FGirlSaviorChargerTuning
{
    GENERATED_BODY()

    // Минимальная дистанция между центрами Тарана и героя для начала разбега.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float MinRange = 350.f;

    // Максимальная дистанция между центрами Тарана и героя для начала разбега.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float MaxRange = 750.f;

    // Длительность подготовки разбега, включая время с зафиксированной полосой.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Preparation = 1.0f;

    // Минимальное время показа зафиксированной полосы перед началом движения.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float LockedTail = 0.40f;

    // Пауза после остановки, прежде чем Таран снова сможет идти и атаковать.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "s"))
    float Recovery = 1.1f;

    // Физический урон первого столкновения с героем во время разбега.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    float PhysicalDamage = 18.f;

    // Скорость движения тела по зафиксированной полосе.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm/s"))
    float ChargeSpeed = 1300.f;

    // Максимальный путь центра Тарана при разбеге без препятствий.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (Units = "cm"))
    float MaxTravel = 750.f;
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

    // Дистанции, фазы прицеливания, урон и параметры снаряда Стрелка.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGirlSaviorShooterTuning Shooter;

    // Дистанции начала, фазы подготовки, урон и параметры разбега Тарана.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGirlSaviorChargerTuning Charger;

    bool IsValidForSpawn() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
