"""Observe real post-camera aim ticks. Run only in focused PIE; restores transient state."""
import json
from pathlib import Path
import unreal

world = unreal.EditorLevelLibrary.get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
hero = unreal.GameplayStatics.get_player_character(world, 0)
aim = hero.get_aim_component()
assert pc.can_use_gameplay_input(), 'Focused foreground gameplay viewport required'
original = hero.get_actor_location()
rotation = hero.get_actor_rotation()
mode = hero.get_movement_component().movement_mode
available = pc.get_gameplay_input_available()
mouse = pc.get_mouse_position()
hero.get_movement_component().stop_movement_immediately()
hero.get_movement_component().set_movement_mode(unreal.MovementMode.MOVE_NONE)
hero.set_actor_location(unreal.Vector(0, -300, 90), False, True)
pc.player_camera_manager.request_follow_snap()
pc.set_mouse_location(750, 200)
state = {'frame': 0, 'handle': None, 'saved_direction': None}
result = {'samples': [], 'failures': [], 'tracking_samples': 0, 'suspended_samples': 0, 'paused_samples': 0}

def observe(delta):
    try:
        frame = state['frame']
        state['frame'] += 1
        direction = aim.get_aim_direction()
        if 3 <= frame < 33 or 42 <= frame < 47 or 55 <= frame < 60:
            assert pc.can_use_gameplay_input(), 'live control gate unexpectedly unavailable'
            assert aim.is_cursor_inside_game_view(), 'actual tick did not sample cursor'
            ray = pc.deproject_mouse_position_to_world()
            assert ray, 'deprojection unavailable'
            origin, forward = ray
            body = hero.get_actor_location()
            t = (body.z - origin.z) / forward.z
            hit = origin + forward * t
            dx, dy = hit.x - body.x, hit.y - body.y
            length = (dx * dx + dy * dy) ** .5
            dot = (direction.x * dx + direction.y * dy) / length
            result['samples'].append({'frame': frame, 'dot': dot, 'body': [body.x, body.y, body.z],
                                      'camera': str(pc.player_camera_manager.get_camera_location())})
            assert dot > .99999, 'aim does not match final camera cache in current frame'
            result['tracking_samples'] += 1
        if 35 <= frame < 40 or 49 <= frame < 53:
            assert not pc.can_use_gameplay_input(), 'suspended/paused gate still available'
            previous = state['saved_direction']
            assert (direction - previous).length() < .00001, 'interruption changed last facing'
            if frame < 40:
                assert not aim.is_cursor_inside_game_view(), 'suspension failed to invalidate cursor'
                result['suspended_samples'] += 1
            else:
                result['paused_samples'] += 1
        if frame < 32:
            hero.set_actor_location(unreal.Vector(frame * 12, -300 + frame * 3, 90), False, True)
        if frame == 33:
            state['saved_direction'] = direction
            pc.set_gameplay_input_available(False)
            pc.set_mouse_location(250, 180)
        if frame == 40:
            pc.set_gameplay_input_available(True)
        if frame == 47:
            state['saved_direction'] = direction
            unreal.GameplayStatics.set_game_paused(world, True)
            pc.set_mouse_location(600, 550)
        if frame == 53:
            unreal.GameplayStatics.set_game_paused(world, False)
        if frame < 61:
            return
    except Exception as error:
        result['failures'].append(str(error))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    unreal.GameplayStatics.set_game_paused(world, False)
    pc.set_gameplay_input_available(available)
    hero.set_actor_location(original, False, True)
    hero.set_actor_rotation(rotation, True)
    hero.get_movement_component().set_movement_mode(mode)
    pc.player_camera_manager.request_follow_snap()
    if mouse:
        pc.set_mouse_location(int(mouse[0]), int(mouse[1]))
    result['passed'] = not result['failures'] and result['tracking_samples'] >= 35 and result['paused_samples'] == 4 and result['suspended_samples'] == 5
    path = Path(unreal.Paths.project_saved_dir()) / 'Stage3' / 'LiveAim.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2), encoding='utf-8')

state['handle'] = unreal.register_slate_post_tick_callback(observe)
print('Stage3 live aim observer scheduled; result: Saved/Stage3/LiveAim.json')
