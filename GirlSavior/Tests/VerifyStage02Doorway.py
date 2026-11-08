"""PIE regression: a swept free doorway route must keep the camera advancing both ways."""
import unreal
import json
import math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
hero=unreal.GameplayStatics.get_player_pawn(world,0)
camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
movement=hero.get_movement_component()
original=hero.get_actor_location()
original_mode=movement.movement_mode
movement.stop_movement_immediately()
movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
# These are traversable approach/destination positions, independent of the transition's current extents.
forward=[900,1000,1100,1200,1300,1400,1500,1600,1710,1800]
points=forward+list(reversed(forward[:-1]))
hero.set_actor_location(unreal.Vector(points[0],100,90),False,True)
camera.request_follow_snap()
state={'index':0,'started':unreal.GameplayStatics.get_time_seconds(world),'frames':0,'handle':None}
result={'samples':[],'failures':[]}
def tick(delta):
    try:
        state['frames']+=1
        elapsed=unreal.GameplayStatics.get_time_seconds(world)-state['started']
        if elapsed<.35 and state['frames']<4000: return
        actual=hero.get_actor_location()
        sample={'target_x':points[state['index']],'hero_x':actual.x,'hero_y':actual.y,
                'camera_x':camera.get_camera_location().x,'elapsed':elapsed}
        result['samples'].append(sample)
        if not all(math.isfinite(value) for value in [actual.x, actual.y, sample['camera_x']]):
            result['failures'].append('Non-finite hero or camera position')
        if abs(actual.x-sample['target_x'])>1 or abs(actual.y-100)>1:
            result['failures'].append('Unexpected collision on free doorway route')
        if len(result['samples'])>1:
            previous=result['samples'][-2]
            sign=1 if sample['target_x']>previous['target_x'] else -1
            advance=(sample['camera_x']-previous['camera_x'])*sign
            sample['camera_advance']=advance
            if not math.isfinite(advance) or advance<=1:
                result['failures'].append('Camera stopped between X=%s and X=%s (advance %.4f cm)'%
                                          (previous['target_x'],sample['target_x'],advance))
        state['index']+=1
        if state['index']<len(points) and state['frames']<4000:
            hero.set_actor_location(unreal.Vector(points[state['index']],100,90),True,False)
            state['started']=unreal.GameplayStatics.get_time_seconds(world)
            return
        if state['index']<len(points): result['failures'].append('Frame budget exhausted')
        result['passed']=not result['failures']
    except Exception as error:
        result.update({'passed':False,'error':str(error)})
    unreal.unregister_slate_post_tick_callback(state['handle'])
    hero.set_actor_location(original,False,True)
    movement.set_movement_mode(original_mode)
    camera.request_follow_snap()
    output=Path(unreal.Paths.project_saved_dir())/'Stage2'/'DoorwayFollow.json'
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('Doorway follow regression passed='+str(result.get('passed')))
state['handle']=unreal.register_slate_post_tick_callback(tick)
