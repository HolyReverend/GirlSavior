#include "Encounters/GirlSaviorCombatEncounter.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/GirlSaviorCombatRoom.h"
#include "Encounters/GirlSaviorWaveSpatial.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorWaves, Log, All);

namespace
{
constexpr double QueryInterval = 0.25;
constexpr float PathSampleSpacing = 100.f;
constexpr float GroundedSlotTolerance = 6.f;
constexpr int32 PathFailuresBeforeRetirement = 2;
using ERole = EGirlSaviorEnemyRole;
using EWaveVisibility = GirlSaviorWaveSpatial::EVisibility;

bool ContainsOrderedRoles(TConstArrayView<ERole> Recipe, TConstArrayView<ERole> Roles)
{
    if (Roles.IsEmpty())
    {
        return false;
    }
    int32 Index = 0;
    for (ERole Role : Recipe)
    {
        if (Role == Roles[Index] && ++Index == Roles.Num())
        {
            return true;
        }
    }
    return false;
}

// Prefer the path the enemy is already following over a fresh shortest-path estimate.
bool GetRemainingApproach(const AGirlSaviorCombatEncounter& Encounter, const AGirlSaviorEnemyCharacter& Enemy,
    TArray<FVector>& OutPath)
{
    const auto* Controller = Enemy.GetController();
    const auto* Following = Controller ? Controller->FindComponentByClass<UPathFollowingComponent>() : nullptr;
    if (!Following || Following->GetStatus() != EPathFollowingStatus::Moving)
    {
        return false;
    }
    const auto Path = Following->GetPath();
    if (!Path.IsValid() || !Path->IsValid() || Path->IsPartial())
    {
        return false;
    }
    const auto& Points = Path->GetPathPoints();
    const int32 StartIndex = static_cast<int32>(Following->GetNextPathIndex());
    if (!Points.IsValidIndex(StartIndex))
    {
        return false;
    }
    const auto* Capsule = Enemy.GetCapsuleComponent();
    const FVector Offset(0, 0, Capsule->GetScaledCapsuleHalfHeight());
    OutPath.Reset();
    OutPath.Add(Enemy.GetActorLocation());
    for (int32 Index = StartIndex; Index < Points.Num(); ++Index)
    {
        const FVector Next = Points[Index].Location + Offset;
        if (!Encounter.CanTraverseCombatSegment(OutPath.Last(), Next, Capsule->GetScaledCapsuleRadius()))
        {
            OutPath.Reset();
            return false;
        }
        OutPath.Add(Next);
    }
    return true;
}
}

const AGirlSaviorEnemyCharacter* AGirlSaviorCombatEncounter::GetEnemyDefaults(ERole EnemyRole) const
{
    const auto* Class = EnemyClasses.Find(EnemyRole);
    return Class && *Class && !(*Class)->HasAnyClassFlags(CLASS_Abstract) ? Class->GetDefaultObject() : nullptr;
}

