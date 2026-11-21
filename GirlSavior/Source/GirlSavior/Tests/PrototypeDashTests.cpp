#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorDashSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

struct FGirlSaviorDashTestAccess
{
    static constexpr float Duration = 0.22f;
    static constexpr double Distance = 300.0;

    struct FCommittedPath
    {
        double HorizontalTravel = 0.0;
        double SimulatedSeconds = 0.0;
        bool bCommitSucceeded = true;
        bool bStoppedByGeometry = false;
        bool bGrounded = false;
    };

    // Query the same path as dash admission, with enough budget to measure its full travel.
    static double Probe(const UGirlSaviorCharacterMovementComponent& Movement, const FVector& Direction)
    {
        return Movement.ProbeDashPath(Direction.GetSafeNormal2D() * (Distance / Duration), Duration,
            Distance + 1.0);
    }

    // Apply the approved sweeps to the actual capsule in fixed frame-sized batches.
    static FCommittedPath Commit(UGirlSaviorCharacterMovementComponent& Movement, const FVector& Direction,
        int32 FrameRate)
    {
        FCommittedPath Path;
        FVector Velocity = Direction.GetSafeNormal2D() * (Distance / Duration);
        bool bSliding = false;
        const double FrameDuration = 1.0 / FrameRate;
        for (int32 Frame = 1; Path.SimulatedSeconds < Duration - UE_SMALL_NUMBER && Frame <= 16; ++Frame)
        {
            const double FrameEnd = FMath::Min(Frame * FrameDuration, double(Duration));
            for (int32 Index = 0; Path.SimulatedSeconds < FrameEnd - UE_SMALL_NUMBER && Index < 8; ++Index)
            {
                const FVector Start = Movement.UpdatedComponent->GetComponentLocation();
                const auto Leg = Movement.QueryDashLeg(Start, Velocity, FrameEnd - Path.SimulatedSeconds, bSliding);
                FHitResult UnexpectedContact;
                FScopedMovementUpdate Scope(Movement.UpdatedComponent, EScopedUpdate::DeferredUpdates);
                if (!Movement.CommitDashLeg(Leg, UnexpectedContact, Scope) || UnexpectedContact.bBlockingHit)
                {
                    Path.bCommitSucceeded = false;
                    return Path;
                }
                Path.HorizontalTravel += FVector::Dist2D(Start, Movement.UpdatedComponent->GetComponentLocation());
                Path.SimulatedSeconds += Leg.Time;
                Velocity = Leg.NextVelocity;
                bSliding = Leg.bSliding;
                if (Leg.PathEnd != UGirlSaviorCharacterMovementComponent::EDashPathEnd::Continue)
                {
                    Path.bStoppedByGeometry = true;
                    break;
                }
            }
            if (Path.bStoppedByGeometry)
            {
                break;
            }
        }
        FFindFloorResult Floor;
        Path.bGrounded = Movement.FindDashFloor(Movement.UpdatedComponent->GetComponentLocation(), Floor);
        return Path;
    }

    // Begin from accepted settings to exercise the live clock queries in the isolated world.
    static void CheckProtection(FAutomationTestBase& Test, UGirlSaviorCharacterMovementComponent& Movement)
    {
        Movement.DashSettings = NewObject<UGirlSaviorDashSettings>(&Movement);
        const auto& Settings = *Movement.DashSettings;
        Movement.BeginDash(FVector::ForwardVector * (double(Settings.Distance) / Settings.Duration));
        Test.TestTrue(TEXT("Accepted dash starts with protection"), Movement.IsDashing() && Movement.IsDashProtected());
        const double InitialCooldown = Movement.GetDashCooldownRemaining();
        Test.TestTrue(TEXT("Cooldown starts at the asset duration"),
            FMath::IsNearlyEqual(InitialCooldown, double(Settings.Cooldown)));

        const double ProtectionDeadline = Movement.DashProtectionEndTime;
        Movement.DashProtectionEndTime = Movement.GetWorld()->GetTimeSeconds();
        Test.TestTrue(TEXT("At its exact deadline, protection ends while movement remains active"),
            Movement.IsDashing() && !Movement.IsDashProtected());
        Movement.DashProtectionEndTime = ProtectionDeadline;
        Movement.CancelDash();
        Test.TestTrue(TEXT("Early cancellation removes protection and restores walking"),
            !Movement.IsDashing() && !Movement.IsDashProtected() && Movement.IsMovingOnGround());
        Test.TestEqual(TEXT("Early cancellation keeps the original cooldown"),
            Movement.GetDashCooldownRemaining(), InitialCooldown);
    }
};

namespace
{
    // Keep each test lane independent of saved maps, Blueprint defaults, and viewport input.
    AActor* AddDashTestBlock(UWorld& World, const FVector& Center, const FVector& Extent)
    {
        AActor* Actor = World.SpawnActor<AActor>();
        if (!Actor)
        {
            return nullptr;
        }

        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->SetWorldLocation(Center);
        Box->RegisterComponent();
        return Actor;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorDashGroundPathTest, "GirlSavior.Combat.Dash.GroundPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Check walkable-path decisions without advancing the world or requiring a focused viewport.
bool FGirlSaviorDashGroundPathTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Collision test world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    const AActor* MainFloor = AddDashTestBlock(*World, FVector(0, 0, -20), FVector(700, 1500, 20));
    AActor* Wall = AddDashTestBlock(*World, FVector(64, -750, 100), FVector(10, 250, 100));
    const AActor* Step = AddDashTestBlock(*World, FVector(315, 750, 10), FVector(175, 160, 10));
    const AActor* Edge = AddDashTestBlock(*World, FVector(-200, 2000, -20), FVector(300, 200, 20));
    if (!TestNotNull(TEXT("Main floor"), MainFloor) || !TestNotNull(TEXT("Wall"), Wall)
        || !TestNotNull(TEXT("Walkable step"), Step) || !TestNotNull(TEXT("Separate floor edge"), Edge))
    {
        return false;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGirlSaviorCharacter* Hero = World->SpawnActor<AGirlSaviorCharacter>(FVector(0, 0, 90),
        FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("Native hero"), Hero))
    {
        return false;
    }
    auto& Movement = *CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
    Movement.SetMovementMode(MOVE_Walking);

    auto CheckPath = [&](const TCHAR* Label, const FVector& Start, const FVector& Direction,
        double Minimum, double Maximum)
    {
        Hero->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
        const FTransform Before = Hero->GetCapsuleComponent()->GetComponentTransform();
        const FVector BeforeVelocity = Movement.Velocity;
        const EMovementMode BeforeMode = Movement.MovementMode;
        const double Distance = FGirlSaviorDashTestAccess::Probe(Movement, Direction);
        TestTrue(FString::Printf(TEXT("%s: %.3f cm in [%.2f, %.2f]"), Label, Distance, Minimum, Maximum),
            FMath::IsFinite(Distance) && Distance >= Minimum && Distance <= Maximum);
        TestTrue(FString::Printf(TEXT("%s: probe preserves capsule transform"), Label),
            Hero->GetCapsuleComponent()->GetComponentTransform().Equals(Before));
        TestTrue(FString::Printf(TEXT("%s: probe preserves movement state"), Label),
            Movement.MovementMode == BeforeMode && Movement.Velocity.Equals(BeforeVelocity));
    };

    CheckPath(TEXT("Straight on open floor"), FVector(0, 0, 90), FVector::ForwardVector, 299.8, 300.2);
    CheckPath(TEXT("Diagonal on open floor"), FVector(0, 0, 90), FVector(1, 1, 0), 299.8, 300.2);
    CheckPath(TEXT("Frontal wall leaves less than admission distance"), FVector(0, -750, 90),
        FVector::ForwardVector, 15.0, 22.0);
    CheckPath(TEXT("Ordinary wall preserves tangential movement"), FVector(0, -750, 90),
        FVector(1, 1, 0), 180.0, 260.0);
    Wall->Tags.Add(FName(TEXT("DashHardBlocker")));
    CheckPath(TEXT("Hard blocker prevents tangential movement"), FVector(0, -750, 90),
        FVector(1, 1, 0), 22.0, 29.9);
    CheckPath(TEXT("Walkable step preserves full horizontal distance"), FVector(0, 750, 90),
        FVector::ForwardVector, 299.8, 300.2);
    CheckPath(TEXT("Floor edge limits travel to supported poses"), FVector(0, 2000, 90),
        FVector::ForwardVector, 70.0, 134.0);

    auto CheckCommittedPath = [&](const TCHAR* Label, const FVector& Start, int32 FrameRate, double MinTravel,
        double MaxTravel, double ExpectedRise, bool bExpectStop)
    {
        Hero->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
        const auto Path = FGirlSaviorDashTestAccess::Commit(Movement, FVector::ForwardVector, FrameRate);
        const double Rise = Hero->GetActorLocation().Z - Start.Z;
        const FString Case = FString::Printf(TEXT("%s at %d Hz"), Label, FrameRate);
        TestTrue(Case + TEXT(": committed on supported floor"), Path.bCommitSucceeded && Path.bGrounded);
        TestTrue(FString::Printf(TEXT("%s: traveled %.3f cm"), *Case, Path.HorizontalTravel),
            Path.HorizontalTravel >= MinTravel && Path.HorizontalTravel <= MaxTravel);
        TestTrue(FString::Printf(TEXT("%s: rose %.3f cm"), *Case, Rise),
            FMath::IsNearlyEqual(Rise, ExpectedRise, 0.5));
        TestEqual(Case + TEXT(": stopped by geometry"), Path.bStoppedByGeometry, bExpectStop);
        TestTrue(FString::Printf(TEXT("%s: simulated %.6f s"), *Case, Path.SimulatedSeconds), bExpectStop
            ? Path.SimulatedSeconds > 0.0 && Path.SimulatedSeconds < FGirlSaviorDashTestAccess::Duration
            : FMath::IsNearlyEqual(Path.SimulatedSeconds, double(FGirlSaviorDashTestAccess::Duration), 0.00001));
    };

    for (const int32 FrameRate : {30, 60})
    {
        CheckCommittedPath(TEXT("Flat dash"), FVector(0, 0, 90), FrameRate, 299.8, 300.2, 0.0, false);
        CheckCommittedPath(TEXT("Walkable step"), FVector(0, 750, 90), FrameRate, 299.8, 300.2, 20.0, false);
        CheckCommittedPath(TEXT("Floor edge"), FVector(0, 2000, 90), FrameRate, 70.0, 134.0, 0.0, true);
        CheckCommittedPath(TEXT("Hard block after admission"), FVector(-100, -750, 90), FrameRate,
            113.0, 122.0, 0.0, true);
    }

    Hero->SetActorLocation(FVector(0, 0, 90), false, nullptr, ETeleportType::TeleportPhysics);
    FGirlSaviorDashTestAccess::CheckProtection(*this, Movement);
    return true;
}

#endif
