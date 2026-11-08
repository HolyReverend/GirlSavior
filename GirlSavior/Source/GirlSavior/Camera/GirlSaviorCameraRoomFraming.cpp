#include "Camera/GirlSaviorCameraRoomFraming.h"

#include "Algo/Reverse.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"

namespace
{
using namespace UE::Geometry;
constexpr double BoundaryTolerance = 0.001;

bool IsFinite(const FVector2D& P)
{
    return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y);
}

bool LexicalLess(const FVector2D& A, const FVector2D& B)
{
    return A.X < B.X || (A.X == B.X && A.Y < B.Y);
}

double Cross(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

double TwiceArea(const TArray<FVector2D>& Points)
{
    double Area = 0.0;
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        Area += Cross(Points[I], Points[(I + 1) % Points.Num()]);
    }
    return Area;
}

void Consider(const FVector2D& Candidate, const FVector2D& Point, FVector2D& Best, double& BestDistance)
{
    const double Distance = (Candidate - Point).SizeSquared();
    if (Distance < BestDistance || (Distance == BestDistance && LexicalLess(Candidate, Best)))
    {
        Best = Candidate;
        BestDistance = Distance;
    }
}

void NearestContour(const FPolygon2d& Contour, const FVector2D& Point, FVector2D& Best, double& BestDistance)
{
    const auto& Vertices = Contour.GetVertices();
    for (int32 I = 0; I < Vertices.Num(); ++I)
    {
        const FVector2D A = Vertices[I];
        const FVector2D Edge = Vertices[(I + 1) % Vertices.Num()] - A;
        const double LengthSquared = Edge.SizeSquared();
        const double T = LengthSquared > 0.0 ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0, 1.0) : 0.0;
        Consider(A + Edge * T, Point, Best, BestDistance);
    }
}

void NearestBoundary(const FGeneralPolygon2d& Polygon, const FVector2D& Point, FVector2D& Best, double& BestDistance)
{
    NearestContour(Polygon.GetOuter(), Point, Best, BestDistance);
    for (const FPolygon2d& Hole : Polygon.GetHoles())
    {
        NearestContour(Hole, Point, Best, BestDistance);
    }
}

bool ContainsWithBoundary(const FGeneralPolygon2d& Polygon, const FVector2D& Point)
{
    if (Polygon.Contains(Point))
    {
        return true;
    }
    FVector2D Nearest = Point;
    double Distance = TNumericLimits<double>::Max();
    NearestBoundary(Polygon, Point, Nearest, Distance);
    return Distance <= FMath::Square(BoundaryTolerance);
}

// Отсекаем соседнюю комнату только полосой выбранной грани.
void Clip(TArray<FVector2D>& Points, int32 Axis, double Bound, bool bKeepGreater)
{
    TArray<FVector2D> Result;
    if (Points.IsEmpty())
    {
        return;
    }
    auto Distance = [=](const FVector2D& P) { return (bKeepGreater ? 1.0 : -1.0) * (P[Axis] - Bound); };
    FVector2D Previous = Points.Last();
    double PreviousDistance = Distance(Previous);
    for (const FVector2D& Current : Points)
    {
        const double CurrentDistance = Distance(Current);
        if ((CurrentDistance >= 0.0) != (PreviousDistance >= 0.0))
        {
            Result.Add(Previous + (Current - Previous) * (PreviousDistance / (PreviousDistance - CurrentDistance)));
        }
        if (CurrentDistance >= 0.0)
        {
            Result.Add(Current);
        }
        Previous = Current;
        PreviousDistance = CurrentDistance;
    }
    Points = MoveTemp(Result);
}

TArray<FVector2D> ConvexHull(TArray<FVector2D> Points)
{
    Points.Sort([](const FVector2D& A, const FVector2D& B) { return LexicalLess(A, B); });
    TArray<FVector2D> Hull;
    for (const FVector2D& P : Points)
    {
        while (Hull.Num() >= 2 && Cross(Hull.Last() - Hull[Hull.Num() - 2], P - Hull.Last()) <= 0.0)
        {
            Hull.Pop(EAllowShrinking::No);
        }
        Hull.Add(P);
    }
    const int32 LowerCount = Hull.Num();
    for (int32 I = Points.Num() - 2; I >= 0; --I)
    {
        while (Hull.Num() > LowerCount && Cross(Hull.Last() - Hull[Hull.Num() - 2], Points[I] - Hull.Last()) <= 0.0)
        {
            Hull.Pop(EAllowShrinking::No);
        }
        Hull.Add(Points[I]);
    }
    if (Hull.Num() > 1)
    {
        Hull.Pop(EAllowShrinking::No);
    }
    return Hull;
}

