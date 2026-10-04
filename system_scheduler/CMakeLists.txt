idf_component_register(
    SRCS "src/system_scheduler.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos esp_system
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    SYSTEM_SCHEDULER_VERSION_MAJOR=1
    SYSTEM_SCHEDULER_VERSION_MINOR=0
    SYSTEM_SCHEDULER_VERSION_PATCH=0
)

message(STATUS "=================================================")
message(STATUS "System Scheduler Component v1.0.0")
message(STATUS "=================================================")
message(STATUS "Features:")
message(STATUS "  - Event-driven task scheduling")
message(STATUS "  - Watchdog integration")
message(STATUS "  - Auto lock/unlock injection")
message(STATUS "  - Maximum tasks: ${CONFIG_SCHEDULER_MAX_TASKS}")
message(STATUS "=================================================")