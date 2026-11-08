#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Camera/GirlSaviorCameraSettings.h"
#include "Camera/GirlSaviorCameraRoomBounds.h"
#include "Camera/GirlSaviorCameraRoomFraming.h"
#include "Components/BoxComponent.h"
#include "Camera/GirlSaviorCameraChannels.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "SceneView.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorCamera, Log, All);

namespace
{
bool ProjectInside(const FVector& Point, const FSceneViewProjectionData& Data, const FVector2D& Margin)
{
	const FVector4 Clip = Data.ComputeViewProjectionMatrix().TransformFVector4(FVector4(Point, 1.));
	if (!FMath::IsFinite(Clip.W) || Clip.W <= UE_SMALL_NUMBER) return false;
	const double X = .5 + .5 * Clip.X / Clip.W;
	const double Y = .5 - .5 * Clip.Y / Clip.W;
	return FMath::IsFinite(X) && FMath::IsFinite(Y)
		&& X >= Margin.X && X <= 1. - Margin.X && Y >= Margin.Y && Y <= 1. - Margin.Y;
}
}

const UGirlSaviorCameraSettings& AGirlSaviorPlayerCameraManager::GetCameraSettings() const
{
	// Native/default instances remain usable before a content settings asset is assigned.
	if (CameraSettings && ensureMsgf(CameraSettings->IsConfigurationValid(), TEXT("Invalid prototype camera settings: %s"), *GetNameSafe(CameraSettings)))
		return *CameraSettings;
	return *GetDefault<UGirlSaviorCameraSettings>();
}

FVector AGirlSaviorPlayerCameraManager::ToWorldMoveIntent(const FVector2D& ScreenIntent) const
{
	const FRotationMatrix Basis(FRotator(0.f, GetCameraSettings().ViewRotation.Yaw, 0.f));
	return Basis.GetUnitAxis(EAxis::Y) * ScreenIntent.X + Basis.GetUnitAxis(EAxis::X) * ScreenIntent.Y;
}

void AGirlSaviorPlayerCameraManager::RequestFollowSnap()
{
	bSnapRequested = true;
}

void AGirlSaviorPlayerCameraManager::RefreshBoundsCache()
{
	const TArray<ULevel*>& Levels = GetWorld()->GetLevels();
	bool bChanged = CachedLevels.Num() != Levels.Num();
	for (int32 Index = 0; !bChanged && Index < Levels.Num(); ++Index)
		bChanged = CachedLevels[Index].Get() != Levels[Index];
	if (!bChanged) return;
	CachedLevels.Reset();
	Rooms.Reset();
	RoomFraming.Reset();
	for (ULevel* Level : Levels) CachedLevels.Add(Level);
	for (TActorIterator<AGirlSaviorCameraRoomBounds> It(GetWorld()); It; ++It)
	{
		if (It->IsConfigurationValid()) Rooms.Add(*It);
		else UE_LOG(LogGirlSaviorCamera, Warning, TEXT("Invalid camera room: %s"), *It->GetName());
	}
}

FVector AGirlSaviorPlayerCameraManager::ResolvePreferredPivot(const FVector& BodyCenter)
{
	RefreshBoundsCache();
	ActiveRooms.Reset();
	TArray<TArray<FVector2D>> Polygons;
	TArray<TWeakObjectPtr<AGirlSaviorCameraRoomBounds>> EligibleRooms;
	for (const auto& Entry : Rooms)
	{
		AGirlSaviorCameraRoomBounds* Room = Entry.Get();
		if (!Room || !Room->IsConfigurationValid()) continue;
		const UBoxComponent* Box = Room->MembershipBox;
		const FTransform Transform = Box->GetComponentTransform();
		const FVector Extent = Box->GetUnscaledBoxExtent();
		if (FMath::Abs(Transform.InverseTransformPosition(BodyCenter).Z) > Extent.Z) continue;
		TArray<FVector2D> Polygon;
		for (const FVector Corner : {FVector(-Extent.X,-Extent.Y,0), FVector(Extent.X,-Extent.Y,0),
			FVector(Extent.X,Extent.Y,0), FVector(-Extent.X,Extent.Y,0)})
		{
			const FVector WorldCorner=Transform.TransformPosition(Corner);
			Polygon.Add(FVector2D(WorldCorner.X,WorldCorner.Y));
		}
		Polygons.Add(MoveTemp(Polygon));
		EligibleRooms.Add(Room);
	}
	const double Inset=GetCameraSettings().WallInset;
	// Пересчитываем контуры только при изменении геометрии, набора этажей или отступа.
	if (!RoomFraming || CachedRoomPolygons!=Polygons || CachedWallInset!=Inset)
	{
		RoomFraming=MakeShared<FGirlSaviorCameraRoomFraming>();
		if (!RoomFraming->Build(Polygons,Inset) && !Polygons.IsEmpty())
			UE_LOG(LogGirlSaviorCamera, Warning, TEXT("Could not build room camera framing."));
		CachedRoomPolygons=Polygons;
		CachedWallInset=Inset;
	}
	const FVector2D BodyXY(BodyCenter.X,BodyCenter.Y);
	const int32 Region=RoomFraming->GetRegionIndex(BodyXY);
	if (Region==INDEX_NONE) return BodyCenter;
	for (const auto& Entry : EligibleRooms)
	{
		const FVector Center=Entry->MembershipBox->GetComponentLocation();
		if (RoomFraming->GetRegionIndex(FVector2D(Center.X,Center.Y))==Region)
			ActiveRooms.Add(Entry);
	}
	const FVector2D Pivot=RoomFraming->GetPreferredPivot(BodyXY);
	return FVector(Pivot.X,Pivot.Y,BodyCenter.Z);
}