// The ordered room list also supplies a deterministic tie break for exact boundary contacts.
bool AGirlSaviorCombatEncounter::ValidateWaveRoute()
{
    if (!IsValid(WaveSettings) || !WaveSettings->IsConfigurationValid() || Rooms.IsEmpty())
    {
        UE_LOG(LogGirlSaviorWaves, Error, TEXT("Wave route requires valid shared settings and an ordered room list."));
        return false;
    }
    for (const auto EnemyRole : {ERole::Pursuer, ERole::Shooter, ERole::Charger})
    {
        const auto* Defaults = GetEnemyDefaults(EnemyRole);
        if (!Defaults || !Defaults->Definition || Defaults->Definition->Role != EnemyRole || !Defaults->Definition->IsValidForSpawn())
        {
            UE_LOG(LogGirlSaviorWaves, Error, TEXT("Missing or invalid wave class for role %d."), int32(EnemyRole));
            return false;
        }
    }
    TSet<FName> RoomIds;
    for (const AGirlSaviorCombatRoom* Room : Rooms)
    {
        if (!IsValid(Room) || Room->GetWorld() != GetWorld() || Room->RoomId.IsNone() || RoomIds.Contains(Room->RoomId)
            || !IsValid(Room->Profile) || !Room->Profile->IsConfigurationValid(WaveSettings)
            || !Room->Volume || Room->Volume->GetScaledBoxExtent().GetMin() <= 0.0 || Room->Entrances.IsEmpty())
        {
            UE_LOG(LogGirlSaviorWaves, Error, TEXT("Invalid/duplicate wave room %s."), *GetNameSafe(Room));
            return false;
        }
        RoomIds.Add(Room->RoomId);
        TSet<FName> EntranceIds;
        for (const auto& Entrance : Room->Entrances)
        {
            if (Entrance.EntranceId.IsNone() || EntranceIds.Contains(Entrance.EntranceId)
                || Entrance.ApproachDirection.ContainsNaN() || Entrance.ApproachDirection.IsNearlyZero()
                || Entrance.Slots.Num() < WaveSettings->MaxGroupSize)
            {
                UE_LOG(LogGirlSaviorWaves, Error, TEXT("Invalid entrance %s in %s."),
                    *Entrance.EntranceId.ToString(), *Room->RoomId.ToString());
                return false;
            }
            EntranceIds.Add(Entrance.EntranceId);
            for (const FTransform& Slot : Entrance.Slots)
            {
                const FTransform WorldSlot = Slot * Room->GetActorTransform();
                if (WorldSlot.ContainsNaN() || !WorldSlot.GetScale3D().Equals(FVector::OneVector)
                    || IsPointInSafeArea(WorldSlot.GetLocation()))
                {
                    UE_LOG(LogGirlSaviorWaves, Error, TEXT("Invalid slot in %s/%s."),
                        *Room->RoomId.ToString(), *Entrance.EntranceId.ToString());
                    return false;
                }
            }
        }
        TSet<AGirlSaviorCombatRoom*> LinkedRooms;
        for (const auto& Link : Room->Neighbors)
        {
            if (!IsValid(Link.Room) || Link.Room == Room || !Rooms.Contains(Link.Room.Get()) || LinkedRooms.Contains(Link.Room.Get())
                || (Link.Door && (!IsValid(Link.Door) || Link.Door->GetWorld() != GetWorld())))
            {
                UE_LOG(LogGirlSaviorWaves, Error, TEXT("Invalid topology link in %s."), *Room->RoomId.ToString());
                return false;
            }
            LinkedRooms.Add(Link.Room.Get());
            if (Link.Door)
            {
                RouteDoors.AddUnique(TWeakObjectPtr<const AGirlSaviorCombatDoor>(Link.Door.Get()));
            }
        }
    }
    return true;
}

bool AGirlSaviorCombatEncounter::RegisterEnemy(AGirlSaviorEnemyCharacter& Enemy, AGirlSaviorCombatRoom* Room,
    bool bFixture, int64 GroupId)
{
    if (LiveEnemies.Contains(&Enemy) || !IsValid(Enemy.Definition)
        || (bFixture ? RunState != EGirlSaviorRunState::Waiting : RunState != EGirlSaviorRunState::Active))
    {
        return false;
    }
    const auto EnemyRole = Enemy.Definition->Role;
    if (bWavesEnabled)
    {
        const auto Live = GetLivePopulation();
        if (Live.Total >= WaveSettings->GlobalCap || Live.Get(EnemyRole) >= WaveSettings->GetRoleCap(EnemyRole))
        {
            return false;
        }
    }
    if (!Enemy.InitializeForEncounter(*this) || !IsValid(&Enemy) || !Enemy.GetDamageReceiver()->IsAlive()
        || (!bFixture && RunState != EGirlSaviorRunState::Active))
    {
        return false;
    }
    BindAdmission(*Enemy.GetDamageReceiver());
    FEnemyRecord& Record = LiveEnemies.Add(&Enemy);
    Record.Role = EnemyRole;
    Record.Room = Room;
    Record.bFixture = bFixture;
    Record.bRetain = bFixture && Enemy.ActorHasTag(TEXT("CombatRetainFixture"));
    Record.GroupId = GroupId;
    return true;
}

// Presenters only get a removal for a registration they were given.
void AGirlSaviorCombatEncounter::ActivateRegisteredEnemy(AGirlSaviorEnemyCharacter& Enemy)
{
    FEnemyRecord* Record = LiveEnemies.Find(&Enemy);
    if (Record && !Record->bPublished)
    {
        Enemy.ActivateForEncounter();
        Record->bPublished = true;
        OnEnemyRegistered.Broadcast(&Enemy);
    }
}

