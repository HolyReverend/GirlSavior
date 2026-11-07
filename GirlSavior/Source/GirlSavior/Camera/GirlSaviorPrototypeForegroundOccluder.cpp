#include "Camera/GirlSaviorPrototypeForegroundOccluder.h"

#include "Camera/GirlSaviorCameraChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGirlSaviorPrototypeForegroundOccluder::AGirlSaviorPrototypeForegroundOccluder()
{
	PrimaryActorTick.bCanEverTick = false;
	GameplayBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("GameplayBlocker"));
	SetRootComponent(GameplayBlocker);
	GameplayBlocker->InitBoxExtent(FVector(45.0, 180.0, 150.0));
	GameplayBlocker->SetCollisionProfileName(TEXT("BlockAll"));
	GameplayBlocker->SetCollisionResponseToChannel(GirlSaviorCamera::OcclusionChannel, ECR_Ignore);
	GameplayBlocker->SetGenerateOverlapEvents(false);

	Boundary = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boundary"));
	Boundary->SetupAttachment(GameplayBlocker);
	Boundary->SetRelativeLocation(FVector(0.0, 0.0, -135.0));
	Boundary->SetRelativeScale3D(FVector(0.9, 3.6, 0.3));
	Boundary->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Boundary->SetGenerateOverlapEvents(false);

	Upper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Upper"));
	Upper->SetupAttachment(GameplayBlocker);
	Upper->SetRelativeLocation(FVector(0.0, 0.0, 15.0));
	Upper->SetRelativeScale3D(FVector(0.9, 3.6, 2.7));
	Upper->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Upper->SetCollisionResponseToAllChannels(ECR_Ignore);
	Upper->SetCollisionResponseToChannel(GirlSaviorCamera::OcclusionChannel, ECR_Overlap);
	Upper->SetGenerateOverlapEvents(false);
	Upper->ComponentTags.Add(TEXT("CameraFade"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Boundary->SetStaticMesh(Cube.Object);
		Upper->SetStaticMesh(Cube.Object);
	}
}
