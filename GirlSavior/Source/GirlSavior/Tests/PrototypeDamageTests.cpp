#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorDamageResolutionTest, "GirlSavior.Combat.DamageResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Use fresh receivers for each case so one hit cannot seed the next case.
bool FGirlSaviorDamageResolutionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Damage test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	const auto AddReceiver = [World]()
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UGirlSaviorDamageReceiverComponent* Receiver = NewObject<UGirlSaviorDamageReceiverComponent>(Actor);
		Actor->AddInstanceComponent(Receiver);
		Receiver->RegisterComponent();
		return Receiver;
	};

	FGirlSaviorDamageSpec Hit;
	Hit.Delivery = EGirlSaviorDamageDelivery::DirectProjectile;
	Hit.SourceFaction = EGirlSaviorFaction::Enemy;
	Hit.SourceId = TEXT("Enemy.Pursuer.Melee");
	Hit.Cause = FText::FromString(TEXT("Test lethal cause"));

	struct FBarrierCase
	{
		float Incoming;
		double ExpectedBarrier;
		double ExpectedHealth;
		bool bExpectedBreak;
	};
	const FBarrierCase BarrierCases[] = {{3, 1, 100, false}, {4, 0, 100, true}, {7, 0, 97, true}};
	for (const FBarrierCase& Case : BarrierCases)
	{
		auto* Receiver = AddReceiver();
		FGirlSaviorVitals Vitals;
		Vitals.InitialBarrier = 4.f;
		TestTrue(TEXT("Initialize hero with barrier"), Receiver->InitializeVitals(Vitals, EGirlSaviorFaction::Hero));
		Hit.Physical = Case.Incoming;
		const FGirlSaviorDamageResult Result = Receiver->ResolveDamage(Hit);
		TestTrue(TEXT("Barrier hit applied"), Result.Outcome == EGirlSaviorDamageOutcome::Applied);
		TestEqual(TEXT("Barrier absorbs adjusted damage first"), Result.BarrierAfter, Case.ExpectedBarrier);
		TestEqual(TEXT("Only overflow reaches health"), Result.HealthAfter, Case.ExpectedHealth);
		TestEqual(TEXT("Barrier break reports the crossing"), Result.bBarrierBroken, Case.bExpectedBreak);
		TestEqual(TEXT("Snapshot and live health agree"), Result.HealthAfter, Receiver->GetHealth());
	}

	auto* Resistant = AddReceiver();
	FGirlSaviorVitals Resistances;
	Resistances.InitialBarrier = 4.f;
	Resistances.PhysicalResistance = 0.1f;
	Resistances.MagicalResistance = 0.2f;
	TestTrue(TEXT("Initialize mixed resistances"), Resistant->InitializeVitals(Resistances, EGirlSaviorFaction::Hero));
	Hit.Physical = 10.f;
	Hit.Magical = 10.f;
	const FGirlSaviorDamageResult Mixed = Resistant->ResolveDamage(Hit);
	TestTrue(TEXT("Physical and magic are reduced independently before barrier"),
		FMath::IsNearlyEqual(Mixed.HealthLost, 13.0, 0.000001) && Mixed.BarrierLost == 4.0);

	auto* Immune = AddReceiver();
	Resistances.PhysicalResistance = 1.f;
	Resistances.MagicalResistance = 1.f;
	TestTrue(TEXT("Full resistance is valid"), Immune->InitializeVitals(Resistances, EGirlSaviorFaction::Hero));
	const FGirlSaviorDamageResult Zero = Immune->ResolveDamage(Hit);
	TestTrue(TEXT("A fully resisted hit is applied with zero loss"),
		Zero.Outcome == EGirlSaviorDamageOutcome::Applied && Zero.HealthLost == 0 && Zero.BarrierLost == 0);

	auto* Victim = AddReceiver();
	FGirlSaviorVitals LowVitals;
	LowVitals.MaxHealth = 4.f;
	LowVitals.InitialBarrier = 4.f;
	FGirlSaviorVitals BrokenSettings = LowVitals;
	BrokenSettings.MagicalResistance = 1.1f;
	TestFalse(TEXT("Reject out-of-range defense tuning"), Victim->InitializeVitals(BrokenSettings, EGirlSaviorFaction::Hero));
	TestTrue(TEXT("Initialize lethal overflow victim"), Victim->InitializeVitals(LowVitals, EGirlSaviorFaction::Hero));
	TestFalse(TEXT("Reject reinitialization"), Victim->InitializeVitals(LowVitals, EGirlSaviorFaction::Hero));
	int32 AdmissionCalls = 0;
	Victim->DamageAdmission.BindLambda([&](const FGirlSaviorDamageSpec&)
	{
		++AdmissionCalls;
		if (AdmissionCalls == 1)
		{
			TestTrue(TEXT("A synchronous nested hit on a living receiver is rejected"),
				Victim->ResolveDamage(Hit).Outcome == EGirlSaviorDamageOutcome::Rejected);
		}
		return EGirlSaviorDamageOutcome::BlockedBySafeArea;
	});

	const auto InvalidHit = [&](const TCHAR* Label, float Physical, float Magical)
	{
		FGirlSaviorDamageSpec Invalid = Hit;
		Invalid.Physical = Physical;
		Invalid.Magical = Magical;
		TestTrue(Label, Victim->ResolveDamage(Invalid).Outcome == EGirlSaviorDamageOutcome::Rejected);
	};
	InvalidHit(TEXT("Reject zero total"), 0.f, 0.f);
	InvalidHit(TEXT("Reject negative portion despite positive total"), -1.f, 10.f);
	InvalidHit(TEXT("Reject NaN"), std::numeric_limits<float>::quiet_NaN(), 1.f);
	InvalidHit(TEXT("Reject infinity"), 1.f, std::numeric_limits<float>::infinity());
	FGirlSaviorDamageSpec WrongDelivery = Hit;
	WrongDelivery.Delivery = static_cast<EGirlSaviorDamageDelivery>(255);
	TestTrue(TEXT("Reject unknown delivery"), Victim->ResolveDamage(WrongDelivery).Outcome == EGirlSaviorDamageOutcome::Rejected);
	FGirlSaviorDamageSpec Friendly = Hit;
	Friendly.SourceFaction = EGirlSaviorFaction::Hero;
	TestTrue(TEXT("Reject same-faction fire"), Victim->ResolveDamage(Friendly).Outcome == EGirlSaviorDamageOutcome::Rejected);
	FGirlSaviorDamageSpec Self = Hit;
	Self.SourceFaction = EGirlSaviorFaction::Neutral;
	Self.Instigator = Victim->GetOwner();
	TestTrue(TEXT("Reject a neutral self hit"), Victim->ResolveDamage(Self).Outcome == EGirlSaviorDamageOutcome::Rejected);
	TestEqual(TEXT("Validation precedes boundary queries"), AdmissionCalls, 0);
	const FGirlSaviorDamageResult Safe = Victim->ResolveDamage(Hit);
	TestTrue(TEXT("Safe-boundary protection preserves both vitals"), Safe.Outcome == EGirlSaviorDamageOutcome::BlockedBySafeArea
		&& Safe.HealthAfter == 4.0 && Safe.BarrierAfter == 4.0);
	TestEqual(TEXT("One boundary query per valid hit"), AdmissionCalls, 1);
	Victim->DamageAdmission.Unbind();

	AActor* Source = World->SpawnActor<AActor>();
	Hit.Instigator = Source;
	Hit.Magical = 0.f;
	Hit.Delivery = EGirlSaviorDamageDelivery::DirectMelee;
	int32 DeathCount = 0;
	Victim->OnDied.AddLambda([&](const FGirlSaviorDamageResult& LiveResult)
	{
		++DeathCount;
		TestTrue(TEXT("Lethal state is committed before notifying death"), !Victim->IsAlive()
			&& Victim->GetHealth() == 0 && Victim->GetLastLethalResult().bKilled && LiveResult.bKilled);
		TestTrue(TEXT("Nested death hit is rejected"), Victim->ResolveDamage(Hit).Outcome == EGirlSaviorDamageOutcome::Rejected);
	});
	const FGirlSaviorDamageResult Lethal = Victim->ResolveDamage(Hit);
	TestTrue(TEXT("Lethal overflow clamps both actual losses"), Lethal.bKilled && Lethal.bBarrierBroken
		&& Lethal.HealthLost == 4.0 && Lethal.BarrierLost == 4.0);
	TestTrue(TEXT("Reject any later hit on the corpse"), Victim->ResolveDamage(Hit).Outcome == EGirlSaviorDamageOutcome::Rejected);
	TestEqual(TEXT("Structural death fires once"), DeathCount, 1);
	Source->Destroy();
	const FGirlSaviorDamageResult& Snapshot = Victim->GetLastLethalResult();
	TestTrue(TEXT("Lethal cause remains readable after the attacker is destroyed"),
		Snapshot.Spec.SourceId == Hit.SourceId && Snapshot.Spec.Cause.EqualTo(Hit.Cause) && !Snapshot.Spec.Instigator.IsValid());

	auto* Fixture = AddReceiver();
	TestTrue(TEXT("Stage 4 legacy API accepts the Demo projectile"), Fixture->TryApplyDamage(EGirlSaviorDamageKind::DirectProjectile, 1.f));
	TestEqual(TEXT("Stage 4 fixture still accumulates the original amount"), Fixture->AcceptedDamageTotal, 1.0);
	auto* LargeHitTarget = AddReceiver();
	TestTrue(TEXT("Initialize target for finite large portions"), LargeHitTarget->InitializeVitals(LowVitals, EGirlSaviorFaction::Hero));
	Hit.Physical = Hit.Magical = std::numeric_limits<float>::max();
	const FGirlSaviorDamageResult Large = LargeHitTarget->ResolveDamage(Hit);
	TestTrue(TEXT("Double-precision total preserves finite large portions and clamps loss"),
		Large.Outcome == EGirlSaviorDamageOutcome::Applied && Large.HealthLost == 4.0 && Large.BarrierLost == 4.0);
	return true;
}

#endif