FGirlSaviorWavePopulation AGirlSaviorCombatEncounter::GetLivePopulation() const
{
    FGirlSaviorWavePopulation Population;
    for (const auto& Entry : LiveEnemies)
    {
        if (Entry.Key.IsValid() && Entry.Key->GetDamageReceiver()->IsAlive())
        {
            Population.Add(Entry.Value.Role);
        }
    }
    return Population;
}

int32 AGirlSaviorCombatEncounter::GetOccupiedRoleCount(ERole EnemyRole) const
{
    int32 Count = GetLivePopulation().Get(EnemyRole);
    if (Reservation.IsSet())
    {
        for (const auto& Member : Reservation->Members)
        {
            Count += Member.Role == EnemyRole;
        }
    }
    return Count;
}

AGirlSaviorCombatRoom* AGirlSaviorCombatEncounter::FindRoomAt(const FVector& Location) const
{
    for (AGirlSaviorCombatRoom* Room : Rooms)
    {
        if (IsValid(Room) && Room->ContainsPoint(Location))
        {
            return Room;
        }
    }
    return nullptr;
}

bool AGirlSaviorCombatEncounter::IsRoomReachable(const AGirlSaviorCombatRoom* From, const AGirlSaviorCombatRoom* To) const
{
    return IsValid(From) && IsValid(To) && (From == To || From->HasOpenLinkTo(*To));
}

bool AGirlSaviorCombatEncounter::CanTraverseCombatSegment(const FVector& Start, const FVector& End, float Radius) const
{
    return GirlSaviorWaveSpatial::IsSegmentOpen(*this, RouteDoors, Start, End, Radius);
}

void AGirlSaviorCombatEncounter::RefreshHeroRoom()
{
    if (!bWavesEnabled || IsHeroProtected())
    {
        return;
    }
    const FVector Location = Hero->GetActorLocation();
    if (auto* Room = FindRoomAt(Location); Room && Room != CurrentRoom.Get())
    {
        CurrentRoom = Room;
        UE_LOG(LogGirlSaviorWaves, Display, TEXT("t=%.3f entered room=%s profile=%s."),
            GetWorld()->GetTimeSeconds(), *Room->RoomId.ToString(), *GetNameSafe(Room->Profile));
    }
    LastOutsideApproach = Location;
}

void AGirlSaviorCombatEncounter::UpdateWaves(double Now)
{
    if (IsHeroProtected())
    {
        CancelReservation(TEXT("protected-hero"));
    }
    RecordVisibleArrivals(Now);
    const bool bQueryDue = Now >= NextWaveQuery;
    if (bQueryDue)
    {
        NextWaveQuery = Now + QueryInterval;
        UpdateEnemyRoomsAndRetire();
    }
    if (RunState != EGirlSaviorRunState::Active || IsHeroProtected())
    {
        return;
    }
    if (Reservation.IsSet())
    {
        if (Now >= Reservation->DueAt)
        {
            RevalidateAndCommit(Now);
        }
        return;
    }
    if (bQueryDue && bViewValid && CurrentRoom.IsValid() && IsValid(CurrentRoom->Profile))
    {
        TryReserveGroup(Now, *CurrentRoom.Get(), *CurrentRoom->Profile);
    }
}

