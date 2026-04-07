find_package(Threads REQUIRED)

add_executable(engine_smoke_test
    engine_smoke_test.cpp
)

target_link_libraries(engine_smoke_test
PRIVATE
    engine
)

add_test(
    NAME engine_smoke_test
    COMMAND engine_smoke_test
)

add_executable(engine_reset_phases_test
    engine_reset_phases_test.cpp
)

target_link_libraries(engine_reset_phases_test
PRIVATE
    engine
)

add_test(
    NAME engine_reset_phases_test
    COMMAND engine_reset_phases_test
)

add_executable(fixed_step_simulation_test
    fixed_step_simulation_test.cpp
)

target_link_libraries(fixed_step_simulation_test
PRIVATE
    engine
)

add_test(
    NAME fixed_step_simulation_test
    COMMAND fixed_step_simulation_test
)

add_executable(asset_root_resolution_test
    asset_root_resolution_test.cpp
)

target_link_libraries(asset_root_resolution_test
PRIVATE
    engine
)

add_test(
    NAME asset_root_resolution_test
    COMMAND asset_root_resolution_test
)

add_executable(executable_dir_test
    executable_dir_test.cpp
)

target_link_libraries(executable_dir_test
PRIVATE
    engine
)

add_test(
    NAME executable_dir_test
    COMMAND executable_dir_test
)

add_executable(software_engineering_policy_test
    software_engineering_policy_test.cpp
)

target_link_libraries(software_engineering_policy_test
PRIVATE
    engine
)

add_test(
    NAME software_engineering_policy_test
    COMMAND software_engineering_policy_test
)

add_executable(memory_layout_test
    memory_layout_test.cpp
)

target_link_libraries(memory_layout_test
PRIVATE
    engine
)

add_test(
    NAME memory_layout_test
    COMMAND memory_layout_test
)

add_executable(hardware_system_test
    hardware_system_test.cpp
)

target_link_libraries(hardware_system_test
PRIVATE
    engine
)

add_test(
    NAME hardware_system_test
    COMMAND hardware_system_test
)

add_executable(memory_architecture_test
    memory_architecture_test.cpp
)

target_link_libraries(memory_architecture_test
PRIVATE
    engine
)

add_test(
    NAME memory_architecture_test
    COMMAND memory_architecture_test
)

add_executable(parallelism_policy_test
    parallelism_policy_test.cpp
)

target_link_libraries(parallelism_policy_test
PRIVATE
    engine
)

add_test(
    NAME parallelism_policy_test
    COMMAND parallelism_policy_test
)

add_executable(os_process_test
    os_process_test.cpp
)

target_link_libraries(os_process_test
PRIVATE
    engine
)

add_test(
    NAME os_process_test
    COMMAND os_process_test
)

add_executable(synchronization_primitives_test
    synchronization_primitives_test.cpp
)

target_link_libraries(synchronization_primitives_test
PRIVATE
    engine
    Threads::Threads
)

add_test(
    NAME synchronization_primitives_test
    COMMAND synchronization_primitives_test
)

add_executable(lock_ordering_test
    lock_ordering_test.cpp
)

target_link_libraries(lock_ordering_test
PRIVATE
    engine
)

add_test(
    NAME lock_ordering_test
    COMMAND lock_ordering_test
)

add_executable(lock_free_primitives_test
    lock_free_primitives_test.cpp
)

target_link_libraries(lock_free_primitives_test
PRIVATE
    engine
    Threads::Threads
)

add_test(
    NAME lock_free_primitives_test
    COMMAND lock_free_primitives_test
)

add_executable(simd_policy_test
    simd_policy_test.cpp
)

target_link_libraries(simd_policy_test
PRIVATE
    engine
)

add_test(
    NAME simd_policy_test
    COMMAND simd_policy_test
)

add_executable(vec3_math_test
    vec3_math_test.cpp
)

target_link_libraries(vec3_math_test
PRIVATE
    engine
)

add_test(
    NAME vec3_math_test
    COMMAND vec3_math_test
)

add_executable(mat4_math_test
    mat4_math_test.cpp
)

target_link_libraries(mat4_math_test
PRIVATE
    engine
)

add_test(
    NAME mat4_math_test
    COMMAND mat4_math_test
)

add_executable(geometry_math_test
    geometry_math_test.cpp
)

target_link_libraries(geometry_math_test
PRIVATE
    engine
)

add_test(
    NAME geometry_math_test
    COMMAND geometry_math_test
)

