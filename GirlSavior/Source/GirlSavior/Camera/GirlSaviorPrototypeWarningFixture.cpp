#include "Camera/GirlSaviorPrototypeWarningFixture.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AGirlSaviorPrototypeWarningFixture::AGirlSaviorPrototypeWarningFixture()
{
	PrimaryActorTick.bCanEverTick = false;
	MarkerCenter = CreateDefaultSubobject<USceneComponent>(TEXT("MarkerCenter"));
	SetRootComponent(MarkerCenter);
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(MarkerCenter);
	Marker->SetRelativeLocation(FVector(0.0, 0.0, 3.0));
	Marker->SetRelativeScale3D(FVector(2.0, 2.0, 0.04));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetGenerateOverlapEvents(false);
	Marker->SetCastShadow(false);

	ImpactCue = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactCue"));
	ImpactCue->SetupAttachment(MarkerCenter);
	ImpactCue->SetRelativeLocation(FVector(0.0, 0.0, 30.0));
	ImpactCue->SetRelativeScale3D(FVector(2.0, 2.0, 0.6));
	ImpactCue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactCue->SetGenerateOverlapEvents(false);
	ImpactCue->SetHiddenInGame(true);
	ImpactCue->SetCastShadow(false);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(MarkerCenter);
	Trigger->InitBoxExtent(FVector(100.0, 100.0, 100.0));
	Trigger->SetRelativeLocation(FVector(-300.0, 0.0, 100.0));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Marker->SetStaticMesh(Cylinder.Object);
		ImpactCue->SetStaticMesh(Cylinder.Object);
	}
}

void AGirlSaviorPrototypeWarningFixture::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGirlSaviorPrototypeWarningFixture::OnTriggerEnter);
}

void AGirlSaviorPrototypeWarningFixture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(WarningTimer);
	GetWorldTimerManager().ClearTimer(ImpactTimer);
	Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &AGirlSaviorPrototypeWarningFixture::OnTriggerEnter);
	if (AActor* Cue = ActiveImpact.Get())
	{
		Cue->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

void AGirlSaviorPrototypeWarningFixture::OnTriggerEnter(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	// Only the body entry starts a sequence, never additional presentation components.
	if (Pawn && OtherComponent == Pawn->GetRootComponent())
	{
		TriggerWarning();
	}
}

void AGirlSaviorPrototypeWarningFixture::TriggerWarning()
{
	if (bSequenceActive || !HasActorBegunPlay() || !GetWorld())
	{
		return;
	}
	bSequenceActive = true;
	SetActorHiddenInGame(false);
	const float Duration = FMath::IsFinite(WarningSeconds) && WarningSeconds > 0.0f ? WarningSeconds : 0.8f;
	GetWorldTimerManager().SetTimer(WarningTimer, this, &AGirlSaviorPrototypeWarningFixture::ShowImpact, Duration, false);
}

void AGirlSaviorPrototypeWarningFixture::ShowImpact()
{
	SetActorHiddenInGame(true);
	const float Duration = FMath::IsFinite(ImpactSeconds) && ImpactSeconds > 0.0f ? ImpactSeconds : 0.15f;
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Cue = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		ImpactCue->GetComponentTransform(), Parameters);
	if (Cue)
	{
		UStaticMeshComponent* Mesh = Cue->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(ImpactCue->GetStaticMesh());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		for (int32 Index = 0; Index < ImpactCue->GetNumMaterials(); ++Index)
		{
			Mesh->SetMaterial(Index, ImpactCue->GetMaterial(Index));
		}
		Cue->SetLifeSpan(Duration);
		ActiveImpact = Cue;
	}
	GetWorldTimerManager().SetTimer(ImpactTimer, this, &AGirlSaviorPrototypeWarningFixture::FinishImpact, Duration, false);
}

void AGirlSaviorPrototypeWarningFixture::FinishImpact()
{
	ActiveImpact.Reset();
	bSequenceActive = false;
}
