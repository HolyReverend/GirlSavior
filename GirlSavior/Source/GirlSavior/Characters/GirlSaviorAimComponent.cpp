#include "Characters/GirlSaviorAimComponent.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
static TAutoConsoleVariable<int32> CVarAimDebug(TEXT("GirlSavior.AimDebug"), 0,
	TEXT("Draw current aim and print screen-space hold measurements in PIE."));
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorAim, Log, All);

UGirlSaviorAimComponent::UGirlSaviorAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bRunOnAnyThread = false;
}

void UGirlSaviorAimComponent::BeginPlay()
{
	Super::BeginPlay();
	ValidateTuning();
	if (const AActor* Owner = GetOwner())
	{
		AimDirection = FRotator(0., Owner->GetActorRotation().Yaw, 0.).Vector();
	}
	bHolding = false;
	ClearCursorSample();
}

void UGirlSaviorAimComponent::ValidateTuning()
{
	if (!FMath::IsFinite(HoldEnterFraction) || !FMath::IsFinite(HoldExitFraction)
		|| HoldEnterFraction <= 0.f || HoldEnterFraction >= HoldExitFraction || HoldExitFraction >= .5f)
	{
		UE_LOG(LogGirlSaviorAim, Warning, TEXT("%s: invalid aim radii; using 0.010 / 0.015."), *GetPathName());
		HoldEnterFraction = .010f;
		HoldExitFraction = .015f;
	}
}

void UGirlSaviorAimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ClearCursorSample();
	const AGirlSaviorCharacter* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	const AGirlSaviorPlayerController* Controller = Hero
		? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
	if (!Hero || !Hero->IsLocallyControlled() || !Controller || !Controller->CanUseGameplayInput()) return;
	ULocalPlayer* Player = Controller->GetLocalPlayer();
	FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
	float MouseX = 0.f, MouseY = 0.f;
	FSceneViewProjectionData Projection;
	if (!Viewport || !Controller->GetMousePosition(MouseX, MouseY)
		|| !Player->GetProjectionData(Viewport, Projection)) return;
	ResolveViewSample(Hero->GetCapsuleComponent()->GetComponentLocation(), FVector2D(MouseX, MouseY),
		Projection.GetConstrainedViewRect(), Projection.ComputeViewProjectionMatrix());
#if WITH_EDITOR
	if (CVarAimDebug.GetValueOnGameThread() != 0)
	{
		const FVector Body = Hero->GetCapsuleComponent()->GetComponentLocation();
		DrawDebugDirectionalArrow(GetWorld(), Body, Body + AimDirection * 180., 30.f,
			bHolding ? FColor::Yellow : FColor::Green, false, 0.f, 0, 4.f);
		FVector2D Screen;
		if (FSceneView::ProjectWorldToScreen(Body, Projection.GetConstrainedViewRect(),
			Projection.ComputeViewProjectionMatrix(), Screen))
		{
			const int32 Height = Projection.GetConstrainedViewRect().Height();
			DrawDebugString(GetWorld(), Body + FVector(0, 0, 90), FString::Printf(
				TEXT("body %.1f,%.1f  d=%.2f  h=%d  radii=%.2f/%.2f  hold=%d"),
				Screen.X, Screen.Y, FVector2D::Distance(Screen, FVector2D(MouseX, MouseY)), Height,
				HoldEnterFraction * Height, HoldExitFraction * Height, bHolding), nullptr, FColor::White, 0.f);
		}
	}
#endif
}

void UGirlSaviorAimComponent::ResolveViewSample(const FVector& BodyOrigin, const FVector2D& Cursor,
	const FIntRect& ViewRect, const FMatrix& ViewProjection)
{
	// Pixel validity is independent of the plane calculation, including failed geometry.
	ClearCursorSample();
	if (ViewRect.Width() <= 0 || ViewRect.Height() <= 0 || Cursor.ContainsNaN()
		|| Cursor.X < ViewRect.Min.X || Cursor.Y < ViewRect.Min.Y
		|| Cursor.X >= ViewRect.Max.X || Cursor.Y >= ViewRect.Max.Y) return;
	bCursorInsideGameView = true;
	if (BodyOrigin.ContainsNaN() || ViewProjection.ContainsNaN()) return;
	const double Determinant = ViewProjection.Determinant();
	if (!FMath::IsFinite(Determinant) || Determinant == 0.) return;
	const FMatrix Inverse = ViewProjection.Inverse();
	if (Inverse.ContainsNaN()) return;
	FVector2D BodyScreen;
	if (!FSceneView::ProjectWorldToScreen(BodyOrigin, ViewRect, ViewProjection, BodyScreen)
		|| BodyScreen.ContainsNaN()) return;
	FVector RayOrigin, RayDirection;
	FSceneView::DeprojectScreenToWorld(Cursor, ViewRect, Inverse, RayOrigin, RayDirection);
	if (RayOrigin.ContainsNaN() || RayDirection.ContainsNaN() || FMath::Abs(RayDirection.Z) <= 1.e-6) return;
	const double T = (BodyOrigin.Z - RayOrigin.Z) / RayDirection.Z;
	if (!FMath::IsFinite(T) || T < 0.) return;
	const FVector Hit = RayOrigin + T * RayDirection;
	if (Hit.ContainsNaN()) return;
	const FVector Planar(Hit.X - BodyOrigin.X, Hit.Y - BodyOrigin.Y, 0.);
	const double Radius = double(bHolding ? HoldExitFraction : HoldEnterFraction) * ViewRect.Height();
	if (FVector2D::DistSquared(Cursor, BodyScreen) <= FMath::Square(Radius))
	{
		bHolding = true;
		return;
	}
	const FVector Direction = Planar.GetSafeNormal();
	if (Direction.IsNearlyZero() || Direction.ContainsNaN()) return;
	AimDirection = Direction;
	bHolding = false;
	if (AActor* Owner = GetOwner())
	{
		FRotator Rotation = Owner->GetActorRotation();
		Rotation.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
		Owner->SetActorRotation(Rotation);
	}
}
