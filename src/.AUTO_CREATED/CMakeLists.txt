#######################################
# Create a target for hxfilo
#######################################

# Glob all sources
file(GLOB C_CXX_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx)

xpand_add_library(hxfilo
    SHARED
        ${C_CXX_SOURCES}
)

target_link_libraries(hxfilo
    PRIVATE
        AvizoApps::Qt5Core
        AvizoApps::Qt5Gui
        AvizoApps::Qt5Widgets
        AvizoApps::InventorBase
        AvizoApps::Inventor
        hxplot
        hxtime
        hxsurface
        hxcolor
        hxfield
        hxcore
        hxqt
        amiramesh
        mclib
)

#######################################
# Add files to copy to the same relative path within the build folder
#######################################
add_custom_command(
    TARGET hxfilo
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
    ARGS
        -E
        copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/share/resources/hxfilo.rc"
        "${CMAKE_SOURCE_DIR}/../share/resources/hxfilo.rc"
)
