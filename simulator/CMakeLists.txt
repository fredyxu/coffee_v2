cmake_minimum_required(VERSION 3.16)
project(coffee_simulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use simulator-local LVGL config
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(SDL2 REQUIRED)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../managed_components/lvgl__lvgl"
                 "${CMAKE_CURRENT_BINARY_DIR}/lvgl_build")

add_executable(coffee_sim
    src/main.c
    src/app_sim.c
    src/sim_shim.c
    ../main/modules/ui/ui.c
    ../main/modules/ui/page/page_init/page_init.c
    ../main/modules/ui/page/page_home/page_home.c
    ../main/modules/ui/font/font_14.c
)

target_include_directories(coffee_sim
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${CMAKE_CURRENT_LIST_DIR}/../main"
        "${CMAKE_CURRENT_LIST_DIR}/../main/modules"
)

target_link_libraries(coffee_sim
    PRIVATE
        lvgl
        SDL2::SDL2
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(coffee_sim PRIVATE SDL2::SDL2main)
endif()
