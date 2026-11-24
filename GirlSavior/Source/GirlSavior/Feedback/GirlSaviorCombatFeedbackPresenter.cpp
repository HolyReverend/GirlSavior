#include "Feedback/GirlSaviorCombatFeedbackPresenter.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Encounters/GirlSaviorCombatHUD.h"
#include "Encounters/GirlSaviorCombatTarget.h"
#include "Encounters/GirlSaviorCombatWidgets.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/GirlSaviorCombatWarning.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorFeedback, Log, All);

namespace
{
using ERole = EGirlSaviorEnemyRole;
using EPhase = EGirlSaviorEnemyAttackPhase;
using ECue = EGirlSaviorFeedbackCue;
constexpr double FallSeconds = 0.32;
constexpr double CorpseMinimum = 3.0;
constexpr double CorpseFadeStart = 9.15;
constexpr double CorpseEnd = 10.0;
constexpr int32 MaximumStrokes = 180;

const FLinearColor ShotColor(1.f, 0.88f, 0.57f);
const FLinearColor EnemyShotColor(1.f, 0.19f, 0.06f);
const FLinearColor BarrierColor(0.19f, 0.75f, 1.f);
const FLinearColor DashColor(0.43f, 1.f, 0.89f);

UMeshComponent* FindHeroBody(const AGirlSaviorCharacter& Hero)
{
    if (Hero.GetMesh()->GetSkeletalMeshAsset())
    {
        return Hero.GetMesh();
    }
    for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(&Hero))
    {
        if (Mesh->GetStaticMesh() && Mesh->GetVisibleFlag() && !Mesh->bHiddenInGame)
        {
            return Mesh;
        }
    }
    return nullptr;
}
}

AGirlSaviorCombatFeedbackPresenter::AGirlSaviorCombatFeedbackPresenter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Lines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("TransientFeedback"));
    SetRootComponent(Lines);
    Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Lines->SetCanEverAffectNavigation(false);
    Lines->PrimaryComponentTick.bStartWithTickEnabled = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Box(TEXT("/Engine/BasicShapes/Cube.Cube"));
    WeaponMesh = Box.Object;
}

bool AGirlSaviorCombatFeedbackPresenter::HasWarningClass() const
{
    return WarningClass && !WarningClass->HasAnyClassFlags(CLASS_Abstract);
}

int32 AGirlSaviorCombatFeedbackPresenter::GetActiveWarningCount() const
{
    int32 Count = 0;
    for (const auto& Entry : Warnings)
    {
        Count += Entry.Value.IsValid();
    }
    return Count;
}

// BeginPlay precedes the encounter's first late-frame activation and wave registration.
void AGirlSaviorCombatFeedbackPresenter::BeginPlay()
{
    Super::BeginPlay();
    if (!IsValid(Encounter) || !Encounter->AttachFeedback(*this))
    {
        UE_LOG(LogGirlSaviorFeedback, Warning, TEXT("%s requires a unique encounter in this world."), *GetName());
        SetActorTickEnabled(false);
        return;
    }
    bAttached = true;
    if (!HasWarningClass())
    {
        UE_LOG(LogGirlSaviorFeedback, Error, TEXT("%s requires a concrete warning Blueprint."), *GetName());
    }
    AddTickPrerequisiteActor(Encounter);
    Encounter->OnHeroChanged.AddUObject(this, &ThisClass::HandleHeroChanged);
    Encounter->OnRunStateChanged.AddUObject(this, &ThisClass::HandleRunState);
    Encounter->OnProjectileRegistered.AddUObject(this, &ThisClass::BindProjectile);
    Encounter->OnEnemyRegistered.AddUniqueDynamic(this, &ThisClass::BindEnemy);
    for (AGirlSaviorEnemyCharacter* Enemy : Encounter->GetLivingEnemies())
    {
        BindEnemy(Enemy);
    }
    for (AGirlSaviorCombatTarget* Target : Encounter->PlacedTargets)
    {
        if (IsValid(Target))
        {
            BindBody(*Target, nullptr, false, ERole::Pursuer);
        }
    }
    HandleHeroChanged(Encounter->GetHero());
}

