"""Author the Stage 2 camera route in the existing Demo; preserves Stage 1 actors."""
import unreal
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name() == '/Game/Maps/Demo.Demo'
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
assert len(actors) < 100
existing = {a.get_actor_label(): a for a in actors}
v = unreal.Vector
rot = unreal.Rotator
def spawn(label, cls, location, yaw=0):
    actor = existing.get(label)
    if actor is None:
        actor = sub.spawn_actor_from_class(cls, v(*location), rot(pitch=0,yaw=yaw,roll=0))
        actor.set_actor_label(label)
    assert actor is not None
    actor.set_actor_location(v(*location), False, False)
    actor.set_actor_rotation(rot(pitch=0,yaw=yaw,roll=0), False)
    return actor
def cube(label, location, scale, material=None, collision=True):
    a = spawn(label, unreal.StaticMeshActor, location)
    m = a.static_mesh_component
    m.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(v(*scale))
    m.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    if material: m.set_material(0, unreal.load_asset(material))
    return a
cam_a = spawn('Stage2_LargeRoom', unreal.GirlSaviorCameraRoomBounds, (0,0,0))
cam_a.membership_box.set_box_extent(v(1200,1200,300), False)
cam_b = spawn('Stage2_SmallRoom', unreal.GirlSaviorCameraRoomBounds, (1800,0,0))
cam_b.membership_box.set_box_extent(v(600,500,300), False)
cube('Stage2_SmallRoomFloor',(1800,0,-25),(12,10,.5))
cube('Stage2_PartitionNorth',(1200,710,90),(.4,9.8,1.8))
cube('Stage2_PartitionSouth',(1200,-710,90),(.4,9.8,1.8))
cube('Stage2_SmallRoomNorth',(1800,510,65),(12.4,.2,1.3))
cube('Stage2_SmallRoomSouth',(1800,-510,30),(12.4,.2,.6))
# Leave a visible exit gap in the far end and mark it in cyan.
cube('Stage2_ExitNorth',(2410,350,65),(.2,3,1.3))
cube('Stage2_ExitSouth',(2410,-350,65),(.2,3,1.3))
cyan='/Game/Art/Materials/M_PrototypeCameraBoundary'
cube('Stage2_ExitMarker',(2300,0,3),(.15,3,.04),cyan,False)
cube('Stage2_ApproachMarker',(1030,-320,3),(.15,2,.04),cyan,False)
warn = spawn('Stage2_Warning',unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Core/BP_PrototypeWarningFixture'),(1650,100,0))
warn.trigger.set_relative_location(v(-480,-100,100),False,False)
warn.trigger.set_box_extent(v(100,160,100),False)
warn.set_editor_property('warning_seconds',.8)
cam_a.set_editor_property('warning_actors',[warn])
cam_b.set_editor_property('warning_actors',[warn])
occ = spawn('Stage2_Foreground',unreal.EditorAssetLibrary.load_blueprint_class('/Game/Blueprints/Core/BP_PrototypeForegroundOccluder'),(1410,-140,150),45)
# A distant cosmetic source stays outside the planned approach's frame.
cube('Stage2_OffscreenSource',(3200,-1200,100),(1,1,2),'/Game/Art/Materials/M_PrototypeCameraImpact',False)
for a in [cam_a,cam_b]:
    assert a.is_configuration_valid(), a.get_actor_label()
for a in sub.get_all_level_actors():
    if a.get_actor_label().startswith('Stage2_'):
        assert unreal.PinWrightPackageLibrary.mark_actor_package_dirty(a)
print('Stage2 route ready; original Stage1 actors preserved')
