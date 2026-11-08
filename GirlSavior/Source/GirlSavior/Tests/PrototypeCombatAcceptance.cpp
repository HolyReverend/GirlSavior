#if WITH_EDITOR
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorSafeArea.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <limits>

namespace
{
// Fixtures use runtime components and leave all authored actors untouched.
UBoxComponent* AddCombatBox(AActor* Actor, const FVector& Extent, ECollisionChannel ObjectType)
{
 UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
 Actor->AddInstanceComponent(Box);
 Actor->SetRootComponent(Box);
 Box->InitBoxExtent(Extent);
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Box->SetCollisionObjectType(ObjectType);
 Box->SetCollisionResponseToAllChannels(ECR_Ignore);
 Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
 Box->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
 Box->RegisterComponent();
 return Box;
}

// Run only against the live PIE controller; collision lanes are isolated above Demo.
void RunCombatAcceptance(const TArray<FString>& Args, UWorld* World)
{
 FString Lines = TEXT("Stage 4 native combat probes. Temporary high-Z fixtures; no physical input synthesized.\n");
 int32 Checks = 0, Failures = 0;
 auto Check = [&](bool bPass, const TCHAR* Label)
 {
  ++Checks;
  Failures += !bPass;
  Lines += FString::Printf(TEXT("%03d %s %s\n"), Checks, bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
 };
 AGirlSaviorPlayerController* PC = World ? Cast<AGirlSaviorPlayerController>(World->GetFirstPlayerController()) : nullptr;
 TArray<AActor*> Fixtures;
 if (!World || World->WorldType != EWorldType::PIE || !PC || !PC->GetPawn())
 {
  Check(false, TEXT("requires Demo PIE with possessed GirlSavior controller"));
 }
 else
 {
  FActorSpawnParameters Spawn;
  Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto MakeBox = [&](FVector Location, bool bPawn) -> AActor*
  {
   AActor* Actor = bPawn ? static_cast<AActor*>(World->SpawnActor<APawn>(APawn::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn))
    : World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
   if (Actor)
   {
    Fixtures.Add(Actor);
    AddCombatBox(Actor, FVector(20, 60, 60), ECC_WorldDynamic);
    Actor->SetActorLocation(Location);
   }
   return Actor;
  };
  auto AddReceiver = [&](AActor* Actor) -> UGirlSaviorDamageReceiverComponent*
  {
   if (!Actor) return nullptr;
   auto* Receiver = NewObject<UGirlSaviorDamageReceiverComponent>(Actor);
   Actor->AddInstanceComponent(Receiver);
   Receiver->RegisterComponent();
   return Receiver;
  };
  auto Launch = [&](FVector Location, double Yaw, APawn* Owner) -> AGirlSaviorTestProjectile*
  {
   FActorSpawnParameters Params = Spawn;
   Params.Owner = Owner;
   Params.Instigator = Owner;
   auto* Projectile = World->SpawnActor<AGirlSaviorTestProjectile>(AGirlSaviorTestProjectile::StaticClass(), Location, FRotator(0, Yaw, 0), Params);
   if (Projectile) Fixtures.Add(Projectile);
   return Projectile;
  };

  AActor* ValidationTarget = MakeBox(FVector(0, 0, 100000), false);
  auto* Receiver = AddReceiver(ValidationTarget);
  Check(Receiver != nullptr, TEXT("damage receiver fixture created"));
  if (Receiver)
  {
   for (float Amount : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
   {
    Check(!Receiver->TryApplyDamage(EGirlSaviorDamageKind::DirectProjectile, Amount), TEXT("invalid amount rejected"));
   }
   Check(Receiver->AcceptedDamageTotal == 0.0, TEXT("rejected damage leaves total unchanged"));
   Check(Receiver->TryApplyDamage(EGirlSaviorDamageKind::DirectProjectile, 1.0f)
    && Receiver->AcceptedDamageTotal == 1.0, TEXT("positive direct projectile damage accepted exactly once"));
  }

  // Owner occupies the launch point and blocks this channel: ignoring it is required to reach the target.
  const FVector Origin(0, 1000, 100000);
  APawn* Owner = Cast<APawn>(MakeBox(Origin, true));
  AActor* Target = MakeBox(Origin + FVector(300, 0, 0), false);
  auto* TargetReceiver = AddReceiver(Target);
  auto* Projectile = Launch(Origin, 0, Owner);
  Check(Owner && TargetReceiver && Projectile, TEXT("owner and direct-hit lane created"));
  if (Owner && TargetReceiver && Projectile)
  {
   Check(Projectile->CollisionSphere->GetMoveIgnoreActors().Contains(Owner), TEXT("BeginPlay ignores shooter actor"));
   Check(FMath::IsNearlyEqual(Projectile->GetLifeSpan(), 1.5f, .01f), TEXT("empty-space lifetime starts at 1.5 seconds"));
   Check(Projectile->ProjectileMovement->Velocity.Equals(FVector(2000, 0, 0), .01), TEXT("local +X launch initializes 2000 cm/s"));
   Projectile->ProjectileMovement->TickComponent(.2f, LEVELTICK_All, nullptr);
   Check(Projectile->IsActorBeingDestroyed(), TEXT("first swept target impact destroys projectile"));
   Check(TargetReceiver->AcceptedDamageTotal == 1.0, TEXT("real swept hit delivers one direct damage"));
  }

  const FVector ShieldOrigin(0, 2000, 100000);
  AActor* Wall = MakeBox(ShieldOrigin + FVector(120, 0, 0), false);
  AActor* ShieldedTarget = MakeBox(ShieldOrigin + FVector(300, 0, 0), false);
  auto* ShieldedReceiver = AddReceiver(ShieldedTarget);
  auto* Blocked = Launch(ShieldOrigin, 0, Owner);
  Check(Wall && ShieldedReceiver && Blocked, TEXT("wall shielding lane created"));
  if (Wall && ShieldedReceiver && Blocked)
  {
   Blocked->ProjectileMovement->TickComponent(.2f, LEVELTICK_All, nullptr);
   Check(Blocked->IsActorBeingDestroyed(), TEXT("first obstacle ends swept projectile travel"));
   Check(ShieldedReceiver->AcceptedDamageTotal == 0.0, TEXT("first blocker shields receiver behind wall"));
  }

  auto* Turned = Launch(FVector(0, 3000, 100000), 90, Owner);
  Check(Turned != nullptr, TEXT("yaw launch fixture created"));
  if (Turned)
  {
   Check(Turned->ProjectileMovement->Velocity.Equals(FVector(0, 2000, 0), .01), TEXT("spawn yaw rotates local initial velocity into world direction"));
   const FVector Before = Turned->GetActorLocation();
   Turned->ProjectileMovement->TickComponent(.05f, LEVELTICK_All, nullptr);
   Check((Turned->GetActorLocation() - Before).Equals(FVector(0, 100, 0), .1), TEXT("unobstructed projectile travels at expected horizontal speed"));
  }

  // Exercise saved Blueprint composition and map collision, not just native temporary fixtures.
  AActor* AuthoredTarget = nullptr;
  AActor* AuthoredWall = nullptr;
  for (TActorIterator<AActor> It(World); It; ++It)
  {
   if (It->GetActorLabel() == TEXT("Stage4_TestTarget")) AuthoredTarget = *It;
   if (It->GetActorLabel() == TEXT("Stage4_BlockingWall")) AuthoredWall = *It;
  }
  UClass* BlueprintProjectile = LoadClass<AGirlSaviorTestProjectile>(nullptr,
   TEXT("/Game/Blueprints/Core/BP_TestProjectile.BP_TestProjectile_C"));
  Check(AuthoredTarget && AuthoredWall && BlueprintProjectile, TEXT("saved Blueprint projectile and Demo lane fixtures resolve"));
  for (AActor* Destination : {AuthoredTarget, AuthoredWall})
  {
   if (!Destination || !BlueprintProjectile) continue;
   const FVector Start(0, 0, 90);
   const FVector AimPoint(Destination->GetActorLocation().X, Destination->GetActorLocation().Y, 90);
   const auto* Primitive = Destination->FindComponentByClass<UPrimitiveComponent>();
   Check(Primitive && Primitive->GetCollisionResponseToChannel(ECC_GameTraceChannel2) == ECR_Block,
    TEXT("authored fixture blocks projectile channel after PIE instancing"));
   auto* Damage = Destination->FindComponentByClass<UGirlSaviorDamageReceiverComponent>();
   const double BeforeDamage = Damage ? Damage->AcceptedDamageTotal : 0.;
   FActorSpawnParameters Params = Spawn;
   Params.Owner = PC->GetPawn();
   Params.Instigator = PC->GetPawn();
   auto* Shot = World->SpawnActor<AGirlSaviorTestProjectile>(BlueprintProjectile, Start,
    (AimPoint - Start).Rotation(), Params);
   Check(Shot != nullptr, TEXT("authored Blueprint projectile spawns"));
   if (Shot)
   {
    Fixtures.Add(Shot);
    Shot->ProjectileMovement->TickComponent(.25f, LEVELTICK_All, nullptr);
    Check(Shot->IsActorBeingDestroyed(), TEXT("saved Blueprint projectile stops on authored fixture"));
    if (Damage) Check(Damage->AcceptedDamageTotal == BeforeDamage + 1., TEXT("authored target receives one typed damage"));
   }
  }

  // A false baseline may include authored safe areas; leave that live state untouched.
  if (PC->IsFireAllowed() && ValidationTarget && Wall)
  {
   PC->SetFireAllowed(false);
   Check(!PC->IsFireAllowed(), TEXT("explicit combat permission disables firing"));
   PC->SetFireAllowed(true);
   Check(PC->IsFireAllowed(), TEXT("explicit combat permission restores firing"));
   PC->SetSafeAreaActive(ValidationTarget, true);
   PC->SetSafeAreaActive(Wall, true);
   PC->SetSafeAreaActive(ValidationTarget, true);
   Check(!PC->IsFireAllowed(), TEXT("safe-area tokens disable firing idempotently"));
   PC->SetSafeAreaActive(ValidationTarget, false);
   Check(!PC->IsFireAllowed(), TEXT("leaving one overlapping area remains disallowed"));
   PC->SetSafeAreaActive(Wall, false);
   Check(PC->IsFireAllowed(), TEXT("leaving final safe area restores permission"));
  }
  else Lines += TEXT("NotRun: permission mutation probes (baseline disallowed or fixture unavailable).\n");
  // Possessing a pawn already inside a real volume must synchronize both registries.
  if (PC->IsFireAllowed())
  {
   APawn* SavedPawn = PC->GetPawn();
   AActor* SavedViewTarget = PC->GetViewTarget();
   const FVector ZoneCenter(0, 5000, 100000);
   auto* Zone = World->SpawnActor<AGirlSaviorSafeArea>(AGirlSaviorSafeArea::StaticClass(), ZoneCenter, FRotator::ZeroRotator, Spawn);
   auto* TestHero = World->SpawnActor<AGirlSaviorCharacter>(AGirlSaviorCharacter::StaticClass(), ZoneCenter + FVector(1000, 0, 0), FRotator::ZeroRotator, Spawn);
   if (Zone) Fixtures.Add(Zone);
   if (TestHero) Fixtures.Add(TestHero);
   Check(Zone && TestHero && !TestHero->GetController(), TEXT("real safe volume and unpossessed hero created"));
   if (Zone && TestHero)
   {
    TestHero->SetActorLocation(ZoneCenter, false, nullptr, ETeleportType::TeleportPhysics);
    TestHero->GetCapsuleComponent()->UpdateOverlaps();
    Zone->SafeVolume->UpdateOverlaps();
    Check(Zone->SafeVolume->IsOverlappingActor(TestHero), TEXT("unpossessed hero overlaps real safe volume before possession"));
    PC->UnPossess();
    PC->Possess(TestHero);
    Check(PC->GetPawn() == TestHero && !PC->IsFireAllowed(), TEXT("possession inside safe volume disables firing"));
    TestHero->SetActorLocation(ZoneCenter + FVector(1000, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
    TestHero->GetCapsuleComponent()->UpdateOverlaps();
    Zone->SafeVolume->UpdateOverlaps();
    Check(!Zone->SafeVolume->IsOverlappingActor(TestHero) && PC->IsFireAllowed(), TEXT("leaving real safe volume clears possession-time registry token"));
    PC->UnPossess();
    PC->Possess(SavedPawn);
    PC->SetViewTarget(SavedViewTarget);
    Check(PC->GetPawn() == SavedPawn && PC->GetViewTarget() == SavedViewTarget, TEXT("original possession and view target restored"));
   }
  }
  else Lines += TEXT("NotRun: actual safe-volume possession probe (baseline fire permission disallowed).\n");
 }
 for (AActor* Fixture : Fixtures)
 {
  if (IsValid(Fixture) && !Fixture->IsActorBeingDestroyed()) Fixture->Destroy();
 }
 Lines += FString::Printf(TEXT("TOTAL checks=%d failures=%d\nNotRun: Blueprint visual cues, physical input/focus, automatic lifetime expiry, manual safe-volume traversal, scheduling and manual firing scenario.\n"), Checks, Failures);
 const FString Directory = FPaths::ProjectSavedDir() / TEXT("Stage4");
 IFileManager::Get().MakeDirectory(*Directory, true);
 const FString Path = Directory / TEXT("CombatAcceptance.txt");
 const bool bSaved = FFileHelper::SaveStringToFile(Lines, *Path);
 UE_LOG(LogTemp, Display, TEXT("Combat acceptance: %d checks, %d failures; saved=%d: %s"), Checks, Failures, bSaved, *Path);
}

FAutoConsoleCommandWithWorldAndArgs CombatAcceptanceCommand(TEXT("GirlSavior.TestCombat"),
 TEXT("Run native Stage 4 collision, damage, and permission probes in Demo PIE."),
 FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCombatAcceptance));
}
#endif