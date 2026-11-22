#if WITH_DEV_AUTOMATION_TESTS

#include "Encounters/GirlSaviorWaveSpatial.h"
#include "Math/PerspectiveMatrix.h"
#include "Misc/AutomationTest.h"
#include "SceneView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWaveProjectionTest, "GirlSavior.Combat.Waves.Projection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWaveProjectionTest::RunTest(const FString& Parameters)
{
    using GirlSaviorWaveSpatial::ProjectBounds;
    using EWaveVisibility = GirlSaviorWaveSpatial::EVisibility;
    // Identity projection maps X=1 to the right edge of the constrained rectangle.
    FSceneViewProjectionData View;
    View.SetViewRectangle(FIntRect(0, 0, 800, 600));
    View.SetConstrainedViewRectangle(FIntRect(100, 50, 700, 450));
    const FBox StraddlingEdge(FVector(0.95, -0.1, 0.2), FVector(1.2, 0.1, 0.4));
    const FBox NearScreen(FVector(1.05, -0.1, 0.2), FVector(1.2, 0.1, 0.4));
    const FBox BeyondMargin(FVector(1.12, -0.1, 0.2), FVector(1.2, 0.1, 0.4));

    TestTrue(TEXT("A capsule corner in the constrained rect makes the bounds visible"),
        ProjectBounds(View, StraddlingEdge) == EWaveVisibility::Visible);
    TestTrue(TEXT("The unexpanded constrained rect excludes bounds starting 15 pixels outside"),
        ProjectBounds(View, NearScreen) == EWaveVisibility::Outside);
    TestTrue(TEXT("The 32-pixel admission margin catches those bounds"),
        ProjectBounds(View, NearScreen, 32) == EWaveVisibility::Visible);
    TestTrue(TEXT("Bounds starting 36 pixels outside are beyond the 32-pixel margin"),
        ProjectBounds(View, BeyondMargin, 32) == EWaveVisibility::Outside);
    TestTrue(TEXT("An empty constrained rectangle cannot admit a spawn"),
        ProjectBounds(FSceneViewProjectionData(), BeyondMargin, 32) == EWaveVisibility::Invalid);

    View.ProjectionMatrix = FReversedZPerspectiveMatrix(UE_PI / 4.0, 800.0, 600.0, 10.0);
    const FBox NearPlaneCrossing(FVector(300.0, -10.0, 9.0), FVector(350.0, 10.0, 15.0));
    const FBox BehindCamera(FVector(300.0, -10.0, -15.0), FVector(350.0, 10.0, -11.0));
    TestTrue(TEXT("One corner inside the perspective near plane defers even an offscreen candidate"),
        ProjectBounds(View, NearPlaneCrossing, 32) == EWaveVisibility::Invalid);
    TestTrue(TEXT("Bounds behind the camera cannot appear outside the viewport"),
        ProjectBounds(View, BehindCamera, 32) == EWaveVisibility::Invalid);
    TestTrue(TEXT("A distant live body entirely behind the camera is offscreen for retirement"),
        GirlSaviorWaveSpatial::ProjectLiveBounds(View, BehindCamera) == EWaveVisibility::Outside);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWaveApproachETATest, "GirlSavior.Combat.Waves.ApproachETA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWaveApproachETATest::RunTest(const FString& Parameters)
{
    using GirlSaviorWaveSpatial::FirstVisibleETA;
    FSceneViewProjectionData View;
    View.SetViewRectangle(FIntRect(0, 0, 800, 600));
    View.ProjectionMatrix.M[0][0] = 0.001;
    View.ProjectionMatrix.M[1][1] = 0.001;
    const FBox Enemy(FVector(1475.0, -25.0, 0.2), FVector(1525.0, 25.0, 0.4));
    const FVector Start(1500.0, 0.0, 0.3);
    const FVector Path[] = {Start, FVector(1300.0, 0.0, 0.3), FVector(800.0, 0.0, 0.3)};

    // X=1000 is the screen edge. The first visible sample is at X=1000, preceded by X=1100.
    const auto ETA = FirstVisibleETA(View, Enemy, Path, 400.f, 100.f);
    TestTrue(TEXT("A reachable enemy suppresses acceleration at the one-second boundary"),
        ETA.IsSet() && FMath::IsNearlyEqual(ETA.GetValue(), 1.0));
    TestTrue(TEXT("Unknown view falls back to the normal interval"),
        !FirstVisibleETA(FSceneViewProjectionData(), Enemy, Path, 400.f, 100.f).IsSet());
    TestTrue(TEXT("A currently visible enemy is already in-frame"),
        FirstVisibleETA(View, Enemy.ShiftBy(FVector(-700.0, 0.0, 0.0)), Path, 400.f, 100.f).Get(1.0) == 0.0);
    const FVector OutsidePath[] = {Start, FVector(1700.0, 0.0, 0.3)};
    TestTrue(TEXT("A known path that stays outside the screen has no imminent arrival"),
        FirstVisibleETA(View, Enemy, OutsidePath, 400.f, 100.f).Get(0.0) == TNumericLimits<double>::Max());
    return true;
}

#endif
