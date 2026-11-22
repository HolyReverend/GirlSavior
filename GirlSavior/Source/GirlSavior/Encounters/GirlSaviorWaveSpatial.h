#pragma once

#include "CoreMinimal.h"

class AGirlSaviorCombatDoor;
class AGirlSaviorCombatEncounter;
class AGirlSaviorEnemyCharacter;
struct FSceneViewProjectionData;

namespace GirlSaviorWaveSpatial
{
enum class EVisibility : uint8 { Invalid, Visible, Outside };

EVisibility ProjectBounds(const FSceneViewProjectionData& View, const FBox& Bounds, int32 Margin = 0);
EVisibility ProjectLiveBounds(const FSceneViewProjectionData& View, const FBox& Bounds);
FBox SpawnBounds(const AGirlSaviorEnemyCharacter& Defaults, const FTransform& Transform);
FBox EnemyBounds(const AGirlSaviorEnemyCharacter& Enemy);
EVisibility EnemyVisibility(const FSceneViewProjectionData& View, const AGirlSaviorEnemyCharacter& Enemy);

bool IsSlotClear(const AGirlSaviorCombatEncounter& Encounter, const AGirlSaviorEnemyCharacter& Defaults,
    const FTransform& Transform, float FloorTolerance);
bool IsSegmentOpen(const AGirlSaviorCombatEncounter& Encounter, TConstArrayView<TWeakObjectPtr<const AGirlSaviorCombatDoor>> Doors,
    const FVector& StartCenter, const FVector& EndCenter, float Radius);
bool FindApproachPath(const AGirlSaviorCombatEncounter& Encounter, TConstArrayView<TWeakObjectPtr<const AGirlSaviorCombatDoor>> Doors,
    const AGirlSaviorEnemyCharacter& Enemy, const FVector& FromCenter, const FVector& TargetCenter,
    float ProjectionTolerance, TArray<FVector>& OutCenterPath, double& OutLength);

TOptional<double> FirstVisibleETA(const FSceneViewProjectionData& View, const FBox& CurrentBounds,
    TConstArrayView<FVector> CenterPath, float WalkSpeed, float SampleSpacing);
}
