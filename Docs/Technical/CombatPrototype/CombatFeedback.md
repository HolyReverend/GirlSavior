# Combat prototype — Feedback and presentation

**Status.** CP4 presentation is wired in `CombatPrototypeRoute` on Unreal Engine 5.9. The earlier `CombatPrototype` map remains available for isolated CP1 and CP2 checks.

Follow the [stage plan](ImplementationPlan.md) and consume the contracts in [Damage](DamageSystem.md), [Basic enemies](BasicEnemies.md), and [Waves](EnemyWaves.md). Gameplay code decides when a shot, attack, hit, or death occurs. Blueprints, materials, audio, and widgets display those decisions.

Design sources are [Shot feedback](../../GameDesign/Mechanics/Weapons/ShotFeedback.md), [Enemy attack feedback](../../GameDesign/Mechanics/Encounters/EnemyAttackFeedback.md), and [Death feedback](../../GameDesign/Mechanics/Encounters/DeathFeedback.md).

## Step 1 — Wire a minimal presentation pass

1. In `Content/Maps/TestMaps/CombatPrototypeRoute.umap`, place one `BP_CombatFeedbackPresenter` with an explicit reference to `AGirlSaviorCombatEncounter`. The presenter configures the local HUD and owns active warning visuals and cosmetic bindings. Subscribe before the encounter enables combat. Include any already registered combatants after binding and make registration idempotent. Keep each receiver bound through the dispatch of its lethal result even if the living roster removes it during that dispatch. Release its binding on actor teardown. Unbind, remove the widget, cancel presentation timers, and clear spawned visual references when the presenter ends play. A new PIE world or retry creates fresh bindings.
2. Author the presenter, `BP_CombatWarning`, and the visual projectile variants under `Content/Blueprints/Combat/`. Add `WBP_CombatHUD` and `WBP_CombatOutcome` under `Content/Blueprints/Combat/UI/`. Put reusable cue materials and audio under `Content/Art/Materials/Combat/` and `Content/Audio/Combat/` as needed. Expose cue asset references and cosmetic timing as Blueprint defaults. Attack geometry, timing, and damage use the gameplay Data Assets.
3. Start with flat floor strips or simple mesh sections, visible projectile meshes, and three clearly different silhouettes. Give warnings, projectile effects, and corpse visuals no collision, no navigation influence, and small bounded lifetimes. Give the greybox room a dim ambient baseline and attach a small point light to the hero as a Torch readability fixture. Test in the actual gameplay view with the map's normal foreground fading and a pillar.
4. CP1 needs one readable sector, one player shot trail, a hit pulse, a health readout, an unmistakable enemy death, and a minimal defeat panel with working Retry. CP2 adds the line, charge lane, enemy projectile trail, and their cancellation. A room fails attack acceptance if its geometry hides a warning that the player needs to react to. Complete these checks before evaluating waves in CP3.

## Step 2 — Consume gameplay events

| Event source | Presentation response |
| --- | --- |
| `UGirlSaviorFireComponent::OnCombatShotCue` | Short local muzzle pulse, dry pistol sound, and small additive kick on the visible weapon or its mesh. Use the launch direction for the cue. At point-blank range place the effect on the actor's side of the blocker. Keep camera, aim, collision, and launch origin independent of the cosmetic muzzle. |
| Moving player projectile | Bright thin head and short trail sampled over distance traveled. Moving enemy projectile uses a larger elongated head and dense short tail. Use the collision root for the head. Fade the last traveled trail at stop or expiry. |
| `OnResolvedImpact` on a projectile | Spawn geometry contact at the resolved position, oriented by impact normal. Start with dull debris and a local thud. If the map has stone and metal physical materials, give metal a narrow spark and short ring. Use the target receiver for target reactions. Free-flight expiry fades silently. |
| `OnDamageResolved` on a live combatant's receiver | Read the result snapshot. Accepted health loss produces a brief directed hit accent and one body-impact sound. Hero reaction is a short overlay while moving. Enemy reaction is additive and leaves the attack pose and gameplay phase intact. |
| `OnDamageResolved` with `BarrierLost > 0` | A short arc follows the perimeter at the hero's feet. Play a high resonance for a surviving barrier. On `bBarrierBroken`, split the perimeter into a few segments and play a distinct break sound. If `HealthLost > 0` also show the health reaction unless the hit is lethal, in which case the death cue replaces it. Apply both meter changes from the same snapshot. |
| `BlockedByDash`, `BlockedBySafeArea`, or `Rejected` | Keep the dash protection silhouette tied to the real protection window. A dash-protected hit keeps the attack's own cue and omits received-damage accents. A safe boundary blocker may use a short ward contact cue. Ignore rejected damage as a target reaction. |
| `OnDamageResolved` with `bKilled` | Play one finisher cue per target, using the contact position and the lethal snapshot. Start the fall even when the enemy lies outside Torch light. A visible local mark and audible falling accent give confirmation. |
| Attack preparation, update, and end from `UGirlSaviorEnemyAttackComponent` | Create, mutate, and release one warning keyed by `AttackId` within the current encounter. Use the exact shapes and phases in Step 3. |