add_executable(random_policy_test
    random_policy_test.cpp
)

target_link_libraries(random_policy_test
PRIVATE
    engine
)

add_test(
    NAME random_policy_test
    COMMAND random_policy_test
)

add_executable(stack_allocator_test
    stack_allocator_test.cpp
)

target_link_libraries(stack_allocator_test
PRIVATE
    engine
)

add_test(
    NAME stack_allocator_test
    COMMAND stack_allocator_test
)

add_executable(closed_hash_table_test
    closed_hash_table_test.cpp
)

target_link_libraries(closed_hash_table_test
PRIVATE
    engine
)

add_test(
    NAME closed_hash_table_test
    COMMAND closed_hash_table_test
)

add_executable(string_id_test
    string_id_test.cpp
)

target_link_libraries(string_id_test
PRIVATE
    engine
)

add_test(
    NAME string_id_test
    COMMAND string_id_test
)

add_executable(config_store_test
    config_store_test.cpp
)

target_link_libraries(config_store_test
PRIVATE
    engine
)

add_test(
    NAME config_store_test
    COMMAND config_store_test
)

add_executable(filesystem_path_test
    filesystem_path_test.cpp
)

target_link_libraries(filesystem_path_test
PRIVATE
    engine
)

add_test(
    NAME filesystem_path_test
    COMMAND filesystem_path_test
)

add_executable(filesystem_io_test
    filesystem_io_test.cpp
)

target_link_libraries(filesystem_io_test
PRIVATE
    engine
)

add_test(
    NAME filesystem_io_test
    COMMAND filesystem_io_test
)

add_executable(resource_manager_test
    resource_manager_test.cpp
)

target_link_libraries(resource_manager_test
PRIVATE
    engine
)

add_test(
    NAME resource_manager_test
    COMMAND resource_manager_test
)

add_executable(irender_backend_test
    irender_backend_test.cpp
)

target_link_libraries(irender_backend_test
PRIVATE
    engine
)

add_test(
    NAME irender_backend_test
    COMMAND irender_backend_test
)

add_executable(simulation_island_test
    simulation_island_test.cpp
)

target_link_libraries(simulation_island_test
PRIVATE
    engine
)

add_test(
    NAME simulation_island_test
    COMMAND simulation_island_test
)

add_executable(garden_simulation_test
    garden_simulation_test.cpp
)

target_link_libraries(garden_simulation_test
PRIVATE
    engine
    garden_sim
)

add_test(
    NAME garden_simulation_test
    COMMAND garden_simulation_test
)

add_executable(time_policy_test
    time_policy_test.cpp
)

target_link_libraries(time_policy_test
PRIVATE
    engine
)

add_test(
    NAME time_policy_test
    COMMAND time_policy_test
)

add_executable(job_system_baseline_test
    job_system_baseline_test.cpp
)

target_link_libraries(job_system_baseline_test
PRIVATE
    engine
)

add_test(
    NAME job_system_baseline_test
    COMMAND job_system_baseline_test
)

add_executable(hid_input_policy_test
    hid_input_policy_test.cpp
)

target_link_libraries(hid_input_policy_test
PRIVATE
    engine
)

add_test(
    NAME hid_input_policy_test
    COMMAND hid_input_policy_test
)

add_executable(hid_gesture_policy_test
    hid_gesture_policy_test.cpp
)

target_link_libraries(hid_gesture_policy_test
PRIVATE
    engine
)

add_test(
    NAME hid_gesture_policy_test
    COMMAND hid_gesture_policy_test
)

add_executable(hid_mapping_policy_test
    hid_mapping_policy_test.cpp
)

target_link_libraries(hid_mapping_policy_test
PRIVATE
    engine
)

add_test(
    NAME hid_mapping_policy_test
    COMMAND hid_mapping_policy_test
)

add_executable(platform_keyboard_bridge_test
    platform_keyboard_bridge_test.cpp
)

target_link_libraries(platform_keyboard_bridge_test
PRIVATE
    engine
)

add_test(
    NAME platform_keyboard_bridge_test
    COMMAND platform_keyboard_bridge_test
)

add_executable(hid_action_policy_test
    hid_action_policy_test.cpp
)

target_link_libraries(hid_action_policy_test
PRIVATE
    engine
)

add_test(
    NAME hid_action_policy_test
    COMMAND hid_action_policy_test
)

add_executable(hid_action_context_test
    hid_action_context_test.cpp
)

target_link_libraries(hid_action_context_test
PRIVATE
    engine
)