// A visible body always stays live. A nearby offscreen body gets another path check before retirement.
void AGirlSaviorCombatEncounter::UpdateEnemyRoomsAndRetire()
{
    TArray<TWeakObjectPtr<AGirlSaviorEnemyCharacter>> Retirements;
    for (auto It = LiveEnemies.CreateIterator(); It; ++It)
    {
        AGirlSaviorEnemyCharacter* Enemy = It.Key().Get();
        if (!Enemy)
        {
            It.RemoveCurrent();
            continue;
        }
        FEnemyRecord& Record = It.Value();
        const FVector Center = Enemy->GetActorLocation();
        if (auto* Room = FindRoomAt(Center))
        {
            Record.Room = Room;
        }
        if (!bViewValid || Record.bRetain || !LastOutsideApproach.IsSet()
            || GirlSaviorWaveSpatial::EnemyVisibility(View, *Enemy) != EWaveVisibility::Outside)
        {
            Record.ConsecutivePathFailures = 0;
            continue;
        }
        const auto* Room = Record.Room.Get();
        const auto* HeroRoom = CurrentRoom.Get();
        const bool bNearby = Room && HeroRoom && (Room == HeroRoom || Room->IsAdjacentTo(*HeroRoom));
        if (!bNearby)
        {
            Retirements.Add(Enemy);
            continue;
        }
        TArray<FVector> Path;
        double Length = 0.0;
        const bool bReachable = IsRoomReachable(Room, HeroRoom) && GirlSaviorWaveSpatial::FindApproachPath(
            *this, RouteDoors, *Enemy, Center, *LastOutsideApproach, WaveSettings->NavProjectionTolerance, Path, Length);
        Record.ConsecutivePathFailures = bReachable ? 0 : Record.ConsecutivePathFailures + 1;
        if (Record.ConsecutivePathFailures >= PathFailuresBeforeRetirement)
        {
            Retirements.Add(Enemy);
        }
    }
    // Stopping an attack publishes a cue end, so do it with no registry iterators outstanding.
    for (const auto& Entry : Retirements)
    {
        if (AGirlSaviorEnemyCharacter* Enemy = Entry.Get())
        {
            UE_LOG(LogGirlSaviorWaves, Display, TEXT("t=%.3f retired %s without a kill."),
                GetWorld()->GetTimeSeconds(), *Enemy->GetName());
            Enemy->StopForEncounter();
            Enemy->SetActorEnableCollision(false);
            UnregisterEnemy(Enemy);
            Enemy->Destroy();
        }
    }
}

void AGirlSaviorCombatEncounter::RecordVisibleArrivals(double Now)
{
    if (!bViewValid)
    {
        return;
    }
    for (auto& Entry : LiveEnemies)
    {
        if (Entry.Key.IsValid() && Entry.Value.GroupId != 0 && !Entry.Value.bReportedVisible
            && GirlSaviorWaveSpatial::EnemyVisibility(View, *Entry.Key.Get()) == EWaveVisibility::Visible)
        {
            Entry.Value.bReportedVisible = true;
            UE_LOG(LogGirlSaviorWaves, Display, TEXT("t=%.3f first-visible group=%lld enemy=%s room=%s."),
                Now, Entry.Value.GroupId, *Entry.Key->GetName(), *GetNameSafe(CurrentRoom.Get()));
        }
    }
}

bool AGirlSaviorCombatEncounter::CanAccelerateWave() const
{
    if (!bViewValid || !LastOutsideApproach.IsSet())
    {
        return false;
    }
    for (const auto& Entry : LiveEnemies)
    {
        const auto* Enemy = Entry.Key.Get();
        if (!Enemy || !Enemy->GetDamageReceiver()->IsAlive())
        {
            continue;
        }
        if (GirlSaviorWaveSpatial::EnemyVisibility(View, *Enemy) != EWaveVisibility::Outside)
        {
            return false;
        }
        const auto* Room = FindRoomAt(Enemy->GetActorLocation());
        if (!IsRoomReachable(Room ? Room : Entry.Value.Room.Get(), CurrentRoom.Get()))
        {
            // A closed edge or a distant room cannot contribute an imminent approach.
            continue;
        }
        TArray<FVector> Path;
        double Length = 0.0;
        if (!GetRemainingApproach(*this, *Enemy, Path) && !GirlSaviorWaveSpatial::FindApproachPath(
            *this, RouteDoors, *Enemy, Enemy->GetActorLocation(), *LastOutsideApproach,
            WaveSettings->NavProjectionTolerance, Path, Length))
        {
            return false;
        }
        const FBox Bounds = GirlSaviorWaveSpatial::EnemyBounds(*Enemy);
        const auto ETA = GirlSaviorWaveSpatial::FirstVisibleETA(View, Bounds, Path, Enemy->Definition->WalkSpeed, PathSampleSpacing);
        if (!ETA.IsSet() || ETA.GetValue() <= WaveSettings->ImminentApproachTime)
        {
            return false;
        }
    }
    return true;
}

bool AGirlSaviorCombatEncounter::WaveTimingAllows(const UGirlSaviorWaveProfile& Profile, double Now) const
{
    return GirlSaviorWavePolicy::TimingAllows(*WaveSettings, Profile, LastCommittedGroupTime, Now, false)
        || (Profile.bAccelerateEmptyFrame && LastCommittedGroupTime.IsSet()
            && Now - LastCommittedGroupTime.GetValue() >= WaveSettings->MinimumGroupGap && CanAccelerateWave());
}

bool AGirlSaviorCombatEncounter::CheckSpawnSlot(const AGirlSaviorCombatRoom& Room, const FGirlSaviorWaveEntrance& Entrance,
    const FWaveMember& Member, double& OutPathLength, const TCHAR** OutFailure) const
{
    const auto Reject = [OutFailure](const TCHAR* Reason)
    {
        if (OutFailure)
        {
            *OutFailure = Reason;
        }
        return false;
    };
    const auto* Defaults = GetEnemyDefaults(Member.Role);
    if (!Defaults || !Entrance.Slots.IsValidIndex(Member.SlotIndex)
        || !Member.Transform.Equals(Entrance.Slots[Member.SlotIndex] * Room.GetActorTransform()))
    {
        return Reject(TEXT("slot-changed"));
    }
    const auto Visibility = GirlSaviorWaveSpatial::ProjectBounds(View,
        GirlSaviorWaveSpatial::SpawnBounds(*Defaults, Member.Transform), WaveSettings->ScreenMargin);
    if (Visibility != EWaveVisibility::Outside)
    {
        return Reject(Visibility == EWaveVisibility::Visible ? TEXT("slot-in-frame") : TEXT("invalid-projection"));
    }
    if (!GirlSaviorWaveSpatial::IsSlotClear(*this, *Defaults, Member.Transform, GroundedSlotTolerance))
    {
        return Reject(TEXT("slot-collision-or-floor"));
    }
    auto* SlotRoom = FindRoomAt(Member.Transform.GetLocation());
    if (SlotRoom && !IsRoomReachable(SlotRoom, &Room))
    {
        return Reject(TEXT("closed-or-unlinked-room"));
    }
    TArray<FVector> Path;
    if (!GirlSaviorWaveSpatial::FindApproachPath(*this, RouteDoors, *Defaults, Member.Transform.GetLocation(),
        *LastOutsideApproach, WaveSettings->NavProjectionTolerance, Path, OutPathLength))
    {
        return Reject(TEXT("approach-unreachable"));
    }
    return OutPathLength >= WaveSettings->MinimumApproachDistance || Reject(TEXT("approach-too-short"));
}