bool AGirlSaviorPlayerCameraManager::BuildCandidateProjection(const FMinimalViewInfo& POV, FSceneViewProjectionData& Data) const
{
	const ULocalPlayer* Player = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
	if (!Viewport || Viewport->GetSizeXY().X <= 0 || Viewport->GetSizeXY().Y <= 0 || Player->Size.X <= 0 || Player->Size.Y <= 0)
		return false;

	// Match installed LocalPlayer.cpp: raw player rect first, then projection constraint.
	const FIntPoint Size = Viewport->GetSizeXY();
	const FIntPoint Start(FMath::TruncToInt(Player->Origin.X * Size.X) + Viewport->GetInitialPositionXY().X,
		FMath::TruncToInt(Player->Origin.Y * Size.Y) + Viewport->GetInitialPositionXY().Y);
	const FIntPoint Extent(FMath::TruncToInt(Player->Size.X * Size.X), FMath::TruncToInt(Player->Size.Y * Size.Y));
	if (Extent.X <= 0 || Extent.Y <= 0) return false;
	Data.SetViewRectangle(FIntRect(Start, Start + Extent));
	Data.ViewOrigin = POV.Location;
	Data.ViewRotationMatrix = FInverseRotationMatrix(POV.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	FMinimalViewInfo Candidate = POV;
	FMinimalViewInfo::CalculateProjectionMatrixGivenView(Candidate, AspectRatio_MaintainXFOV, Viewport, Data);
	return Data.GetConstrainedViewRect().Width() > 0 && Data.GetConstrainedViewRect().Height() > 0;
}

FVector AGirlSaviorPlayerCameraManager::ConstrainAnchor(const FVector& Anchor, const FVector& BodyCenter, const FMinimalViewInfo& POV) const
{
	FSceneViewProjectionData Data;
	if (!BuildCandidateProjection(POV, Data)) return BodyCenter;
	const FVector2D Margin(FMath::Clamp(GetCameraSettings().HeroScreenMargin.X, 0., .49), FMath::Clamp(GetCameraSettings().HeroScreenMargin.Y, 0., .49));
	if (ProjectInside(BodyCenter, Data, Margin)) return Anchor;
	const FVector Offset = POV.Rotation.Vector() * GetCameraSettings().FollowDistance;
	Data.ViewOrigin = BodyCenter - Offset;
	if (!ProjectInside(BodyCenter, Data, Margin))
	{
		UE_LOG(LogGirlSaviorCamera, Warning, TEXT("Centered hero projection failed; check camera tuning."));
		return BodyCenter;
	}
	double Fail = 0., Pass = 1.;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const double Mid = .5 * (Fail + Pass);
		Data.ViewOrigin = FMath::Lerp(Anchor, BodyCenter, Mid) - Offset;
		if (ProjectInside(BodyCenter, Data, Margin)) Pass = Mid;
		else Fail = Mid;
	}
	return FMath::Lerp(Anchor, BodyCenter, Pass);
}

void AGirlSaviorPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	const AGirlSaviorCharacter* Character = Cast<AGirlSaviorCharacter>(OutVT.Target);
	if (!Character)
	{
		FollowTarget.Reset();
		ActiveRooms.Reset();
		RestoreOcclusion();
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}
	const FVector BodyCenter = Character->GetCapsuleComponent()->GetComponentLocation();
	const FVector Preferred = ResolvePreferredPivot(BodyCenter);
	const bool bSnap = bSnapRequested || FollowTarget.Get() != Character;
	FVector Anchor = Preferred;
	if (!bSnap)
	{
		const float Settle = FMath::IsFinite(GetCameraSettings().FollowSettleSeconds) && GetCameraSettings().FollowSettleSeconds > 0.f ? GetCameraSettings().FollowSettleSeconds : .2f;
		const double Alpha = 1. - FMath::Exp(-FMath::Loge(20.) * FMath::Max(0.f, DeltaTime) / Settle);
		Anchor = FMath::Lerp(FollowAnchor, Preferred, Alpha);
		if (FVector::DistSquaredXY(Anchor, Preferred) < 1.) Anchor = Preferred;
	}
	Anchor.Z = BodyCenter.Z;
	OutVT.POV.Rotation = GetCameraSettings().ViewRotation;
	OutVT.POV.FOV = GetCameraSettings().ViewFieldOfView;
	OutVT.POV.ProjectionMode = ECameraProjectionMode::Perspective;
	OutVT.POV.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
	OutVT.POV.bConstrainAspectRatio = false;
	OutVT.POV.Location = Anchor - GetCameraSettings().ViewRotation.Vector() * GetCameraSettings().FollowDistance;
	FollowAnchor = ConstrainAnchor(Anchor, BodyCenter, OutVT.POV);
	OutVT.POV.Location = FollowAnchor - GetCameraSettings().ViewRotation.Vector() * GetCameraSettings().FollowDistance;
	FollowTarget = OutVT.Target;
	bSnapRequested = false;
	if (bSnap)
	{
		RestoreOcclusion();
		bCutEvaluated = true;
		SetGameCameraCutThisFrame();
	}
}

void AGirlSaviorPlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// Gameplay projection is authoritative; modifiers affect only other view targets.
	if (Cast<AGirlSaviorCharacter>(GetViewTarget())) ClearCachedPPBlends();
	else Super::ApplyCameraModifiers(DeltaTime, InOutPOV);
}

void AGirlSaviorPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	bCutEvaluated = false;
	Super::UpdateCamera(DeltaTime);
	if (FollowTarget.IsValid() && FollowTarget.Get() == GetViewTarget())
	{
		const bool bCut = bCutEvaluated || bGameCameraCutThisFrame;
		if (bCut) RestoreOcclusion();
		UpdateOcclusion(DeltaTime, bCut);
	}
	else
	{
		RestoreOcclusion();
		FollowTarget.Reset();
		ActiveRooms.Reset();
	}
}

void AGirlSaviorPlayerCameraManager::UpdateOcclusion(float DeltaTime, bool bImmediate)
{
	const ULocalPlayer* Player = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
	FSceneViewProjectionData Data;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> Obstructing;
	if (Player && Viewport && Player->GetProjectionData(Viewport, Data))
	{
		const FVector BodyCenter = FollowTarget->GetActorLocation();
		TArray<FVector> Samples = { BodyCenter };
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeCameraOcclusion), false);
		Params.AddIgnoredActor(FollowTarget.Get());
		for (const auto& Entry : ActiveRooms)
		{
			if (const AGirlSaviorCameraRoomBounds* Room = Entry.Get())
			{
				for (AActor* Warning : Room->WarningActors)
				{
					if (!IsValid(Warning)) continue;
					Params.AddIgnoredActor(Warning);
					if (!Warning->IsHidden() && FVector::DistSquaredXY(BodyCenter, Warning->GetActorLocation()) <= FMath::Square(FMath::Max(0.f, GetCameraSettings().NearbyWarningRange)))
						Samples.AddUnique(Warning->GetActorLocation());
				}
			}
		}
		for (const FVector& Point : Samples)
		{
			if (!ProjectInside(Point, Data, FVector2D::ZeroVector)) continue;
			TArray<FHitResult> Hits;
			// Overlap-only multi-traces return false but still contain useful hits.
			GetWorld()->LineTraceMultiByChannel(Hits, Data.ViewOrigin, Point, GirlSaviorCamera::OcclusionChannel, Params);
			for (const FHitResult& Hit : Hits)
				if (UPrimitiveComponent* Component = Hit.GetComponent(); Component && Component->ComponentHasTag(TEXT("CameraFade")))
					Obstructing.Add(Component);
		}
	}
	for (const auto& Component : Obstructing) Fades.FindOrAdd(Component);
	const float Duration = FMath::IsFinite(GetCameraSettings().FadeSeconds) && GetCameraSettings().FadeSeconds > 0.f ? GetCameraSettings().FadeSeconds : .15f;
	const float Step = FMath::Max(0.f, DeltaTime) / Duration;
	for (auto It = Fades.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* Component = It.Key().Get();
		if (!Component) { It.RemoveCurrent(); continue; }
		const float Target = Obstructing.Contains(It.Key()) ? 1.f : 0.f;
		It.Value() = bImmediate ? Target : FMath::FInterpConstantTo(It.Value(), Target, 1.f, Step);
		Component->SetCustomPrimitiveDataFloat(0, It.Value() * (1.f - FMath::Clamp(GetCameraSettings().MinimumCoverage, 0.f, 1.f)));
		if (It.Value() == 0.f && Target == 0.f) It.RemoveCurrent();
	}
}

void AGirlSaviorPlayerCameraManager::RestoreOcclusion()
{
	for (const auto& Entry : Fades)
		if (UPrimitiveComponent* Component = Entry.Key.Get()) Component->SetCustomPrimitiveDataFloat(0, 0.f);
	Fades.Reset();
}

void AGirlSaviorPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreOcclusion();
	Super::EndPlay(EndPlayReason);
}
