#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/GirlSaviorCameraRoomFraming.h"
#include "Misc/AutomationTest.h"

namespace GirlSaviorRoomFramingTests
{
TArray<FVector2D> Rectangle(double Left, double Bottom, double Right, double Top)
{
	return { FVector2D(Left, Bottom), FVector2D(Right, Bottom),
		FVector2D(Right, Top), FVector2D(Left, Top) };
}

FVector2D Transform(const FVector2D& Point, double Scale)
{
	const double Diagonal = FMath::Sqrt(0.5);
	return FVector2D(Point.X - Point.Y, Point.X + Point.Y) * (Scale * Diagonal);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorRoomFramingTest,
	"GirlSavior.Camera.RoomFraming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorRoomFramingTest::RunTest(const FString& Parameters)
{
	using namespace GirlSaviorRoomFramingTests;
	const auto Square = Rectangle(-1200.0, -1200.0, 1200.0, 1200.0);
	const auto Annex = Rectangle(1200.0, -500.0, 2400.0, 500.0);
	constexpr double Tolerance = 0.05;
	const auto ExpectPivot = [this](const TCHAR* Label, const FGirlSaviorCameraRoomFraming& Framing,
		const FVector2D& Point, const FVector2D& Expected, double AllowedError = 0.05)
	{
		const FVector2D Actual = Framing.GetPreferredPivot(Point);
		TestTrue(FString::Printf(TEXT("%s: expected %s, actual %s"), Label,
			*Expected.ToString(), *Actual.ToString()), Actual.Equals(Expected, AllowedError));
	};

	FGirlSaviorCameraRoomFraming Isolated;
	if (!TestTrue(TEXT("Build isolated square"), Isolated.Build({ Square }, 300.0)))
	{
		return false;
	}
	ExpectPivot(TEXT("Exposed square edge applies inset"), Isolated, FVector2D(1100.0, 0.0), FVector2D(900.0, 0.0));

	FGirlSaviorCameraRoomFraming Joined;
	if (!TestTrue(TEXT("Build touching Demo rooms"), Joined.Build({ Square, Annex }, 300.0)))
	{
		return false;
	}
	ExpectPivot(TEXT("Approach to internal seam follows player"), Joined, FVector2D(1100.0, 0.0), FVector2D(1100.0, 0.0));
	ExpectPivot(TEXT("Beyond internal seam follows player"), Joined, FVector2D(1300.0, 0.0), FVector2D(1300.0, 0.0));
	TestEqual(TEXT("Touching rooms share component"), Joined.GetRegionIndex(FVector2D::ZeroVector),
		Joined.GetRegionIndex(FVector2D(1800.0, 0.0)));
	const FVector2D Exposed = Joined.GetPreferredPivot(FVector2D(1100.0, 1000.0));
	TestTrue(TEXT("Exposed upper wall still applies both insets"),
		Exposed.X <= 900.0 + Tolerance && Exposed.Y <= 900.0 + Tolerance);

	// Порог зазора строгий: мост возникает только при расстоянии меньше отступа.
	for (const double Gap : { 299.9, 300.0, 300.1 })
	{
		FGirlSaviorCameraRoomFraming Gapped;
		const auto DistantAnnex = Rectangle(1200.0 + Gap, -500.0, 2400.0 + Gap, 500.0);
		if (!TestTrue(FString::Printf(TEXT("Build gap %.1f"), Gap), Gapped.Build({ Square, DistantAnnex }, 300.0)))
		{
			continue;
		}
		const int32 FirstRegion = Gapped.GetRegionIndex(FVector2D::ZeroVector);
		const int32 SecondRegion = Gapped.GetRegionIndex(FVector2D(1800.0 + Gap, 0.0));
		TestTrue(TEXT("Each room has a valid region"), FirstRegion != INDEX_NONE && SecondRegion != INDEX_NONE);
		if (Gap < 300.0)
		{
			TestEqual(TEXT("Subthreshold gap joins components"), FirstRegion, SecondRegion);
			const FVector2D GapPoint(1350.0, 0.0);
			TestTrue(TEXT("Bridge contains gap point"), Gapped.Contains(GapPoint));
			ExpectPivot(TEXT("Bridge follows player"), Gapped, GapPoint, GapPoint);
		}
		else
		{
			TestTrue(TEXT("Threshold and larger gaps remain separate"), FirstRegion != SecondRegion);
			ExpectPivot(TEXT("Separate room cannot supply another room's safe pivot"), Gapped,
				FVector2D(1100.0, 0.0), FVector2D(900.0, 0.0));
		}
	}

	FGirlSaviorCameraRoomFraming DiagonalRooms;
	TestTrue(TEXT("Build nearby corners without facing side overlap"),
		DiagonalRooms.Build({ Square, Rectangle(1300.0, 1300.0, 2500.0, 2500.0) }, 300.0));
	const int32 SquareRegion = DiagonalRooms.GetRegionIndex(FVector2D::ZeroVector);
	const int32 DiagonalRegion = DiagonalRooms.GetRegionIndex(FVector2D(1900.0, 1900.0));
	TestTrue(TEXT("Nearby corners do not create a doorway bridge"),
		SquareRegion != INDEX_NONE && DiagonalRegion != INDEX_NONE && SquareRegion != DiagonalRegion);
	FGirlSaviorCameraRoomFraming ZeroInset;
	TestTrue(TEXT("Build touching rooms without inset"), ZeroInset.Build({ Square, Annex }, 0.0));
	TestEqual(TEXT("Zero inset still unions touching footprints"), ZeroInset.GetRegionIndex(FVector2D::ZeroVector),
		ZeroInset.GetRegionIndex(FVector2D(1800.0, 0.0)));
	ExpectPivot(TEXT("Zero inset follows seam"), ZeroInset, FVector2D(1200.0, 0.0), FVector2D(1200.0, 0.0));

	FGirlSaviorCameraRoomFraming Overlapping;
	TestTrue(TEXT("Build overlapping and duplicate footprints"),
		Overlapping.Build({ Square, Rectangle(1000.0, -500.0, 2400.0, 500.0), Square }, 300.0));
	ExpectPivot(TEXT("Overlap does not recreate internal wall"), Overlapping, FVector2D(1100.0, 0.0), FVector2D(1100.0, 0.0));
	TestTrue(TEXT("Duplicate footprint preserves membership"), Overlapping.Contains(FVector2D(-1100.0, 0.0)));

	FGirlSaviorCameraRoomFraming Reordered;
	TestTrue(TEXT("Build reversed room order"), Reordered.Build({ Annex, Square }, 300.0));
	for (const FVector2D Point : { FVector2D(1100.0, 0.0), FVector2D(1300.0, 0.0), FVector2D(1100.0, 1000.0) })
	{
		ExpectPivot(TEXT("Room order preserves preferred pivot"), Reordered, Point, Joined.GetPreferredPivot(Point));
	}

	// Поворот и масштаб проверяют геометрию независимо от мировых осей и единиц.
	for (const double Scale : { 0.1, 1.0, 10.0 })
	{
		TArray<TArray<FVector2D>> RotatedRooms = { Square, Annex };
		for (TArray<FVector2D>& Polygon : RotatedRooms)
		{
			for (FVector2D& Vertex : Polygon)
			{
				Vertex = Transform(Vertex, Scale);
			}
		}
		FGirlSaviorCameraRoomFraming Rotated;
		TestTrue(TEXT("Build rotated and scaled rooms"), Rotated.Build(RotatedRooms, 300.0 * Scale));
		const FVector2D SeamPoint = Transform(FVector2D(1100.0, 0.0), Scale);
		ExpectPivot(TEXT("Rotated internal seam follows player"), Rotated, SeamPoint, SeamPoint, Tolerance * FMath::Max(1.0, Scale));
		ExpectPivot(TEXT("Rotated exposed edge applies inset"), Rotated,
			Transform(FVector2D(-1100.0, 0.0), Scale), Transform(FVector2D(-900.0, 0.0), Scale), Tolerance * FMath::Max(1.0, Scale));
	}

	FGirlSaviorCameraRoomFraming Ring;
	TestTrue(TEXT("Build ring without filling its cavity"), Ring.Build({
		Rectangle(-2000,-2000,2000,-1000), Rectangle(-2000,1000,2000,2000),
		Rectangle(-2000,-1000,-1000,1000), Rectangle(1000,-1000,2000,1000)},200));
	TestFalse(TEXT("Union preserves unfilled hole"), Ring.Contains(FVector2D::ZeroVector));
	ExpectPivot(TEXT("Inset respects hole boundary"),Ring,FVector2D(1100,0),FVector2D(1200,0));
	// Широкий повёрнутый сосед оставляет 300 см до внешних углов; узкий проём не обязан сохранять полный отступ.
	TArray<FVector2D> IndependentAnnex=Rectangle(900,-900,2700,900);
	for (FVector2D& Vertex:IndependentAnnex) Vertex=Transform(Vertex-FVector2D(1800,0),1.)+FVector2D(1800,0);
	FGirlSaviorCameraRoomFraming IndependentYaw;
	TestTrue(TEXT("Build independently rotated neighbor"),IndependentYaw.Build({Square,IndependentAnnex},300));
	ExpectPivot(TEXT("Independent yaw releases covered seam"),IndependentYaw,FVector2D(1100,0),FVector2D(1100,0));

	// На одной повёрнутой стороне могут одновременно быть пересечение и небольшой зазор.
	TArray<FVector2D> WedgeNeighbor=Rectangle(1200,-1500,2400,1500);
	const double C=FMath::Cos(FMath::DegreesToRadians(-10.)), S=FMath::Sin(FMath::DegreesToRadians(-10.));
	for(FVector2D& V:WedgeNeighbor)
	{
		const FVector2D D=V-FVector2D(1800,0);
		V=FVector2D(C*D.X-S*D.Y,S*D.X+C*D.Y)+FVector2D(1800,0);
	}
	FGirlSaviorCameraRoomFraming Wedge;
	TestTrue(TEXT("Build partial overlap and partial facing gap"),Wedge.Build({Square,WedgeNeighbor},300));
	ExpectPivot(TEXT("Overlap elsewhere does not suppress subthreshold gap"),Wedge,FVector2D(1100,900),FVector2D(1100,900));

	FGirlSaviorCameraRoomFraming Collapsed;
	TestTrue(TEXT("Build room whose erosion is empty"), Collapsed.Build({ Rectangle(-100.0, -100.0, 100.0, 100.0) }, 300.0));
	ExpectPivot(TEXT("Empty erosion uses documented centroid fallback"), Collapsed, FVector2D(90.0, 0.0), FVector2D::ZeroVector);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS