#pragma once

#include "CoreMinimal.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorWaveSettings.generated.h"

USTRUCT(BlueprintType)
struct FGirlSaviorWaveRecipe
{
    GENERATED_BODY()

    // Состав одной группы. Порядок ролей определяет, кого сохранить при нехватке мест.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    TArray<EGirlSaviorEnemyRole> Roles;
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorWaveSettings : public UDataAsset
{
    GENERATED_BODY()

public:
    // Общий потолок живых врагов и зарезервированных мест во всех комнатах.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 GlobalCap = 8;

    // Сколько Стрелков одновременно могут быть живы или ожидать появления.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 ShooterCap = 2;

    // Сколько Таранщиков одновременно могут быть живы или ожидать появления.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 ChargerCap = 1;

    // Максимальное число врагов, которые выходят с одного входа за одну попытку.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 MaxGroupSize = 4;

    // Минимальная пауза между фактически появившимися группами во всех комнатах.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "s"))
    float MinimumGroupGap = 3.f;

    // Время между выделением мест и повторной проверкой перед появлением группы.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "s"))
    float ReservationDelay = 0.20f;

    // Запас в пикселях вокруг игрового кадра, где новые враги ещё не могут появиться.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 ScreenMargin = 32;

    // Кратчайший допустимый путь от места появления до героя.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "cm"))
    float MinimumApproachDistance = 300.f;

    // Если уже идущий враг станет видимым за это время, пустой кадр не ускоряет волну.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "s"))
    float ImminentApproachTime = 1.f;

    // Максимальное смещение от авторской точки пола до пригодной точки на навмешe.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "cm"))
    float NavProjectionTolerance = 30.f;

    bool IsConfigurationValid() const;
    int32 GetRoleCap(EGirlSaviorEnemyRole Role) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorWaveProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    // Желаемая общая численность после пополнения в этой комнате.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 TargetCount = 3;

    // Если занято столько мест или меньше, профиль допускает новую группу.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    int32 RefillThreshold = 1;

    // Обычная пауза после последней группы перед новой проверкой пополнения.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (Units = "s"))
    float Interval = 7.f;

    // Первая успешная группа в комнате знакомит игрока с одной ролью.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    bool bHasIntroduction = false;

    // Единственный противник первой группы, если знакомство включено.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (EditCondition = "bHasIntroduction"))
    EGirlSaviorEnemyRole IntroductionRole = EGirlSaviorEnemyRole::Pursuer;

    // Приоритетные составы. Следующий состав используется, если предыдущий сейчас недоступен.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    TArray<FGirlSaviorWaveRecipe> Recipes;

    // Разрешает раннюю проверку, когда врагов нет в кадре и никто не появится в нём скоро.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
    bool bAccelerateEmptyFrame = true;

    bool IsConfigurationValid(const UGirlSaviorWaveSettings* Settings = nullptr) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
