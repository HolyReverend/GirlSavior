#if WITH_EDITOR

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
// Exercise real receiver and registry cleanup while isolating the scheduling timings from aim.
void ExerciseWaveFixture(const TArray<FString>& Args, UWorld* World)
{
    if (!World || World->WorldType != EWorldType::PIE || Args.Num() != 1)
    {
        return;
    }
    TActorIterator<AGirlSaviorCombatEncounter> EncounterIt(World);
    AGirlSaviorCombatEncounter* Encounter = EncounterIt ? *EncounterIt : nullptr;
    if (!Encounter || !Encounter->bWavesEnabled || !Encounter->GetHero()
        || Encounter->GetRunState() != EGirlSaviorRunState::Active)
    {
        return;
    }

    FGirlSaviorDamageSpec Spec;
    Spec.Delivery = EGirlSaviorDamageDelivery::Area;
    Spec.Physical = 1000.f;
    Spec.SourceId = TEXT("CP3.PIE.Fixture");
    if (Args[0] == TEXT("Defeat"))
    {
        Spec.SourceFaction = EGirlSaviorFaction::Enemy;
        Encounter->GetHero()->GetDamageReceiver()->ResolveDamage(Spec);
    }
    else if (Args[0] == TEXT("Clear"))
    {
        Spec.SourceFaction = EGirlSaviorFaction::Hero;
        for (TActorIterator<AGirlSaviorEnemyCharacter> It(World); It; ++It)
        {
            if (It->GetDamageReceiver()->IsAlive())
            {
                It->GetDamageReceiver()->ResolveDamage(Spec);
            }
        }
    }
    else if (Args[0] == TEXT("SafeBoundary"))
    {
        // Check the open entrance from the combat side with each projectile's real object channel.
        for (const auto Channel : {GirlSaviorCombatChannels::HeroProjectile, GirlSaviorCombatChannels::EnemyProjectile})
        {
            FHitResult Hit;
            FCollisionResponseParams Responses;
            Responses.CollisionResponse.SetAllChannels(ECR_Ignore);
            Responses.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
            Responses.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
            const bool bBlocked = World->SweepSingleByChannel(Hit, FVector(-700, 0, 90), FVector(-1020, 0, 90),
                FQuat::Identity, Channel, FCollisionShape::MakeSphere(6.f), FCollisionQueryParams::DefaultQueryParam,
                Responses);
            UE_LOG(LogTemp, Display, TEXT("CP3 safe projectile channel=%d blocked=%d actor=%s"),
                static_cast<int32>(Channel), bBlocked, *GetNameSafe(Hit.GetActor()));
        }
    }
}

FAutoConsoleCommandWithWorldAndArgs WaveFixtureCommand(TEXT("GirlSavior.TestWaveFixture"),
    TEXT("Exercise actual CP3 cleanup in PIE: Clear, Defeat, or SafeBoundary."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExerciseWaveFixture));
}

#endif
