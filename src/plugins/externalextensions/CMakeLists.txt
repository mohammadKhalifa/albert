cmake_minimum_required(VERSION 2.8.12)

PROJECT(externalextensions)

FILE(GLOB_RECURSE SRC src/* metadata.json)

find_package(Qt5 5.2.0 REQUIRED COMPONENTS
    Widgets
)

qt5_wrap_ui(UI
    configwidget.ui
)

# Define the target
add_library(${PROJECT_NAME} SHARED ${SRC} ${UI})

# Set INCLUDE_DIRS and INTERFACE_INCLUDE_DIRS
target_include_directories(${PROJECT_NAME} PRIVATE src/)

# Link target to libraries
target_link_libraries(${PROJECT_NAME}
    ${Qt5Widgets_LIBRARIES}
    albertcore
    xdg
)

# Install target
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib/albert/plugins)
