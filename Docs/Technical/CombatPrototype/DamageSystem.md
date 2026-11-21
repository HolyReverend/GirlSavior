# Damage, protection, and death

**Stage CP1.** Extend the existing `UGirlSaviorDamageReceiverComponent`. Give the hero and enemies one receiver each. Attackers decide when and where contact occurs. The receiver decides what that contact changes. Start with the pistol, three ordinary attacks, and a configurable barrier fixture for verification.

**Design inputs.** [Pistol](../../GameDesign/Mechanics/Weapons/Pistol.md) · [Defensive dash](../../GameDesign/Mechanics/Player/CharacterCharge.md) · [Resistances](../../GameDesign/Mechanics/Progression/HeroProgression.md#пассивки-героя-и-оружие) · [Barrier and seal](../../GameDesign/Mechanics/Heroines/Saira.md#способности) · [Death and lethal cause](../../GameDesign/Mechanics/Encounters/DeathFeedback.md).

## 1. Add the native contract

Keep the following records under `Source/GirlSavior/Combat/`. Use reflected structs for data exposed to Blueprint and const snapshots for listeners. Keep mutable state private.

| Type or API | Required fields or behavior |
| --- | --- |
| `EGirlSaviorDamageDelivery` | `DirectProjectile`, `DirectMelee`, `DirectCharge`, `Area`, `FloorHazard`. Delivery determines dash eligibility. The damage portions independently determine resistance. |
| `FGirlSaviorDamageSpec` | Physical and magical amounts, Delivery, Source faction (`Hero`, `Enemy`, or `Neutral`), a stable source ID (`FName`), a localized cause (`FText`), optional weak instigator and causer, AttackId for correlation, origin/contact position and normal when known. Construct it in C++ at delivery, with source identity and amounts copied at launch for detached projectiles. |
| `FGirlSaviorDamageResult` | Input snapshot, Outcome (`Applied`, `BlockedByDash`, `BlockedBySafeArea`, `Rejected`), HealthBefore/After, BarrierBefore/After, HealthLost, BarrierLost, `bBarrierBroken`, `bKilled`. Use actual deltas after clamping. |
| `UGirlSaviorDamageReceiverComponent::ResolveDamage` | Single new native entry point returning a result. Use `OnDamageResolved` for accepted or protected contact, plus a native one-time `OnDied` for the structural death transition. Provide read-only vitals and the last lethal result. Initialization sets maxima and current values once before combat activation. |

Use hero **100 HP**, **0 barrier**, and zero resistance by default. For a separate authored test fixture, expose initial barrier and resistance (e.g. barrier 4). On actual runtime combatants require finite positive MaxHealth, finite nonnegative Barrier, and resistances in `[0, 1]`. Apply tuning at spawn and clamp vitals. Set Barrier to zero for all three prototype enemies. An already running hit uses its original source data and the target's live defenses.

For PIE, place a damage fixture in a side lane or use the native test probe pattern from [PrototypeCombatAcceptance.cpp](../../../GirlSavior/Source/GirlSavior/Tests/PrototypeCombatAcceptance.cpp). Configure its amount, delivery, and neutral source. Each trigger or probe invocation issues one `ResolveDamage` call to the live hero. Start from a fresh fixture state for each barrier case. Use the same probe to check area damage during a real dash and to sample before/after early interruption.

### Preserve the Stage 4 entry point

Keep the serialized `EGirlSaviorDamageKind`, `TryApplyDamage(Kind, Amount)`, `OnDamageAccepted` and `AcceptedDamageTotal`. Route the legacy call through `ResolveDamage` as one physical portion. If Stage 5 has since added enum entries, map each existing entry explicitly. Preserve the default Stage 4 demonstration receiver as an accumulating test fixture and enable vitals explicitly for the combat hero, enemies and health target. The fixture records every accepted amount and emits the legacy delegate once. Runtime death and damage feedback use the result snapshot. Check the existing `BP_TestTarget` bindings and the Stage 4 combat probe before removing any compatibility API.

## 2. Resolve every hit in the same order

1. On the game thread, reject a dead/inactive target, an invalid delivery, a non-finite or negative portion, or a nonpositive sum. Validate the sum in double precision. Reject self and same-faction hits for configured hero/enemy teams. Allow explicitly neutral world hazards and Stage 4 fixtures. Reject attacks after the run has ended. Keep inactive, rejected and friendly contacts out of the damage presentation event.
2. Check the combat boundary. In `CombatPrototype`, require the target and attack contact to be on the allowed side of the safe pocket. Use the live safe volume for target protection. Use the physical or query boundary along the travelled segment so a shot cannot cross and exit a protected area in one frame. Centralize volume ownership and checks in the encounter and reuse them for spawns, navigation intent and attacks. Extend the Stage 4 fire gate with incoming protection and interception in this map.
3. For a hero with `IsDashProtected()`, return `BlockedByDash` for `DirectProjectile`, `DirectMelee` and `DirectCharge`. The prototype treats body contact in a charge as a direct hit. `Area` and `FloorHazard` go through damage calculation. Query the live Stage 5 movement state at contact time, including early interruption and the strict end deadline. A protected physical collision still stops a projectile or a charge.
4. Compute `Adjusted = Physical * (1 - PhysicalResistance) + Magical * (1 - MagicalResistance)`. Apply an already active incoming multiplier after resistance. The prototype multiplier is **1.0**. An adjustment to zero leaves both vitals intact. Reserve the multiplier seam for the future seal. Future seal triggers consume the resolved result after its application and affect later hits.
5. Compute `BarrierLost = min(Barrier, Adjusted)`, `HealthLost = min(Health, Adjusted - BarrierLost)`. Commit both values, clamp to their bounds, and set the death latch when Health crosses from above zero to zero. Set `bBarrierBroken` for a nonzero BarrierBefore and zero BarrierAfter. Record the first lethal snapshot before any notification. Saira's recharge and threshold-triggered seal belong to future support code.
6. Deliver structural death once, with health and the lethal cause already committed, and then deliver the final `OnDamageResolved` snapshot. Route legacy accepted-damage notification at most once per accepted call. Keep listener callbacks observational for damage. Reject synchronous re-entry during dispatch and route later effects as separate actions. Hold presentation subscriptions through the final hit dispatch. Defer actor teardown that would invalidate a dispatch until it returns.

| Starting values and contact | Result |
| --- | --- |
| HP 100, barrier 4, adjusted 3 | Barrier 1, HP 100, absorption cue |
| HP 100, barrier 4, adjusted 4 | Barrier 0, HP 100, barrier break |
| HP 100, barrier 4, adjusted 7 | Barrier 0, HP 97, break and hurt |
| HP 4, barrier 0, adjusted 10 | HP 0, `HealthLost = 4`, one death with the original source |
| Physical 10 at 10% and magical 10 at 20%, with barrier 4 | Adjusted 17, barrier 0, HP drops by 13 |
| Hero protected, incoming projectile, melee or charge | Both deltas zero, protected contact with no injury cue |

## 3. Use one projectile pipeline

- Extend the existing `AGirlSaviorTestProjectile` with editable delivery settings and a native launch snapshot. Keep `USphereComponent` as the swept root and `UProjectileMovementComponent` for straight travel. Set the owner, collision profile, radius, speed, and source snapshot before enabling movement. Use one native deferred-spawn helper for the hero and Shooter and keep the existing `UGirlSaviorFireComponent` scheduling and `OnShotCue`. Blueprint children provide visuals.
- Retain existing Demo projectile defaults, including the **1 damage**, **2000 cm/s**, and **1.5 s** fixture. Author `BP_CombatPistolProjectile` and `BP_EnemyProjectile` as direct children of the shared native projectile class. The combat pistol uses **10 physical damage**, **2000 cm/s**, **6 cm** radius, **1400 cm** travel budget, and **4 shots/s** through the inherited fire component. For the straight combat variants, cap the last movement tick to `RemainingDistance / Speed` in the movement component before calling its parent implementation. After the call, if the projectile is still active, measure actual travel and expire on exhaustion. Use a separate game-time cleanup bound well above the expected flight time and verify normal range expiry at supported frame rates. Preserve the legacy lifetime path for the Demo fixture.
- Keep `PrototypeProjectile` on its existing object channel. Add an `EnemyProjectile` object channel with default Ignore, using a free slot verified in `DefaultEngine.ini`. Both spheres use Query Only and block WorldStatic and WorldDynamic with matching responses on real blockers. Both sphere profiles also block Pawn. Enemy capsules block the hero projectile channel and ignore the enemy projectile channel. The hero capsule does the inverse. Cosmetic meshes and other projectiles ignore both. Keep the first blocking geometry authoritative. Verify both sides of these response pairs in the editor.
- Use a capsule-center launch origin and simple collision with capsule clearance greater than the sphere radius. Ignore the owner before the first sweep. Reject initial penetration against blocking geometry with an immediate geometry impact at the near side. Never place a projectile beyond cover by moving a muzzle or a spawn point through it. If the collision configuration changes, recheck the old Stage 4 wall, target, and pawn cases. [Epic Projectile Movement API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UProjectileMovementComponent)
- For the first blocking stop, latch completion, find the receiver, resolve once, then publish `OnResolvedImpact` with Geometry or Target contact plus the resolved result when present. Keep the old `OnImpactCue(Location, bHitTarget)` adapter for Demo. Resolve expiry and cleanup through the same one-time terminal path, with a separate completion notification for attack-slot release. Expiry produces no impact cue. EndPlay releases ownership by handle as a fallback for externally destroyed shots.

## 4. Connect death to the run

- Enemy death stops navigation and attached melee/charge, clears the warning, disables collision and releases its live registration immediately. The cosmetic body can remain on the map. Preserve a detached Shooter projectile's snapshot and slot until that projectile ends. Use the stable causes `Enemy.Pursuer.Melee` («Удар преследователя»), `Enemy.Shooter.Projectile` («Снаряд стрелка»), and `Enemy.Charger.Charge` («Бросок таранщика»).
- Hero death latches Defeat with the exact lethal result. Stop movement and dash, use `SetGameplayInputAvailable(false)` to cancel input and firing, and ask the encounter to close the run. Finish current dispatch safely, terminate the remaining threats, and show the cause from the stored snapshot. Use «Смертельное попадание» if the cause is missing. Hooks for future rewards or expedition results receive this transition once.

## 5. Verify the contract

- Automation covers inputs including NaN/infinity, both resistance portions, zero after resistance, dash admission/expiry/early stop, safe pocket, barrier cases including lethal overflow, and multiple attempts against zero HP. Assert both numeric results and event counts. Confirm the lethal source is preserved after the instigator is destroyed.
- In PIE, shoot close to a wall and through an open door, hit a live enemy, fire through a dead body's position, and observe a full-range miss at normal and low frame rates. Run the existing Demo combat probe. Verify the current shot schedule across death and restart. [Pending projectile lifetime check](../../OpenItems/GS-012-ProjectileLifetime.md)
- Use the health target and one Pursuer first. Connect all three enemy attacks through this contract in CP2. Compare output with [feedback outcomes](CombatFeedback.md) and confirm final ownership with the [implementation plan](ImplementationPlan.md).
