#pragma once

#include "CoreMinimal.h"
#include "Curve/GeneralPolygon2.h"

// Геометрия комнат и допустимых опорных точек камеры в мировой плоскости XY.
class GIRLSAVIOR_API FGirlSaviorCameraRoomFraming
{
public:
    bool Build(const TArray<TArray<FVector2D>>& RoomPolygons, double Inset);
    bool Contains(const FVector2D& Point) const;
    int32 GetRegionIndex(const FVector2D& Point) const;
    FVector2D GetPreferredPivot(const FVector2D& Point) const;

private:
    TArray<UE::Geometry::FGeneralPolygon2d> Domain;
    TArray<TArray<UE::Geometry::FGeneralPolygon2d>> SafeRegions;
    TArray<TArray<FVector2D>> FallbackCenters;
};