// Select a single entrance. Prefer the first eligible recipe and shortest approach, preserving stable ID ties.
void AGirlSaviorCombatEncounter::TryReserveGroup(double Now, AGirlSaviorCombatRoom& Room, const UGirlSaviorWaveProfile& Profile)
{
    const auto Live = GetLivePopulation();
    const int32 Budget = GirlSaviorWavePolicy::GetBudget(*WaveSettings, Profile, Live.Total);
    if (Budget <= 0 || !WaveTimingAllows(Profile, Now))
    {
        return;
    }
    const bool bIntro = Profile.bHasIntroduction && !ServedIntroductions.Contains(Room.RoomId);
    const TArray<ERole> IntroRoles = {Profile.IntroductionRole};
    if (bIntro && !GirlSaviorWavePolicy::CanAddGroup(*WaveSettings, Profile, Live, IntroRoles))
    {
        return;
    }
    for (int32 RecipeIndex = 0; RecipeIndex < (bIntro ? 1 : Profile.Recipes.Num()); ++RecipeIndex)
    {
        const auto Roles = bIntro ? IntroRoles : GirlSaviorWavePolicy::SelectRepeatRoles(
            *WaveSettings, Profile.Recipes[RecipeIndex].Roles, Budget, Live);
        if (Roles.IsEmpty())
        {
            continue;
        }
        // Assign the required role first if the entrance has fewer valid slots than the recipe.
        const int32 RequiredIndex = bIntro ? 0 : Roles.Find(ERole::Pursuer);
        TArray<int32> AssignmentOrder = {RequiredIndex};
        for (int32 Index = 0; Index < Roles.Num(); ++Index)
        {
            if (Index != RequiredIndex)
            {
                AssignmentOrder.Add(Index);
            }
        }
        const FGirlSaviorWaveEntrance* BestEntrance = nullptr;
        TArray<FWaveMember> BestMembers;
        double BestApproach = TNumericLimits<double>::Max();
        for (const auto& Entrance : Room.Entrances)
        {
            TArray<TOptional<FWaveMember>> ByRecipeOrder;
            ByRecipeOrder.SetNum(Roles.Num());
            double Nearest = TNumericLimits<double>::Max();
            for (int32 RecipePosition : AssignmentOrder)
            {
                const auto EnemyRole = Roles[RecipePosition];
                const auto* Defaults = GetEnemyDefaults(EnemyRole);
                if (!Defaults)
                {
                    continue;
                }
                for (int32 Index = 0; Index < Entrance.Slots.Num(); ++Index)
                {
                    const FTransform Transform = Entrance.Slots[Index] * Room.GetActorTransform();
                    if (ByRecipeOrder.ContainsByPredicate([&](const TOptional<FWaveMember>& Used)
                        {
                            if (!Used.IsSet())
                            {
                                return false;
                            }
                            const auto* Other = GetEnemyDefaults(Used->Role);
                            const double CombinedRadius = Defaults->Definition->CapsuleRadius + Other->Definition->CapsuleRadius;
                            return FVector::DistSquared2D(Used->Transform.GetLocation(), Transform.GetLocation())
                                < FMath::Square(CombinedRadius);
                        }))
                    {
                        continue;
                    }
                    const FWaveMember Member{EnemyRole, Index, Transform};
                    double Length = 0.0;
                    if (CheckSpawnSlot(Room, Entrance, Member, Length))
                    {
                        ByRecipeOrder[RecipePosition] = Member;
                        Nearest = FMath::Min(Nearest, Length);
                        break;
                    }
                }
                if (!ByRecipeOrder[RequiredIndex].IsSet())
                {
                    break;
                }
            }
            if (ByRecipeOrder[RequiredIndex].IsSet()
                && (!BestEntrance || Nearest < BestApproach - 0.1 || (FMath::IsNearlyEqual(Nearest, BestApproach, 0.1)
                    && Entrance.EntranceId.LexicalLess(BestEntrance->EntranceId))))
            {
                BestEntrance = &Entrance;
                BestApproach = Nearest;
                BestMembers.Reset();
                for (const auto& Member : ByRecipeOrder)
                {
                    if (Member.IsSet())
                    {
                        BestMembers.Add(Member.GetValue());
                    }
                }
            }
        }
        if (BestEntrance)
        {
            FWaveReservation& Group = Reservation.Emplace();
            Group.Id = ++LastGroupId;
            Group.Room = &Room;
            Group.Profile = Room.Profile;
            Group.EntranceId = BestEntrance->EntranceId;
            Group.Members = MoveTemp(BestMembers);
            Group.CreatedAt = Now;
            Group.DueAt = Now + WaveSettings->ReservationDelay;
            Group.bIntroduction = bIntro;
            LogGroup(Group, TEXT("reserve"), TEXT("eligible"));
            return;
        }
    }
}