void AGirlSaviorCombatFeedbackPresenter::HandleHeroChanged(AGirlSaviorCharacter* Pawn)
{
    if (Hero.Get() == Pawn && (Pawn || Hero.IsExplicitlyNull()))
    {
        return;
    }
    UnbindHero();
    if (!IsValid(Pawn))
    {
        return;
    }

    Hero = Pawn;
    if (auto* Controller = Cast<APlayerController>(Pawn->GetController()))
    {
        Controller->SetAudioListenerAttenuationOverride(Pawn->GetRootComponent(), FVector::ZeroVector);
        ListeningController = Controller;
    }
    UMeshComponent* Mesh = FindHeroBody(*Pawn);
    BindBody(*Pawn, Mesh, true, ERole::Pursuer);
    Pawn->GetFireComponent()->OnCombatShotCue.AddUObject(this, &ThisClass::HandleShot);
    if (Mesh)
    {
        // The lamp's own holder should not project a black disk around the player's feet.
        Mesh->SetCastShadow(false);
        if (DarkBodyMaterial)
        {
            Mesh->SetMaterial(0, DarkBodyMaterial);
        }
    }

    Torch = NewObject<UPointLightComponent>(this);
    Torch->SetMobility(EComponentMobility::Movable);
    Torch->SetIntensityUnits(ELightUnits::Lumens);
    Torch->SetIntensity(TorchIntensity);
    Torch->SetAttenuationRadius(TorchRadius);
    Torch->SetLightColor(TorchColor);
    Torch->SetSourceRadius(22.f);
    Torch->SetCastShadows(true);
    Torch->SetupAttachment(Pawn->GetRootComponent());
    Torch->SetRelativeLocation(TorchOffset);
    Torch->RegisterComponent();

    if (WeaponMesh)
    {
        Weapon = NewObject<UStaticMeshComponent>(this);
        Weapon->SetStaticMesh(WeaponMesh);
        Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Weapon->SetCanEverAffectNavigation(false);
        Weapon->SetCastShadow(false);
        Weapon->SetRelativeScale3D(FVector(0.42, 0.10, 0.14));
        Weapon->SetupAttachment(Pawn->GetRootComponent());
        Weapon->RegisterComponent();
        UpdateWeapon(GetWorld()->GetTimeSeconds());
    }
    ConfigureHUD();
}

void AGirlSaviorCombatFeedbackPresenter::ConfigureHUD()
{
    auto* Controller = Hero.IsValid() ? Cast<APlayerController>(Hero->GetController()) : nullptr;
    auto* HUD = Controller ? Cast<AGirlSaviorCombatHUD>(Controller->GetHUD()) : nullptr;
    if (HUD && IsValid(Encounter) && ConfiguredHUD.Get() != HUD)
    {
        HUD->ConfigurePresentation(*Encounter, VitalsWidgetClass, OutcomeWidgetClass);
        ConfiguredHUD = HUD;
    }
}

void AGirlSaviorCombatFeedbackPresenter::UnbindHero()
{
    if (auto* Controller = ListeningController.Get())
    {
        Controller->ClearAudioListenerAttenuationOverride();
    }
    ListeningController.Reset();
    if (AGirlSaviorCharacter* Pawn = Hero.Get())
    {
        Pawn->GetFireComponent()->OnCombatShotCue.RemoveAll(this);
        UnbindBody(*Pawn);
    }
    if (IsValid(Torch))
    {
        Torch->DestroyComponent();
    }
    if (IsValid(Weapon))
    {
        Weapon->DestroyComponent();
    }
    Torch = nullptr;
    Weapon = nullptr;
    Hero.Reset();
    LastRecoil = -1.0;
}

void AGirlSaviorCombatFeedbackPresenter::BindBody(AActor& Actor, UMeshComponent* Mesh, bool bIsHero, ERole EnemyRole)
{
    auto* Receiver = Actor.FindComponentByClass<UGirlSaviorDamageReceiverComponent>();
    if (!Receiver || BodyPoses.Contains(&Actor))
    {
        return;
    }
    auto& Pose = BodyPoses.Add(&Actor);
    Pose.Mesh = Mesh;
    Pose.Rest = Mesh ? Mesh->GetRelativeTransform() : FTransform::Identity;
    Pose.EnemyRole = EnemyRole;
    Pose.bHero = bIsHero;
    Pose.bRestCastsShadow = Mesh && Mesh->CastShadow;
    Receiver->OnDamagePresentation.AddUObject(this, &ThisClass::HandleDamage);
    Actor.OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleBodyEndPlay);
}

void AGirlSaviorCombatFeedbackPresenter::BindEnemy(AGirlSaviorEnemyCharacter* Enemy)
{
    if (!IsValid(Enemy) || !Enemy->Definition || BodyPoses.Contains(Enemy))
    {
        return;
    }
    if (DarkBodyMaterial)
    {
        Enemy->BodyVisual->SetMaterial(0, DarkBodyMaterial);
    }
    BindBody(*Enemy, Enemy->BodyVisual, false, Enemy->Definition->Role);
    auto* Attack = Enemy->GetAttackComponent();
    Attack->OnNativeAttackCue.AddUObject(this, &ThisClass::HandleAttack);
    Attack->OnChargeBlocked.AddUObject(this, &ThisClass::HandleChargeStop);
    Attack->OnAttackEnded.AddUniqueDynamic(this, &ThisClass::HandleAttackEnd);
}

// Living registry removal may happen before the lethal presentation dispatch.
void AGirlSaviorCombatFeedbackPresenter::UnbindBody(AActor& Actor)
{
    if (const auto* Pose = BodyPoses.Find(&Actor); Pose && Pose->bHero && Pose->Mesh.IsValid())
    {
        Pose->Mesh->SetCastShadow(Pose->bRestCastsShadow);
    }
    if (auto* Receiver = Actor.FindComponentByClass<UGirlSaviorDamageReceiverComponent>())
    {
        Receiver->OnDamagePresentation.RemoveAll(this);
    }
    if (auto* Enemy = Cast<AGirlSaviorEnemyCharacter>(&Actor))
    {
        auto* Attack = Enemy->GetAttackComponent();
        Attack->OnNativeAttackCue.RemoveAll(this);
        Attack->OnChargeBlocked.RemoveAll(this);
        Attack->OnAttackEnded.RemoveDynamic(this, &ThisClass::HandleAttackEnd);
    }
    Actor.OnEndPlay.RemoveDynamic(this, &ThisClass::HandleBodyEndPlay);
    BodyPoses.Remove(&Actor);
}

void AGirlSaviorCombatFeedbackPresenter::HandleBodyEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
    if (Actor)
    {
        UnbindBody(*Actor);
    }
}

void AGirlSaviorCombatFeedbackPresenter::BindProjectile(AGirlSaviorTestProjectile& Projectile)
{
    if (!Projectiles.Contains(&Projectile))
    {
        Projectiles.Add(&Projectile);
        Projectile.OnResolvedImpact.AddUniqueDynamic(this, &ThisClass::HandleProjectileImpact);
        Projectile.OnCompleted.AddUObject(this, &ThisClass::HandleProjectileEnd);
    }
}

void AGirlSaviorCombatFeedbackPresenter::HandleProjectileImpact(const FGirlSaviorProjectileImpact& Impact)
{
    if (Impact.ContactType == EGirlSaviorProjectileContact::Target)
    {
        return; // The receiver presents the committed target result once.
    }
    if (Impact.bWardContact)
    {
        PlayCue(ECue::Ward, SafeWard, Impact.Point);
        AddBurst(Impact.Point, Impact.Normal, BarrierColor, 27.f, 6, 0.20f);
    }
    else
    {
        PlayCue(ECue::Geometry, GeometryImpact, Impact.Point);
        AddBurst(Impact.Point, Impact.Normal, FLinearColor(0.85f, 0.51f, 0.29f), 22.f, 5, 0.18f);
    }
}

void AGirlSaviorCombatFeedbackPresenter::HandleProjectileEnd(AGirlSaviorTestProjectile* Projectile,
    EGirlSaviorProjectileCompletion Reason)
{
    if (!Projectile || Projectiles.Remove(Projectile) == 0)
    {
        return;
    }
    Projectile->OnResolvedImpact.RemoveDynamic(this, &ThisClass::HandleProjectileImpact);
    Projectile->OnCompleted.RemoveAll(this);
    if (Reason == EGirlSaviorProjectileCompletion::Removed || Projectile->GetDistanceTraveled() < 1.0)
    {
        return;
    }
    const bool bEnemy = Projectile->GetSourceFaction() == EGirlSaviorFaction::Enemy;
    const FVector Head = Projectile->GetActorLocation();
    const double TrailLength = FMath::Min(Projectile->GetDistanceTraveled(), bEnemy ? 82.0 : 68.0);
    AddStroke(Head - Projectile->GetTravelDirection() * TrailLength, Head,
        bEnemy ? EnemyShotColor : ShotColor, bEnemy ? 5.f : 2.f, 0.15f);
}

// Render barrier and health from the same committed result, with lethal feedback taking precedence.
void AGirlSaviorCombatFeedbackPresenter::HandleDamage(AActor& Target, const FGirlSaviorDamageResult& Result)
{
    if (Result.Outcome != EGirlSaviorDamageOutcome::Applied)
    {
        return;
    }
    if (Result.BarrierLost > 0.0)
    {
        AddBarrier(Target, Result.bBarrierBroken);
    }
    const FVector Position = Result.Spec.bHasContact ? Result.Spec.Contact : Target.GetActorLocation();
    const FVector Normal = Result.Spec.bHasContact ? Result.Spec.ContactNormal : FVector::UpVector;
    if (Result.bKilled)
    {
        if (auto* Pose = BodyPoses.Find(&Target))
        {
            Pose->DeathTime = GetWorld()->GetTimeSeconds();
            Pose->FallStart = Pose->Mesh.IsValid() ? Pose->Mesh->GetRelativeTransform() : Pose->Rest;
        }
        const bool bHero = &Target == Hero.Get();
        if (bHero && Weapon)
        {
            Weapon->SetVisibility(false);
        }
        PlayCue(bHero ? ECue::HeroDefeat : ECue::EnemyDeath, bHero ? HeroDefeat.Get() : EnemyDeath.Get(), Position, bHero);
        AddBurst(Position, Normal, FLinearColor(1.f, 0.37f, 0.19f), 43.f, 8, 0.32f);
    }
    else if (Result.HealthLost > 0.0)
    {
        PlayCue(ECue::HealthHit, HealthHit, Position);
        AddBurst(Position, Normal, FLinearColor(1.f, 0.20f, 0.16f), 26.f, 4, 0.15f, &Target);
    }
}

void AGirlSaviorCombatFeedbackPresenter::HandleAttack(AGirlSaviorEnemyCharacter& Enemy, const FGirlSaviorAttackCue& Cue)
{
    if (auto* Pose = BodyPoses.Find(&Enemy))
    {
        Pose->Phase = Cue.Phase;
        Pose->AttackId = Cue.AttackId;
        if (Cue.Role == ERole::Pursuer && Cue.Phase == EPhase::Active)
        {
            Pose->SwingUntil = GetWorld()->GetTimeSeconds() + 0.14;
        }
    }
    if (Cue.AttackId == 0 || Cue.Phase == EPhase::Recovery)
    {
        return; // End owns a swing's final frame and detached shots own their own lifetimes.
    }

    auto* Warning = Warnings.FindRef(Cue.AttackId).Get();
    if (!Warning && Cue.Phase == EPhase::Preparation && HasWarningClass())
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Warning = GetWorld()->SpawnActor<AGirlSaviorCombatWarning>(WarningClass, FTransform::Identity, Params);
        if (Warning)
        {
            Warnings.Add(Cue.AttackId, Warning);
            USoundBase* Sound = Cue.Role == ERole::Pursuer ? PursuerReady.Get()
                : Cue.Role == ERole::Shooter ? ShooterReady.Get() : ChargerReady.Get();
            PlayCue(ECue::Windup, Sound, Cue.Origin);
        }
    }
    if (!Warning)
    {
        return;
    }
    if (Cue.Phase == EPhase::Active && Cue.Role == ERole::Shooter)
    {
        Warning->Destroy();
        Warnings.Remove(Cue.AttackId);
        return;
    }
    Warning->ApplyCue(Cue);
    if (Cue.Phase == EPhase::Active && Cue.Role == ERole::Pursuer)
    {
        PlayCue(ECue::Swing, PursuerSwing, Cue.Origin);
    }
}

void AGirlSaviorCombatFeedbackPresenter::HandleAttackEnd(int64 AttackId, EGirlSaviorAttackEndReason Reason)
{
    for (auto& Entry : BodyPoses)
    {
        if (Entry.Value.AttackId == AttackId)
        {
            Entry.Value.AttackId = 0;
            Entry.Value.Phase = EPhase::Approach;
            break;
        }
    }
    if (auto* Warning = Warnings.FindRef(AttackId).Get())
    {
        if (Reason == EGirlSaviorAttackEndReason::Completed && Warning->IsPursuerSwing())
        {
            Warning->PlayFinalSwing();
        }
        else
        {
            Warning->Destroy();
            Warnings.Remove(AttackId);
        }
    }
}

void AGirlSaviorCombatFeedbackPresenter::HandleChargeStop(const FVector& Position, const FVector& Normal)
{
    PlayCue(ECue::ChargeStop, ChargeStop, Position);
    AddBurst(Position, Normal, FLinearColor(0.80f, 0.55f, 0.42f), 38.f, 6, 0.22f);
}

void AGirlSaviorCombatFeedbackPresenter::HandleRunState(EGirlSaviorRunState State)
{
    if (State == EGirlSaviorRunState::Succeeded || State == EGirlSaviorRunState::Defeated
        || State == EGirlSaviorRunState::Recovery)
    {
        ClearWarnings();
        for (auto& Entry : BodyPoses)
        {
            Entry.Value.Phase = EPhase::Approach;
            Entry.Value.SwingUntil = 0.0;
        }
        Strokes.Reset();
        Lines->Flush();
        for (const auto& Entry : Sounds)
        {
            if (auto* Audio = Entry.Get())
            {
                Audio->FadeOut(0.12f, 0.f);
            }
        }
        Sounds.Reset();
    }
}

void AGirlSaviorCombatFeedbackPresenter::ClearWarnings()
{
    for (const auto& Entry : Warnings)
    {
        if (Entry.Value.IsValid())
        {
            Entry.Value->Destroy();
        }
    }
    Warnings.Reset();
}

void AGirlSaviorCombatFeedbackPresenter::PlayCue(ECue Cue, USoundBase* Sound, const FVector& Position, bool bForeground)
{
    ++CueCounts.FindOrAdd(Cue);
    if (!Sound)
    {
        return;
    }
    auto* Audio = bForeground ? UGameplayStatics::SpawnSound2D(this, Sound, 0.85f)
        : UGameplayStatics::SpawnSoundAtLocation(this, Sound, Position, FRotator::ZeroRotator, 0.75f, 1.f, 0.f, CueAttenuation);
    if (Audio)
    {
        Sounds.Add(Audio);
    }
}

void AGirlSaviorCombatFeedbackPresenter::AddStroke(const FVector& From, const FVector& To,
    const FLinearColor& Color, float Width, float Lifetime, const FVector& Drift, AActor* Anchor)
{
    if (Strokes.Num() < MaximumStrokes)
    {
        Strokes.Add({From, To, Drift, Color, Width, GetWorld()->GetTimeSeconds(), Lifetime,
            Anchor, Anchor ? Anchor->GetActorLocation() : FVector::ZeroVector});
    }
}

void AGirlSaviorCombatFeedbackPresenter::AddBurst(const FVector& Center, const FVector& Normal,
    const FLinearColor& Color, float Radius, int32 Count, float Lifetime, AActor* Anchor)
{
    const FVector Axis = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    FVector Right, Up;
    Axis.FindBestAxisVectors(Right, Up);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const double Angle = Index * UE_TWO_PI / Count;
        const FVector Radial = FMath::Cos(Angle) * Right + FMath::Sin(Angle) * Up;
        const FVector Start = Center + Axis * 3 + Radial * 6;
        const FVector End = Start + Radial * Radius + Axis * (4 + Index % 3 * 3);
        AddStroke(Start, End, Color, 3.f, Lifetime, Radial * 22 + Axis * 12, Anchor);
    }
}

void AGirlSaviorCombatFeedbackPresenter::AddBarrier(AActor& Target, bool bBroken)
{
    const auto* Capsule = Target.FindComponentByClass<UCapsuleComponent>();
    const FVector Foot = Target.GetActorLocation()
        - FVector(0, 0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() - 4.0 : 30.0);
    const double Radius = Capsule ? Capsule->GetScaledCapsuleRadius() + 17.0 : 48.0;
    constexpr int32 Segments = 16;
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const double A = Index * UE_TWO_PI / Segments;
        const double B = A + (bBroken ? 0.58 : 1.0) * UE_TWO_PI / Segments;
        const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.0);
        const FVector End(FMath::Cos(B), FMath::Sin(B), 0.0);
        AddStroke(Foot + Radial * Radius, Foot + End * Radius, BarrierColor, bBroken ? 6.f : 4.f,
            bBroken ? 0.32f : 0.20f, bBroken ? Radial * 110 + FVector(0, 0, 35) : FVector::ZeroVector,
            bBroken ? nullptr : &Target);
    }
    const bool bFatal = Target.FindComponentByClass<UGirlSaviorDamageReceiverComponent>()->GetHealth() <= 0.0;
    PlayCue(bBroken ? ECue::BarrierBreak : ECue::BarrierSustain,
        bFatal ? nullptr : bBroken ? BarrierBreak.Get() : BarrierSustain.Get(), Foot);
}

FVector AGirlSaviorCombatFeedbackPresenter::ClipMuzzle(const FVector& Origin, const FVector& Desired) const
{
    FHitResult Hit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorCosmeticMuzzle), false, Hero.Get());
    if (GetWorld()->SweepSingleByChannel(Hit, Origin, Desired, FQuat::Identity,
            GirlSaviorCombatChannels::Occluder, FCollisionShape::MakeSphere(5.f), Query))
    {
        return Hit.bStartPenetrating ? Origin : Hit.Location - (Desired - Origin).GetSafeNormal() * 1.5;
    }
    return Desired;
}

void AGirlSaviorCombatFeedbackPresenter::HandleShot(FVector Position, FVector Direction)
{
    LastRecoil = GetWorld()->GetTimeSeconds();
    const FVector Muzzle = ClipMuzzle(Position, Position + Direction * 47 + FVector(0, 0, 11));
    const FVector FlashEnd = ClipMuzzle(Muzzle, Muzzle + Direction * 23);
    AddBurst(Muzzle, -Direction, ShotColor, 9.f, 5, 0.075f);
    AddStroke(Muzzle, FlashEnd, ShotColor, 6.f, 0.075f);
    PlayCue(ECue::Muzzle, PistolShot, Muzzle);
}

void AGirlSaviorCombatFeedbackPresenter::UpdateWeapon(double Now)
{
    if (!Hero.IsValid() || !Weapon || !Weapon->IsVisible())
    {
        return;
    }
    const double Kick = LastRecoil >= 0.0 ? 8.0 * FMath::Clamp(1.0 - (Now - LastRecoil) / 0.12, 0.0, 1.0) : 0.0;
    const FVector Center = Hero->GetActorLocation();
    const FVector Target = Center + Hero->GetActorForwardVector() * (29.0 - Kick)
        + Hero->GetActorRightVector() * 17 + FVector(0, 0, 11);
    Weapon->SetWorldLocation(ClipMuzzle(Center, Target));
}

void AGirlSaviorCombatFeedbackPresenter::DrawCosmetics(double Now)
{
    Strokes.RemoveAllSwap([&](const FStroke& Stroke) { return Now >= Stroke.Start + Stroke.Lifetime; }, EAllowShrinking::No);
    TArray<FBatchedLine, TInlineAllocator<80>> Batch;
    for (const FStroke& Stroke : Strokes)
    {
        const float Age = static_cast<float>(Now - Stroke.Start);
        const float Gain = 1.f - Age / static_cast<float>(Stroke.Lifetime);
        const AActor* Anchor = Stroke.Anchor.Get();
        const FVector Offset = Stroke.Drift * Age
            + (Anchor ? Anchor->GetActorLocation() - Stroke.AnchorStart : FVector::ZeroVector);
        Batch.Emplace(Stroke.From + Offset, Stroke.To + Offset, Stroke.Color * Gain,
            0.f, Stroke.Width * Gain, SDPG_World);
    }
    if (Hero.IsValid())
    {
        const auto* Movement = Cast<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
        if (Movement && Movement->IsDashProtected())
        {
            const auto* Capsule = Hero->GetCapsuleComponent();
            const FVector Center = Hero->GetActorLocation();
            const double Radius = Capsule->GetScaledCapsuleRadius() + 5.0;
            const double Height = Capsule->GetScaledCapsuleHalfHeight();
            for (int32 Level = 0; Level < 3; ++Level)
            {
                const FVector RingCenter = Center + FVector(0, 0, -Height + 8.0 + Level * Height * 0.7);
                for (int32 Index = 0; Index < 12; ++Index)
                {
                    const double A = Index * UE_TWO_PI / 12;
                    const double B = (Index + 1) * UE_TWO_PI / 12;
                    Batch.Emplace(RingCenter + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius,
                        RingCenter + FVector(FMath::Cos(B), FMath::Sin(B), 0) * Radius,
                        DashColor * (Level == 0 ? 1.f : 0.6f), 0.f, 2.5f, SDPG_World);
                }
            }
        }
    }
    Lines->Flush();
    if (!Batch.IsEmpty())
    {
        Lines->DrawLines(Batch);
    }
}

// Animate only the proxy mesh. Collision, actor transforms and camera remain in gameplay ownership.
void AGirlSaviorCombatFeedbackPresenter::UpdateBodies(double Now, float DeltaSeconds)
{
    TArray<TWeakObjectPtr<AActor>> Expired;
    for (auto& Entry : BodyPoses)
    {
        auto& Pose = Entry.Value;
        auto* Mesh = Pose.Mesh.Get();
        if (!Mesh)
        {
            continue;
        }
        if (Pose.DeathTime >= 0.0)
        {
            const double Age = Now - Pose.DeathTime;
            const bool bOffscreen = Age >= CorpseMinimum && IsValid(Encounter)
                && Encounter->IsPresentationBoundsOffscreen(Mesh->Bounds.GetBox());
            if (!Pose.bHero && (Age >= CorpseEnd || bOffscreen))
            {
                Expired.Add(Entry.Key);
                continue;
            }
            if (!Pose.bHero && Age >= CorpseFadeStart && DarkBodyMaterial)
            {
                auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
                if (!MID)
                {
                    MID = Mesh->CreateDynamicMaterialInstance(0, DarkBodyMaterial);
                }
                if (MID)
                {
                    MID->SetScalarParameterValue(TEXT("Coverage"), static_cast<float>((CorpseEnd - Age) / (CorpseEnd - CorpseFadeStart)));
                }
            }
            const FRotator Fall = Pose.bHero ? FRotator(86, 0, -12) : Pose.EnemyRole == ERole::Pursuer
                ? FRotator(83, 0, 20) : Pose.EnemyRole == ERole::Shooter ? FRotator(-75, 18, 0) : FRotator(13, 0, 84);
            FTransform End = Pose.Rest;
            End.SetRotation((Pose.Rest.GetRotation() * Fall.Quaternion()).GetNormalized());
            End.AddToTranslation(FVector(0, 0, Pose.bHero ? -49.0 : -45.0));
            FTransform Current;
            Current.Blend(Pose.FallStart, End, static_cast<float>(FMath::SmoothStep(0.0, FallSeconds, Age)));
            Mesh->SetRelativeTransform(Current);
            continue;
        }
        if (Pose.bHero)
        {
            continue;
        }

        FTransform Goal = Pose.Rest;
        const EPhase PosePhase = Now < Pose.SwingUntil ? EPhase::Active : Pose.Phase;
        if (PosePhase == EPhase::Preparation || PosePhase == EPhase::Active)
        {
            if (Pose.EnemyRole == ERole::Charger)
            {
                FVector Scale = Goal.GetScale3D();
                Scale.Z *= 0.72;
                Goal.SetScale3D(Scale);
                Goal.AddToTranslation(FVector(0, 0, -19));
            }
            const float Pitch = PosePhase == EPhase::Active ? 32.f : Pose.EnemyRole == ERole::Pursuer ? -18.f : 9.f;
            Goal.SetRotation((Pose.Rest.GetRotation() * FRotator(Pitch, 0, 0).Quaternion()).GetNormalized());
        }
        FTransform Next;
        Next.Blend(Mesh->GetRelativeTransform(), Goal, FMath::Min(1.f, DeltaSeconds * 14.f));
        Mesh->SetRelativeTransform(Next);
    }
    for (const auto& Entry : Expired)
    {
        if (auto* Enemy = Cast<AGirlSaviorEnemyCharacter>(Entry.Get()))
        {
            Enemy->Destroy();
        }
    }
}

void AGirlSaviorCombatFeedbackPresenter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now = GetWorld()->GetTimeSeconds();
    if (!ConfiguredHUD.IsValid())
    {
        ConfigureHUD();
    }
    for (auto It = Warnings.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = Projectiles.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
    Sounds.RemoveAllSwap([](const auto& Sound) { return !Sound.IsValid(); }, EAllowShrinking::No);
    UpdateWeapon(Now);
    UpdateBodies(Now, DeltaSeconds);
    DrawCosmetics(Now);
}

void AGirlSaviorCombatFeedbackPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* HUD = ConfiguredHUD.Get())
    {
        HUD->ReleasePresentation();
    }
    if (bAttached && IsValid(Encounter))
    {
        Encounter->OnHeroChanged.RemoveAll(this);
        Encounter->OnRunStateChanged.RemoveAll(this);
        Encounter->OnProjectileRegistered.RemoveAll(this);
        Encounter->OnEnemyRegistered.RemoveDynamic(this, &ThisClass::BindEnemy);
        Encounter->DetachFeedback(*this);
    }
    UnbindHero();
    TArray<TWeakObjectPtr<AActor>> Bodies;
    BodyPoses.GetKeys(Bodies);
    for (const auto& Entry : Bodies)
    {
        if (AActor* Actor = Entry.Get())
        {
            UnbindBody(*Actor);
        }
    }
    for (const auto& Entry : Projectiles)
    {
        if (auto* Projectile = Entry.Get())
        {
            Projectile->OnResolvedImpact.RemoveDynamic(this, &ThisClass::HandleProjectileImpact);
            Projectile->OnCompleted.RemoveAll(this);
        }
    }
    for (const auto& Entry : Sounds)
    {
        if (auto* Audio = Entry.Get())
        {
            Audio->Stop();
        }
    }
    ClearWarnings();
    Lines->Flush();
    Super::EndPlay(EndPlayReason);
}
