#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GirlSaviorCameraSettings.generated.h"

// Shared tuning for the prototype gameplay view and foreground visibility.
UCLASS(BlueprintType)
class GIRLSAVIOR_API UGirlSaviorCameraSettings : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Фиксированный ракурс камеры: Pitch задаёт наклон, Yaw — направление обзора, Roll — крен. Этот же Yaw определяет направления движения WASD относительно экрана. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	FRotator ViewRotation = FRotator(-35.f, 45.f, 0.f);
	/** Расстояние от опорной точки кадра до камеры в сантиметрах вдоль направления обзора. Увеличение отдаляет камеру: герой выглядит меньше, а в кадр помещается больше пространства. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "1", Units = "cm"))
	float FollowDistance = 1600.f;
	/** Горизонтальный угол обзора перспективной камеры в градусах. Большее значение показывает больше пространства и усиливает перспективу; масштаб кадра настраивается вместе с FollowDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "5", ClampMax = "170", Units = "deg"))
	float ViewFieldOfView = 45.f;
	/** Время в секундах, за которое сглаживание убирает 95% горизонтального отклонения от неподвижной целевой точки кадра. Меньшее значение ускоряет догон; высота камеры следует за высотой героя без этого сглаживания. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0.15", ClampMax = "0.25", Units = "s"))
	float FollowSettleSeconds = .20f;
	/** Запас от краёв экрана до центра капсулы героя: X — доля ширины слева и справа, Y — доля высоты сверху и снизу. Например, 0.2 оставляет герою область 20–80%; при выходе за неё камера сокращает отставание, даже за пределами предпочтительных рамок комнаты. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", ClampMax = "0.49"))
	FVector2D HeroScreenMargin = FVector2D(.20, .20);
	/** Длительность полного приглушения или восстановления меша переднего плана в секундах. Меньшее значение быстрее открывает героя или активное предупреждение и быстрее возвращает препятствию обычный вид. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0.001", Units = "s"))
	float FadeSeconds = .15f;
	/** Доля поверхности верхнего меша, остающаяся видимой при полном приглушении: 0 — скрыть полностью, 1 — сохранить обычную видимость. Значение 0.2 оставляет около 20% покрытия через маску DitherTemporalAA; нижняя граница и коллизия препятствия сохраняются отдельно. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float MinimumCoverage = .20f;
	/** Горизонтальный радиус вокруг героя в сантиметрах для проверки перекрытий активных предупреждений связанных Room Bounds. Только предупреждения внутри радиуса и в кадре могут вызвать приглушение; видимость самого героя проверяется независимо. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", Units = "cm"))
	float NearbyWarningRange = 1000.f;

	/** Отступ опорной точки камеры от внешней границы объединённых Room Bounds, в сантиметрах. Общие стороны комнат и зазоры между перекрывающимися сторонами меньше этого расстояния не ограничивают камеру; физические стены на это правило не влияют. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", Units = "cm"))
	float WallInset = 300.f;

	bool IsConfigurationValid() const
	{
		return FMath::IsFinite(WallInset) && WallInset>=0.f && !ViewRotation.ContainsNaN() && FMath::IsFinite(FollowDistance) && FollowDistance>0.f
			&& FMath::IsFinite(ViewFieldOfView) && ViewFieldOfView>=5.f && ViewFieldOfView<=170.f
			&& FMath::IsFinite(FollowSettleSeconds) && FollowSettleSeconds>0.f
			&& FMath::IsFinite(HeroScreenMargin.X) && FMath::IsFinite(HeroScreenMargin.Y)
			&& HeroScreenMargin.X>=0. && HeroScreenMargin.X<.5 && HeroScreenMargin.Y>=0. && HeroScreenMargin.Y<.5
			&& FMath::IsFinite(FadeSeconds) && FadeSeconds>0.f
			&& FMath::IsFinite(MinimumCoverage) && MinimumCoverage>=0.f && MinimumCoverage<=1.f
			&& FMath::IsFinite(NearbyWarningRange) && NearbyWarningRange>=0.f;
	}
};