void AGirlSaviorCombatEncounter::RevalidateAndCommit(double Now)
{
    const FWaveReservation& Pending = Reservation.GetValue();
    auto* Room = CurrentRoom.Get();
    auto* Profile = Room ? Room->Profile.Get() : nullptr;
    if (!Room || !Profile || Room != Pending.Room.Get() || Profile != Pending.Profile.Get())
    {
        CancelReservation(TEXT("room-or-profile-changed"));
        return;
    }
    if (!bViewValid || !LastOutsideApproach.IsSet() || !Profile->IsConfigurationValid(WaveSettings)
        || !WaveTimingAllows(*Profile, Now))
    {
        CancelReservation(TEXT("view-or-timing"));
        return;
    }

    TArray<ERole> Roles;
    for (const auto& Member : Pending.Members)
    {
        Roles.Add(Member.Role);
    }
    const bool bIntro = Profile->bHasIntroduction && !ServedIntroductions.Contains(Room->RoomId);
    const bool bRecipeAllowed = bIntro
        ? Roles.Num() == 1 && Roles[0] == Profile->IntroductionRole
        : Roles.Contains(ERole::Pursuer) && Profile->Recipes.ContainsByPredicate(
            [&](const auto& Recipe) { return ContainsOrderedRoles(Recipe.Roles, Roles); });
    if (bIntro != Pending.bIntroduction || !bRecipeAllowed
        || !GirlSaviorWavePolicy::CanAddGroup(*WaveSettings, *Profile, GetLivePopulation(), Roles))
    {
        CancelReservation(TEXT("occupancy-or-recipe"));
        return;
    }
    const auto* Entrance = Room->Entrances.FindByPredicate(
        [&](const auto& Candidate) { return Candidate.EntranceId == Pending.EntranceId; });
    for (const auto& Member : Pending.Members)
    {
        double Approach = 0.0;
        const TCHAR* Failure = TEXT("entrance-removed");
        if (!Entrance || !CheckSpawnSlot(*Room, *Entrance, Member, Approach, &Failure))
        {
            CancelReservation(Failure);
            return;
        }
    }

    // Keep each place reserved through BeginPlay until it is exchanged or reclaimed.
    const FWaveReservation Group = Pending;
    NextWaveQuery = Now + QueryInterval;
    TArray<TWeakObjectPtr<AGirlSaviorEnemyCharacter>> Spawned;
    for (const auto& Member : Group.Members)
    {
        if (RunState != EGirlSaviorRunState::Active || !Reservation.IsSet())
        {
            break;
        }
        const auto Class = EnemyClasses.FindRef(Member.Role);
        auto* Enemy = GetWorld()->SpawnActorDeferred<AGirlSaviorEnemyCharacter>(
            Class, Member.Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
        if (Enemy)
        {
            Enemy->Encounter = this;
            Enemy->FinishSpawning(Member.Transform);
        }
        bool bRegistered = false;
        if (IsValid(Enemy) && RunState == EGirlSaviorRunState::Active
            && Enemy->GetActorLocation().Equals(Member.Transform.GetLocation(), 1.0))
        {
            auto* SpawnRoom = FindRoomAt(Enemy->GetActorLocation());
            bRegistered = RegisterEnemy(*Enemy, SpawnRoom ? SpawnRoom : Room, false, Group.Id);
        }
        if (Reservation.IsSet())
        {
            Reservation->Members.RemoveAt(0);
        }
        if (bRegistered)
        {
            Spawned.Add(Enemy);
        }
        else if (IsValid(Enemy))
        {
            Enemy->Destroy();
        }
    }
    if (RunState != EGirlSaviorRunState::Active || !Reservation.IsSet())
    {
        return; // A spawn callback may have closed the run and cleared its reservation.
    }
    Reservation.Reset();
    if (Spawned.IsEmpty())
    {
        LogGroup(Group, TEXT("cancel"), TEXT("spawn-failed"));
        return;
    }
    LastCommittedGroupTime = Now;
    if (Group.bIntroduction)
    {
        ServedIntroductions.Add(Room->RoomId);
    }
    LogGroup(Group, TEXT("commit"), TEXT("revalidated"), Spawned.Num());
    for (const auto& Entry : Spawned)
    {
        if (Entry.IsValid())
        {
            ActivateRegisteredEnemy(*Entry.Get());
        }
    }
}

void AGirlSaviorCombatEncounter::CancelReservation(const TCHAR* Reason)
{
    if (Reservation.IsSet())
    {
        LogGroup(Reservation.GetValue(), TEXT("cancel"), Reason);
        Reservation.Reset();
        NextWaveQuery = GetWorld()->GetTimeSeconds() + QueryInterval;
    }
}

void AGirlSaviorCombatEncounter::LogGroup(const FWaveReservation& Group, const TCHAR* Action, const TCHAR* Reason, int32 Actual) const
{
    const auto Live = GetLivePopulation();
    FGirlSaviorWavePopulation Planned;
    for (const auto& Member : Group.Members)
    {
        Planned.Add(Member.Role);
    }
    const auto* Room = Group.Room.Get();
    UE_LOG(LogGirlSaviorWaves, Display,
        TEXT("t=%.3f %s group=%lld room=%s profile=%s entrance=%s intro=%d live=%d reserved=%d actual=%d liveS=%d liveC=%d plannedS=%d plannedC=%d reason=%s."),
        GetWorld()->GetTimeSeconds(), Action, Group.Id, Room ? *Room->RoomId.ToString() : TEXT("none"),
        *GetNameSafe(Group.Profile.Get()), *Group.EntranceId.ToString(), Group.bIntroduction,
        Live.Total, GetReservedEnemyCount(), Actual, Live.Shooters, Live.Chargers, Planned.Shooters, Planned.Chargers, Reason);
}
