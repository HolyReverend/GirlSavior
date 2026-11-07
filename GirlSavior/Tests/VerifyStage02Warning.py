"""Run in PIE through PinWright. Bounded world-time warning/impact observation."""
import unreal
import json
from pathlib import Path
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
hero = unreal.GameplayStatics.get_player_pawn(world,0)
pc = unreal.GameplayStatics.get_player_controller(world,0)
fixtures = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GirlSaviorPrototypeWarningFixture)
assert len(fixtures)==1
warning=fixtures[0]
original=hero.get_actor_location()
hero.get_movement_component().stop_movement_immediately()
hero.get_movement_component().set_movement_mode(unreal.MovementMode.MOVE_NONE)
hero.set_actor_location(unreal.Vector(900,0,90),False,True)
pc.player_camera_manager.request_follow_snap()
assert warning.get_editor_property('hidden')
start=unreal.GameplayStatics.get_time_seconds(world)
hero.set_actor_location(unreal.Vector(1170,0,90),False,True)
result={'trigger_active_immediately':not warning.get_editor_property('hidden'),'samples':[]}
state={'handle':None,'frames':0,'last':0,'max_dt':0}
def observe(delta):
    try:
        state['frames']+=1
        elapsed=unreal.GameplayStatics.get_time_seconds(world)-start
        state['max_dt']=max(state['max_dt'],elapsed-state['last'])
        state['last']=elapsed
        cue=any(a.get_life_span()>0 for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor))
        result['samples'].append({'elapsed':elapsed,'hidden':warning.get_editor_property('hidden'),'impact':cue})
        if elapsed < 1.2 and state['frames']<600:
            return
        tolerance=state['max_dt']+.01
        hidden=next((s['elapsed'] for s in result['samples'] if s['hidden']),None)
        impact=next((s['elapsed'] for s in result['samples'] if s['impact']),None)
        clear=next((s['elapsed'] for s in result['samples'] if impact is not None and s['elapsed']>impact and not s['impact']),None)
        result.update({'hidden_at':hidden,'impact_at':impact,'impact_cleared_at':clear,'max_dt':state['max_dt']})
        result['passed']=bool(result['trigger_active_immediately'] and hidden is not None and impact is not None
                              and .8-tolerance<=hidden<=.8+tolerance and .8-tolerance<=impact<=.8+tolerance
                              and clear is not None and .15-tolerance<=clear-impact<=.15+tolerance)
    except Exception as error:
        result.update({'passed':False,'error':str(error)})
    unreal.unregister_slate_post_tick_callback(state['handle'])
    hero.set_actor_location(original,False,True)
    hero.get_movement_component().set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    pc.player_camera_manager.request_follow_snap()
    path=Path(unreal.Paths.project_saved_dir())/'Stage2'/'WarningTiming.json'
    path.write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('Stage2 warning timing passed='+str(result.get('passed')))
state['handle']=unreal.register_slate_post_tick_callback(observe)