FGeneralPolygon2d MakePolygon(const TArray<FVector2D>& Vertices)
{
    FPolygon2d Polygon;
    // Базис грани с внешней нормалью меняет ориентацию: приводим все контуры к одному обходу.
    TArray<FVector2D> Oriented = Vertices;
    if (TwiceArea(Oriented)<0.0) Algo::Reverse(Oriented);
    for (const FVector2D& Vertex : Oriented)
    {
        Polygon.AppendVertex(Vertex);
    }
    return FGeneralPolygon2d(Polygon);
}
}

bool FGirlSaviorCameraRoomFraming::Build(const TArray<TArray<FVector2D>>& RoomPolygons, double Inset)
{
    Domain.Reset();
    SafeRegions.Reset();
    FallbackCenters.Reset();
    if (!FMath::IsFinite(Inset) || Inset < 0.0)
    {
        return false;
    }

    TArray<TArray<FVector2D>> Rooms;
    TArray<FVector2D> Centers;
    TArray<FGeneralPolygon2d> Inputs;
    for (const TArray<FVector2D>& Input : RoomPolygons)
    {
        if (Input.Num() < 3 || Input.ContainsByPredicate([](const FVector2D& P) { return !IsFinite(P); }))
        {
            continue;
        }
        TArray<FVector2D> Vertices = Input;
        double Area = TwiceArea(Vertices);
        if (FMath::Abs(Area) <= UE_DOUBLE_SMALL_NUMBER)
        {
            continue;
        }
        if (Area < 0.0)
        {
            Algo::Reverse(Vertices);
            Area = -Area;
        }
        FVector2D Center = FVector2D::ZeroVector;
        for (int32 I = 0; I < Vertices.Num(); ++I)
        {
            const FVector2D& A = Vertices[I];
            const FVector2D& B = Vertices[(I + 1) % Vertices.Num()];
            Center += (A + B) * Cross(A, B);
        }
        Centers.Add(Center / (3.0 * Area));
        Inputs.Add(MakePolygon(Vertices));
        Rooms.Add(MoveTemp(Vertices));
    }

    TArray<FGeneralPolygon2d> OriginalDomain;
    if (Inputs.IsEmpty() || !PolygonsUnion(Inputs, OriginalDomain, false)) return false;

    // Мост соединяет только реально прикрытый участок грани; дальние комнаты не расширяют контур.
    if (Inset > 0.0)
    {
        for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
        {
            const auto& Room = Rooms[RoomIndex];
            for (int32 EdgeIndex = 0; EdgeIndex < Room.Num(); ++EdgeIndex)
            {
                const FVector2D Origin = Room[EdgeIndex];
                const FVector2D Edge = Room[(EdgeIndex + 1) % Room.Num()] - Origin;
                const double Length = Edge.Size();
                if (Length <= UE_DOUBLE_SMALL_NUMBER)
                {
                    continue;
                }
                const FVector2D Tangent = Edge / Length;
                const FVector2D Normal(Tangent.Y, -Tangent.X);
                for (int32 NeighborIndex = 0; NeighborIndex < Rooms.Num(); ++NeighborIndex)
                {
                    if (NeighborIndex == RoomIndex)
                    {
                        continue;
                    }
                    TArray<FVector2D> Local;
                    for (const FVector2D& P : Rooms[NeighborIndex])
                    {
                        const FVector2D Delta = P - Origin;
                        Local.Emplace(FVector2D::DotProduct(Delta, Tangent), FVector2D::DotProduct(Delta, Normal));
                    }
                    Clip(Local, 0, 0.0, true);
                    Clip(Local, 0, Length, false);
                    Clip(Local, 1, 0.0, true);
                    Clip(Local, 1, Inset, false);
                    if (Local.Num() < 3 || FMath::Abs(TwiceArea(Local)) <= UE_DOUBLE_SMALL_NUMBER)
                    {
                        continue;
                    }
                    double MinNormal = Inset;
                    double MaxNormal = 0.0;
                    for (const FVector2D& P : Local)
                    {
                        MinNormal = FMath::Min(MinNormal, P.Y);
                        MaxNormal = FMath::Max(MaxNormal, P.Y);
                    }
                    if (!(MinNormal < Inset && MaxNormal > 0.0))
                    {
                        continue;
                    }
                    const int32 ClippedCount = Local.Num();
                    for (int32 I = 0; I < ClippedCount; ++I)
                    {
                        const double AlongEdge = Local[I].X;
                        Local.Emplace(AlongEdge, 0.0);
                    }
                    TArray<FVector2D> Bridge = ConvexHull(MoveTemp(Local));
                    for (FVector2D& P : Bridge)
                    {
                        P = Origin + Tangent * P.X + Normal * P.Y;
                    }
                    const FGeneralPolygon2d BridgePolygon=MakePolygon(Bridge);
                    TArray<FGeneralPolygon2d> Candidate { BridgePolygon };
                    TArray<FGeneralPolygon2d> AddedCoverage;
                    if (!PolygonsDifference(Candidate, OriginalDomain, AddedCoverage)) return false;
                    // Отбрасываем только полностью избыточное покрытие: контакт на одном участке
                    // не должен отменять реальный зазор на другом участке повёрнутой стороны.
                    if (!AddedCoverage.IsEmpty()) Inputs.Add(BridgePolygon);
                }
            }
        }
    }
    if (Inputs.IsEmpty() || !PolygonsUnion(Inputs, Domain, false) || Domain.IsEmpty())
    {
        Domain.Reset();
        return false;
    }

    // Каждая связная область имеет собственные опоры: камера не перескакивает в соседнюю область.
    SafeRegions.SetNum(Domain.Num());
    FallbackCenters.SetNum(Domain.Num());
    for (int32 I = 0; I < Domain.Num(); ++I)
    {
        TArray<FGeneralPolygon2d> Component;
        Component.Add(Domain[I]);
        if (!PolygonsOffset(-Inset, Component, SafeRegions[I], false, 2.0, EPolygonOffsetJoinType::Miter))
        {
            Domain.Reset();
            SafeRegions.Reset();
            FallbackCenters.Reset();
            return false;
        }
        for (const FVector2D& Center : Centers)
        {
            if (ContainsWithBoundary(Domain[I], Center))
            {
                FallbackCenters[I].Add(Center);
            }
        }
    }
    return true;
}