add_test(
    NAME hid_action_context_test
    COMMAND hid_action_context_test
)

add_executable(platform_gamepad_bridge_test
    platform_gamepad_bridge_test.cpp
)

target_link_libraries(platform_gamepad_bridge_test
PRIVATE
    engine
    glfw
)

add_test(
    NAME platform_gamepad_bridge_test
    COMMAND platform_gamepad_bridge_test
)

add_executable(log_policy_test
    log_policy_test.cpp
)

target_link_libraries(log_policy_test
PRIVATE
    engine
)

add_test(
    NAME log_policy_test
    COMMAND log_policy_test
)

add_executable(animation_pose_test
    animation_pose_test.cpp
)

target_link_libraries(animation_pose_test
PRIVATE
    engine
)

add_test(
    NAME animation_pose_test
    COMMAND animation_pose_test
)

add_executable(animation_clip_skinning_test
    animation_clip_skinning_test.cpp
)

target_link_libraries(animation_clip_skinning_test
PRIVATE
    engine
)

add_test(
    NAME animation_clip_skinning_test
    COMMAND animation_clip_skinning_test
)

add_executable(animation_blend_test
    animation_blend_test.cpp
)

target_link_libraries(animation_blend_test
PRIVATE
    engine
)

add_test(
    NAME animation_blend_test
    COMMAND animation_blend_test
)

add_executable(animation_compression_test
    animation_compression_test.cpp
)

target_link_libraries(animation_compression_test
PRIVATE
    engine
)

add_test(
    NAME animation_compression_test
    COMMAND animation_compression_test
)

add_executable(animation_action_state_machine_test
    animation_action_state_machine_test.cpp
)

target_link_libraries(animation_action_state_machine_test
PRIVATE
    engine
)

add_test(
    NAME animation_action_state_machine_test
    COMMAND animation_action_state_machine_test
)

add_executable(animation_constraints_test
    animation_constraints_test.cpp
)

target_link_libraries(animation_constraints_test
PRIVATE
    engine
)

add_test(
    NAME animation_constraints_test
    COMMAND animation_constraints_test
)

add_executable(physics_middleware_test
    physics_middleware_test.cpp
)

target_link_libraries(physics_middleware_test
PRIVATE
    engine
)

add_test(
    NAME physics_middleware_test
    COMMAND physics_middleware_test
)

add_executable(collision_detection_test
    collision_detection_test.cpp
)

target_link_libraries(collision_detection_test
PRIVATE
    engine
)

add_test(
    NAME collision_detection_test
    COMMAND collision_detection_test
)

add_executable(rigid_body_dynamics_test
    rigid_body_dynamics_test.cpp
)

target_link_libraries(rigid_body_dynamics_test
PRIVATE
    engine
)

add_test(
    NAME rigid_body_dynamics_test
    COMMAND rigid_body_dynamics_test
)

add_executable(physics_world_integration_test
    physics_world_integration_test.cpp
)

target_link_libraries(physics_world_integration_test
PRIVATE
    engine
)

add_test(
    NAME physics_world_integration_test
    COMMAND physics_world_integration_test
)

add_executable(physics_of_sound_test
    physics_of_sound_test.cpp
)

target_link_libraries(physics_of_sound_test
PRIVATE
    engine
)

add_test(
    NAME physics_of_sound_test
    COMMAND physics_of_sound_test
)

add_executable(mathematics_of_sound_test
    mathematics_of_sound_test.cpp
)

target_link_libraries(mathematics_of_sound_test
PRIVATE
    engine
)

add_test(
    NAME mathematics_of_sound_test
    COMMAND mathematics_of_sound_test
)

add_executable(sound_technology_test
    sound_technology_test.cpp
)

target_link_libraries(sound_technology_test
PRIVATE
    engine
)

add_test(
    NAME sound_technology_test
    COMMAND sound_technology_test
)

add_executable(audio_spatial_3d_test
    audio_spatial_3d_test.cpp
)

target_link_libraries(audio_spatial_3d_test
PRIVATE
    engine
)

add_test(
    NAME audio_spatial_3d_test
    COMMAND audio_spatial_3d_test
)

add_executable(audio_engine_architecture_test
    audio_engine_architecture_test.cpp
)

target_link_libraries(audio_engine_architecture_test
PRIVATE
    engine
)

add_test(
    NAME audio_engine_architecture_test
    COMMAND audio_engine_architecture_test
)

add_executable(game_specific_audio_test
    game_specific_audio_test.cpp
)

