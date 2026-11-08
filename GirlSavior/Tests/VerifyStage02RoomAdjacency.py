"""Controlled PIE regression for room geometry, height filtering and live WallInset edits."""
import unreal, math, json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
hero=unreal.GameplayStatics.get_player_pawn(world,0)
camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
rooms=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GirlSaviorCameraRoomBounds)
assert len(rooms)==2
neighbor=max(rooms,key=lambda a:a.get_actor_location().x)
settings=camera.get_editor_property('camera_settings')
movement=hero.get_movement_component()
original=(hero.get_actor_location(),movement.movement_mode,neighbor.get_actor_transform(),settings.get_editor_property('wall_inset'))
movement.stop_movement_immediately()
movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
cases=[('touching',1800,0,300,1100,1100),('different_floor',1800,1000,300,1100,900),
       ('gap_below_inset',2099,0,300,1100,1100),('gap_equal_inset',2100,0,300,1100,900),
       ('live_inset',1800,0,150,-1100,-1050)]
state={'index':0,'handle':None,'frames':0,'started':0}
result={'samples':[],'failures':[]}
offset=math.cos(math.radians(35))*math.sqrt(.5)*1600
def begin():
    name,x,z,inset,body,expected=cases[state['index']]
    neighbor.set_actor_location(unreal.Vector(x,0,z),False,True)
    settings.set_editor_property('wall_inset',inset)
    hero.set_actor_location(unreal.Vector(body,0,90),False,True)
    camera.request_follow_snap()
    state['started']=unreal.GameplayStatics.get_time_seconds(world)
def tick(dt):
    try:
        state['frames']+=1
        elapsed=unreal.GameplayStatics.get_time_seconds(world)-state['started']
        if elapsed<.1 and state['frames']<2000: return
        name,x,z,inset,body,expected=cases[state['index']]
        actual=camera.get_camera_location().x+offset
        result['samples'].append({'case':name,'expected_anchor_x':expected,'actual_anchor_x':actual})
        if not math.isfinite(actual) or abs(actual-expected)>.1: result['failures'].append(name)
        state['index']+=1
        if state['index']<len(cases) and state['frames']<2000:
            begin()
            return
        if state['index']<len(cases):result['failures'].append('frame_budget')
    except Exception as error:
        result['failures'].append(str(error))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    try:
        neighbor.set_actor_transform(original[2],False,True)
        settings.set_editor_property('wall_inset',original[3])
        hero.set_actor_location(original[0],False,True)
        movement.set_movement_mode(original[1])
        camera.request_follow_snap()
    except Exception as error:
        result['failures'].append('restore: '+str(error))
    result['passed']=not result['failures']
    (Path(unreal.Paths.project_saved_dir())/'Stage2'/'RoomAdjacency.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('Room adjacency live checks passed='+str(result['passed']))
begin()
state['handle']=unreal.register_slate_post_tick_callback(tick)