Keep `BP_TestProjectile.OnImpactCue` and `UGirlSaviorFireComponent::OnShotCue` wired for Demo compatibility. A launch dispatches the cue for its active presentation route once. An enemy hit generates one receiver reaction even when the projectile publishes its own contact classification. Impact audio and short-lived effects persist independently of the projectile.

## Step 3 — Show the real warning shape and its lifetime

Use the attack snapshot to build geometry in world space at the floor. For the greybox, use reusable unlit mesh strips or triangles with a small height offset on sampled floor points. Depth-test materials against level geometry, including cover. Draw the supplied clipped polygon for the sector and the supplied endpoints and width for lines and stripes. Keep the warning visible in dark parts of the frame using emissive surfaces with no world illumination. Keep the attacker's silhouette dependent on the Torch.

| Attack | During preparation | When active |
| --- | --- | --- |
| Pursuer | Compact floor sector with a solid outer arc and a local windup sound. Use a clear pose pause and arm or claw motion. | Remove the warning on the resolved swing. Show a brief arc and swish even on a miss. Contact sound follows the damage result. |
| Shooter | Thin floor line with a clearer outline when aim locks. Use the current geometry clip. Play the warning sound once at the shooter. | Swap the line for the visible projectile on successful launch. Release the warning on abort or launch failure. The projectile keeps its own visual through normal impact or expiry when the shooter dies. |
| Charger | Wide floor strip with distinct lateral edges, a crouch pose, and a short priming sound. Use the fixed lane once locked. | Move the strip start with the charger and shorten the remaining danger area. End it at stop and show a short local dust or impact cue at a wall. |

Update the material and geometry on native phase or geometry events. A flash near the end of windup is a cosmetic parameter derived from the snapshot's phase deadline. It leaves the collision shape and active time intact. Keep the visible part of a live warning when its owner exits the frame after start. Clear the visual on cancellation, attack completion, owner removal for body-attached attacks, and encounter shutdown. A detached projectile can outlive its owner and retains its own visuals until the projectile ends. Attack slot ownership stays with gameplay.

Use outlines and shape for recognition with audio muted. Keep sparks, dust, muzzle flashes, and bodies secondary to open paths and upcoming attacks. Test localized cue assets without broad dynamic lights or a screen post-process that reveals obscured enemies. If world geometry hides all useful parts of a warning from the gameplay camera, fix room placement and validate the attack start rule with the same room.

## Step 4 — Bind HUD, death, and retry

- Create the HUD with the owning local controller, initialize its health and barrier from the possessed hero receiver, and update from resolved damage and state-change events. Show current and maximum health, the barrier while configured, and the Stage 5 dash state when available in the integration baseline. Consume the actual dash readiness/protection queries. Sample only the remaining displayed cooldown while it is counting down. Clear possession-dependent state on pawn change and unsubscribe when the widget is removed.
- Play the enemy's short fall on its mesh or a collision-free visual proxy after native death removes it from the living roster and disables blocking. Use a distinctive final pose for each role. Keep the corpse visible for at least 3 game seconds, then allow removal outside the gameplay frame. Around 10 game seconds, fade any remaining corpse in view and remove it. These are prototype tuning targets. Corpses and cues have no effect on damage, navigation, or spawn capacity.
- On lethal hero damage, consume the encounter's committed defeat snapshot, including the immutable lethal source label, to start the fall and show `Герой погиб` and the cause. Use localized `FText` for `Удар преследователя`, `Снаряд стрелка`, `Бросок таранщика`, and fallback `Смертельное попадание`. Keep the same camera and hero light through the fall. Prioritize the lethal hit audio, then lower combat audio briefly.
- After a proposed **1.25 game seconds**, show the outcome panel in defeat mode with the same cause and a `Повторить` button. On success, open the same panel immediately with `Выход достигнут` and the retry button. Keep gameplay input closed. Route button focus through UI input, then reload the current map using the normal level restart path. Disarm the button after the first click. Keep defeat delay in game time with combat shut down by the encounter. A world pause freezes the delay. The prototype panel ends this map run. Expedition results, persistence, and rewards are future integrations.

## CP4 authored setup

- The presenter uses world-depth line components for warnings, impacts, trail fades, and the dash protection outline. The attack snapshots own the floor geometry. The native UMG parents own the health, configured barrier, dash status, immediate death notice, and modal result.
- The route uses fixed exposure at EV 0, directional fill of 0.09 lux, and skylight intensity 0.035. The warm Torch starts at 3600 lm with a 950 cm radius, 110 cm above the capsule center. It stays with the hero through defeat. Only the hero mesh's own shadow is suppressed to keep the floor under the hero readable.
- `Content/Audio/Combat` contains 13 original short cues and `ATT_CombatCue`. Positional attenuation uses the local hero as the listener. The defeat sting plays in the foreground while active combat audio fades out.
- `DA_HeroCombatBarrierFixture` supplies 4 barrier for isolated acceptance sessions. The regular route keeps its authored hero vitals.

## Acceptance in CombatPrototype PIE

| Check | Expected observation |
| --- | --- |
| Fire in motion and during Stage 5 dash. Tap on cooldown and repeat at point-blank cover. | Actual launches produce one visual impulse and sound each. Cooldown taps stay silent. Cover stops the projectile and the trail stays on the launch side. |
| Shoot an exposed target and a blocker, then let a shot expire in a clear corridor. | Damage, geometry, and silent expiry look distinct. A lethal contact takes precedence over an ordinary hit pulse. Check projectile lifetime as part of [GS-012](../../OpenItems/GS-012-ProjectileLifetime.md). |
| Face each enemy alone with dash unavailable. Repeat with mixed waves at the light edge and beside a pillar. Move so an attacker leaves the frame after starting its attack. | Sector, line, and strip are distinct with muted sound. Walking can clear each first warning. The useful part of an active warning remains visible when its source leaves the view. Locked line stays fixed when the player ducks behind cover. Charger ends at the wall. |
| With barrier set to 4 for repeatable tests, resolve hits of 3, 4, and 7 from a fresh fixture state, then repeat with no barrier and with active dash protection. | Outcomes respectively show survival, break, and break plus 3 lost health. Direct damage during protection leaves both meters intact. HUD matches the receiver on each hit. |
| Kill an enemy in prep, kill a shooter after release, and walk and shoot through a body. | The first warning ends immediately. The released shot still follows its original path. The body never blocks and is eventually removed. |
| Die to each basic attack, including a projectile whose shooter is already dead. Land two close hits, first breaking the barrier and second killing the hero. | One defeat transition occurs. Both death displays name the lethal attack from its snapshot. Retry resets warnings, effects, HUD, and defeat state with one binding per event. |
| Reach the exit with a live warning or pending reinforcement, then use Retry. | The success panel appears and waves, attacks, and warnings end. The fresh run restores combat and uses one binding per event. |

Record the map, camera/light setup, cue and timing observations, and manual results in the project work report. Track prototype evidence against [GS-027](../../OpenItems/GS-027-ShotFeedbackPlaytest.md), [GS-028](../../OpenItems/GS-028-EnemyAttackFeedbackPlaytest.md), and [GS-029](../../OpenItems/GS-029-DeathFeedbackPlaytest.md). Their full closure also requires the later weapon, support, and expedition systems listed in those cards.
