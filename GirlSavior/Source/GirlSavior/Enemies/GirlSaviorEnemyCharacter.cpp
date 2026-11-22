#include "Enemies/GirlSaviorEnemyCharacter.h"

#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAIController.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/TriggerBox.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorEnemy, Log, All);

AGirlSaviorEnemyCharacter::AGirlSaviorEnemyCharacter()
{
    AIControllerClass = AGirlSaviorEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    Capsule->InitCapsuleSize(34.f, 88.f);
    Capsule->SetCollisionObjectType(ECC_Pawn);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
    Capsule->SetCanEverAffectNavigation(false);
    Capsule->CanCharacterStepUpOn = ECB_No;
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);
    Tags.AddUnique(TEXT("DashHardBlocker"));

    DamageReceiver = CreateDefaultSubobject<UGirlSaviorDamageReceiverComponent>(TEXT("DamageReceiver"));
    Attack = CreateDefaultSubobject<UGirlSaviorEnemyAttackComponent>(TEXT("Attack"));
    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(Capsule);
    BodyVisual->SetRelativeLocation(FVector(0, 0, -8));
    BodyVisual->SetRelativeScale3D(FVector(0.64, 0.64, 1.5));
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetGenerateOverlapEvents(false);
    BodyVisual->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Placeholder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Placeholder.Succeeded())
    {
        BodyVisual->SetStaticMesh(Placeholder.Object);
    }

    AttackVisual = CreateDefaultSubobject<ULineBatchComponent>(TEXT("AttackVisual"));
    AttackVisual->SetupAttachment(Capsule);
    AttackVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackVisual->SetCanEverAffectNavigation(false);
    AttackVisual->PrimaryComponentTick.bStartWithTickEnabled = false;
}

// This registration path also works for a deferred spawn after its definition and encounter are assigned.
bool AGirlSaviorEnemyCharacter::InitializeForEncounter(AGirlSaviorCombatEncounter& InEncounter)
{
    if (bRetired || (Encounter && Encounter != &InEncounter))
    {
        return false;
    }
    if (bEncounterInitialized)
    {
        return DamageReceiver->IsAlive();
    }
    if (!Definition || !Definition->IsValidForSpawn() || !AttackVisual->IsRegistered())
    {
        UE_LOG(LogGirlSaviorEnemy, Warning, TEXT("%s cannot activate without valid enemy tuning and a warning visual."), *GetName());
        return false;
    }
    if (!GetController())
    {
        SpawnDefaultController();
    }
    AGirlSaviorEnemyAIController* AI = Cast<AGirlSaviorEnemyAIController>(GetController());
    if (!AI || !DamageReceiver->InitializeVitals(Definition->Vitals, EGirlSaviorFaction::Enemy))
    {
        UE_LOG(LogGirlSaviorEnemy, Warning, TEXT("%s cannot activate without AI possession and valid vitals."), *GetName());
        return false;
    }

    Encounter = &InEncounter;
    GetCapsuleComponent()->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
    GetCharacterMovement()->MaxWalkSpeed = Definition->WalkSpeed;
    DamageReceiver->OnDied.AddUObject(this, &AGirlSaviorEnemyCharacter::HandleDeath);
    bEncounterInitialized = true;
    Attack->ActivateForEncounter(InEncounter, *Definition, *AttackVisual);
    AI->ActivateForEncounter(InEncounter);
    return true;
}

bool AGirlSaviorEnemyCharacter::IsInFixtureRoom(const FVector& Location) const
{
    if (!FixtureRoomBounds)
    {
        return true;
    }
    const auto* Box = CastChecked<UBoxComponent>(FixtureRoomBounds->GetCollisionComponent());
    const FVector Extent = Box->GetScaledBoxExtent();
    const FVector Local = Box->GetComponentTransform().InverseTransformPositionNoScale(Location);
    return FBox(-Extent, Extent).IsInsideOrOn(Local);
}

// Stop live behavior for death and terminal cleanup while the actor can still present cues.
void AGirlSaviorEnemyCharacter::StopForEncounter()
{
    bRetired = true;
    if (AGirlSaviorEnemyAIController* AI = Cast<AGirlSaviorEnemyAIController>(GetController()))
    {
        AI->StopForEncounter();
    }
    Attack->StopForEncounter();
    GetCharacterMovement()->DisableMovement();
}

void AGirlSaviorEnemyCharacter::HandleDeath(const FGirlSaviorDamageResult& Result)
{
    StopForEncounter();
    SetActorEnableCollision(false);
    Tags.Remove(TEXT("DashHardBlocker"));
    if (IsValid(Encounter))
    {
        Encounter->UnregisterEnemy(this);
    }

    // The prototype corpse stays visible briefly and never obstructs the following shot.
    BodyVisual->SetRelativeRotation(FRotator(85, 0, 0));
    BodyVisual->SetRelativeLocation(FVector(0, 0, -52));
    SetLifeSpan(6.f);
    UE_LOG(LogGirlSaviorEnemy, Log, TEXT("%s died from %s."), *GetName(), *Result.Spec.SourceId.ToString());
}

void AGirlSaviorEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DamageReceiver->OnDied.RemoveAll(this);
    if (bEncounterInitialized && IsValid(Encounter))
    {
        Encounter->UnregisterEnemy(this);
    }
    Super::EndPlay(EndPlayReason);
}