target_link_libraries(game_specific_audio_test
PRIVATE
    engine
)

add_test(
    NAME game_specific_audio_test
    COMMAND game_specific_audio_test
)

add_executable(gameplay_foundation_test
    gameplay_foundation_test.cpp
)

target_link_libraries(gameplay_foundation_test
PRIVATE
    engine
)

add_test(
    NAME gameplay_foundation_test
    COMMAND gameplay_foundation_test
)

add_executable(runtime_object_model_test
    runtime_object_model_test.cpp
)

target_link_libraries(runtime_object_model_test
PRIVATE
    engine
)

add_test(
    NAME runtime_object_model_test
    COMMAND runtime_object_model_test
)

add_executable(world_data_formats_test
    world_data_formats_test.cpp
)

target_link_libraries(world_data_formats_test
PRIVATE
    engine
)

add_test(
    NAME world_data_formats_test
    COMMAND world_data_formats_test
)

add_executable(realtime_object_updates_test
    realtime_object_updates_test.cpp
)

target_link_libraries(realtime_object_updates_test
PRIVATE
    engine
)

add_test(
    NAME realtime_object_updates_test
    COMMAND realtime_object_updates_test
)

add_executable(events_and_messaging_test
    events_and_messaging_test.cpp
)

target_link_libraries(events_and_messaging_test
PRIVATE
    engine
)

add_test(
    NAME events_and_messaging_test
    COMMAND events_and_messaging_test
)

add_executable(online_multiplayer_foundation_test
    online_multiplayer_foundation_test.cpp
)

target_link_libraries(online_multiplayer_foundation_test
PRIVATE
    engine
)

add_test(
    NAME online_multiplayer_foundation_test
    COMMAND online_multiplayer_foundation_test
)

add_executable(multiplayer_wire_format_test
    multiplayer_wire_format_test.cpp
)

target_link_libraries(multiplayer_wire_format_test
PRIVATE
    engine
)

add_test(
    NAME multiplayer_wire_format_test
    COMMAND multiplayer_wire_format_test
)

add_executable(loopback_game_transport_test
    loopback_game_transport_test.cpp
)

target_link_libraries(loopback_game_transport_test
PRIVATE
    engine
)

add_test(
    NAME loopback_game_transport_test
    COMMAND loopback_game_transport_test
)

add_executable(udp_game_transport_integration_test
    udp_game_transport_integration_test.cpp
)

target_link_libraries(udp_game_transport_integration_test
PRIVATE
    engine
)

add_test(
    NAME udp_game_transport_integration_test
    COMMAND udp_game_transport_integration_test
)

add_executable(multiplayer_session_envelope_test
    multiplayer_session_envelope_test.cpp
)

target_link_libraries(multiplayer_session_envelope_test
PRIVATE
    engine
)

add_test(
    NAME multiplayer_session_envelope_test
    COMMAND multiplayer_session_envelope_test
)

add_executable(reliable_channel_test
    reliable_channel_test.cpp
)

target_link_libraries(reliable_channel_test
PRIVATE
    engine
)

add_test(
    NAME reliable_channel_test
    COMMAND reliable_channel_test
)

add_executable(reliable_envelope_test
    reliable_envelope_test.cpp
)

target_link_libraries(reliable_envelope_test
PRIVATE
    engine
)

add_test(
    NAME reliable_envelope_test
    COMMAND reliable_envelope_test
)

add_executable(authoritative_session_test
    authoritative_session_test.cpp
)

target_link_libraries(authoritative_session_test
PRIVATE
    engine
)

add_test(
    NAME authoritative_session_test
    COMMAND authoritative_session_test
)

add_executable(snapshot_interpolator_test
    snapshot_interpolator_test.cpp
)

target_link_libraries(snapshot_interpolator_test
PRIVATE
    engine
)

add_test(
    NAME snapshot_interpolator_test
    COMMAND snapshot_interpolator_test
)

add_executable(scripting_game_flow_test
    scripting_game_flow_test.cpp
)

target_link_libraries(scripting_game_flow_test
PRIVATE
    engine
)

add_test(
    NAME scripting_game_flow_test
    COMMAND scripting_game_flow_test
)

if(TARGET Jolt)
    add_executable(jolt_physics_smoke_test
        jolt_physics_smoke_test.cpp
    )
    target_link_libraries(jolt_physics_smoke_test
    PRIVATE
        Jolt
    )
    add_test(
        NAME jolt_physics_smoke_test
        COMMAND jolt_physics_smoke_test
    )
endif()
