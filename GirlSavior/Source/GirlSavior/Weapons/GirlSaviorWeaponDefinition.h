#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Templates/SubclassOf.h"
#include "GirlSaviorWeaponDefinition.generated.h"

class AGirlSaviorTestProjectile;
class UGirlSaviorFirearm;
class USoundBase;
class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorWeaponPresentation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Модель оружия в руках героя."))
    TObjectPtr<UStaticMesh> HeldMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Множители масштаба модели по трём осям."))
    FVector HeldMeshScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Смещение вспышки от крепления оружия в сантиметрах."))
    FVector MuzzleOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Звук одного выстрела."))
    TObjectPtr<USoundBase> ShotSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Множитель размера вспышки."))
    float FlashScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Время видимости вспышки в секундах."))
    float FlashSeconds = 0.06f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Смещение модели назад при выстреле в сантиметрах."))
    float KickDistance = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Длительность возвращения модели после отдачи в секундах."))
    float KickRecoverSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Максимальная длина следа снаряда в сантиметрах."))
    float TrailLength = 68.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Толщина линии следа снаряда."))
    float TrailWidth = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Время затухания следа снаряда в секундах."))
    float TrailFadeSeconds = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ToolTip = "Цвет следа снаряда."))
    FLinearColor TrailColor = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorWeaponRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Название оружия в интерфейсе."))
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Описание оружия на странице снаряжения."))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Подпись источника урона от снаряда."))
    FText DamageCause;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Изображение оружия в интерфейсе."))
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Порядок карточки в списке оружия."))
    int32 DisplayOrder = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Класс с поведением оружия."))
    TSubclassOf<UGirlSaviorFirearm> FirearmClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Класс снаряда, создаваемого при выстреле."))
    TSubclassOf<AGirlSaviorTestProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Физический урон одного снаряда до защиты и снижения от расстояния."))
    float DamagePerProjectile = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Число выстрелов в секунду до модификаторов."))
    float ShotsPerSecond = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Радиус сферы столкновения снаряда в сантиметрах."))
    float ProjectileRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Скорость снаряда в сантиметрах в секунду."))
    float ProjectileSpeed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Расстояние в сантиметрах, до которого снаряд наносит полный урон."))
    float FullDamageDistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Максимальное расстояние полёта снаряда в сантиметрах."))
    float MaxDistance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Количество снарядов в одном выстреле."))
    int32 ProjectileCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Угол между соседними дробинами в градусах."))
    float PelletStepDegrees = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Число точных выстрелов в начале серии."))
    int32 TightShotCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Предельное отклонение точных выстрелов от прицела в градусах."))
    float TightSpreadDegrees = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Предельное отклонение при непрерывной стрельбе в градусах."))
    float SustainedSpreadDegrees = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Пауза между выстрелами для сброса разброса в секундах."))
    float SpreadResetSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ToolTip = "Параметры модели, звука и эффектов оружия."))
    FGirlSaviorWeaponPresentation Presentation;

    // Use the same data checks in the editor and when resolving a run.
    bool Validate(FText& OutError) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorWeaponLoadout
{
    GENERATED_BODY()

    static constexpr int32 SlotCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    FName Slot1 = TEXT("Pistol");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    FName Slot2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    FName Slot3;
};

USTRUCT()
struct GIRLSAVIOR_API FGirlSaviorResolvedWeaponSlot
{
    GENERATED_BODY()

    UPROPERTY()
    FName WeaponId;

    UPROPERTY()
    FGirlSaviorWeaponRow Row;
};

// Publish a complete three-slot snapshot, preserving empty positions and leaving output untouched on failure.
GIRLSAVIOR_API bool ResolveWeaponLoadout(const UDataTable& Table, const FGirlSaviorWeaponLoadout& Loadout,
    TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError);
