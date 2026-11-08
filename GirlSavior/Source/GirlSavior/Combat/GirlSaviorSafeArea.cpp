#include "Combat/GirlSaviorSafeArea.h"

#include "Components/BoxComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "GameFramework/Pawn.h"

// Safe areas overlap pawns without obstructing movement or projectiles.
AGirlSaviorSafeArea::AGirlSaviorSafeArea()
{
	PrimaryActorTick.bCanEverTick = false;
	SafeVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SafeVolume"));
	SetRootComponent(SafeVolume);
	SafeVolume->InitBoxExtent(FVector(150.0f, 150.0f, 120.0f));
	SafeVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SafeVolume->SetCollisionObjectType(ECC_WorldDynamic);
	SafeVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	SafeVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SafeVolume->SetGenerateOverlapEvents(true);
	SafeVolume->OnComponentBeginOverlap.AddDynamic(this, &AGirlSaviorSafeArea::HandleBeginOverlap);
	SafeVolume->OnComponentEndOverlap.AddDynamic(this, &AGirlSaviorSafeArea::HandleEndOverlap);
}

// Include pawns already inside when the level starts.
void AGirlSaviorSafeArea::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> OverlappingPawns;
	SafeVolume->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
	for (AActor* Pawn : OverlappingPawns)
	{
		RegisterPawn(Pawn);
	}
}

// Streaming or destroying a volume must release only its own permission token.
void AGirlSaviorSafeArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<AGirlSaviorPlayerController>& Controller : RegisteredControllers)
	{
		if (Controller.IsValid())
		{
			Controller->SetSafeAreaActive(this, false);
		}
	}
	RegisteredControllers.Empty();
	Super::EndPlay(EndPlayReason);
}

void AGirlSaviorSafeArea::RegisterPawn(AActor* OtherActor)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	AGirlSaviorPlayerController* Controller = Pawn ? Cast<AGirlSaviorPlayerController>(Pawn->GetController()) : nullptr;
	if (Controller && Controller->GetPawn() == Pawn)
	{
		RegisteredControllers.Add(Controller);
		Controller->SetSafeAreaActive(this, true);
	}
}

// Repeated overlaps are idempotent in the controller's area registry.
void AGirlSaviorSafeArea::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	RegisterPawn(OtherActor);
}

// Wait for every overlapping component of the possessed pawn to leave.
void AGirlSaviorSafeArea::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (SafeVolume->IsOverlappingActor(OtherActor))
	{
		return;
	}

	for (auto It = RegisteredControllers.CreateIterator(); It; ++It)
	{
		AGirlSaviorPlayerController* Controller = It->Get();
		if (!Controller)
		{
			It.RemoveCurrent();
		}
		else if (Controller->GetPawn() == OtherActor || !SafeVolume->IsOverlappingActor(Controller->GetPawn()))
		{
			Controller->SetSafeAreaActive(this, false);
			It.RemoveCurrent();
		}
	}
}