bool FGirlSaviorCameraRoomFraming::Contains(const FVector2D& Point) const
{
    return GetRegionIndex(Point) != INDEX_NONE;
}

FVector2D FGirlSaviorCameraRoomFraming::GetPreferredPivot(const FVector2D& Point) const
{
    if (!IsFinite(Point))
    {
        return Point;
    }
    for (int32 I = 0; I < Domain.Num(); ++I)
    {
        if (!ContainsWithBoundary(Domain[I], Point))
        {
            continue;
        }
        for (const FGeneralPolygon2d& Safe : SafeRegions[I])
        {
            if (ContainsWithBoundary(Safe, Point))
            {
                return Point;
            }
        }
        FVector2D Best = Point;
        double BestDistance = TNumericLimits<double>::Max();
        for (const FGeneralPolygon2d& Safe : SafeRegions[I])
        {
            NearestBoundary(Safe, Point, Best, BestDistance);
        }
        // Если отступ поглотил маленькую комнату, выбираем ближайший исходный центр этой области.
        if (SafeRegions[I].IsEmpty())
        {
            for (const FVector2D& Center : FallbackCenters[I])
            {
                Consider(Center, Point, Best, BestDistance);
            }
        }
        return Best;
    }
    return Point;
}
int32 FGirlSaviorCameraRoomFraming::GetRegionIndex(const FVector2D& Point) const
{
    if (IsFinite(Point))
    {
        for (int32 I = 0; I < Domain.Num(); ++I)
        {
            if (ContainsWithBoundary(Domain[I], Point))
            {
                return I;
            }
        }
    }
    return INDEX_NONE;
}