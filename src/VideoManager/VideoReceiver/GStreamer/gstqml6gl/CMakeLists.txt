find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(GStreamer REQUIRED COMPONENTS Allocators Audio Codecparsers Controller Mpegts Net Pbutils Riff Rtp Rtsp Sdp Tag OPTIONAL_COMPONENTS Va Photography X11 EGL Wayland)

file(GLOB gstqml6gl_SRCS
    ${QGC_GST_QT6_PLUGIN_PATH}/*.cc
    ${QGC_GST_QT6_PLUGIN_PATH}/*.h
)
qt_add_library(gstqml6gl STATIC ${gstqml6gl_SRCS})

target_include_directories(gstqml6gl PUBLIC ${QGC_GST_QT6_PLUGIN_PATH})

target_link_libraries(gstqml6gl
    PRIVATE
        Qt6::GuiPrivate
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        GStreamer::GStreamer
        GStreamer::Allocators
        GStreamer::Audio
        GStreamer::Codecparsers
        GStreamer::Controller
        GStreamer::Mpegts
        GStreamer::Net
        GStreamer::Pbutils
        GStreamer::Riff
        GStreamer::Rtp
        GStreamer::Rtsp
        GStreamer::Sdp
        GStreamer::Tag
)

# Photography not found on ubuntu 20.04?
if(GStreamer_Photography_FOUND)
    target_link_libraries(gstqml6gl PUBLIC GStreamer::Photography)
endif()

if(GStreamer_Va_FOUND)
    target_link_libraries(gstqml6gl PUBLIC GStreamer::Va)
endif()

if(GStreamer_VERSION VERSION_GREATER_EQUAL 1.22)
    FetchContent_Declare(gstreamer_good_plugins
        URL https://gitlab.freedesktop.org/gstreamer/gstreamer/-/archive/${GST_PLUGINS_VERSION}/gstreamer-${GST_PLUGINS_VERSION}.zip?path=subprojects/gst-plugins-good/ext/qt6
        DOWNLOAD_EXTRACT_TIMESTAMP true
    )
    FetchContent_MakeAvailable(gstreamer_good_plugins)
    set(QGC_GST_QT6_PLUGIN_PATH "${gstreamer_good_plugins_SOURCE_DIR}/subprojects/gst-plugins-good/ext/qt6" CACHE PATH "Location of downloaded GST Qt6 plugin sources")
else()
    set(QGC_GST_QT6_PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/qt6" CACHE PATH "Location of custom local GST plugin")
endif()
cmake_print_variables(QGC_GST_QT6_PLUGIN_PATH)

################################################################################

file(READ ${QGC_GST_QT6_PLUGIN_PATH}/qt6glitem.h FILE_CONTENTS)
string(FIND "${FILE_CONTENTS}" "#include <QtQuick/QQuickWindow>" GST_FIX_INCLUDES)
if(GST_FIX_INCLUDES EQUAL -1)
    string(REPLACE "#include <QtGui/QOpenGLFunctions>" "#include <QtGui/QOpenGLFunctions>\n#include <QtQuick/QQuickWindow>" FILE_CONTENTS "${FILE_CONTENTS}")
    file(WRITE ${QGC_GST_QT6_PLUGIN_PATH}/qt6glitem.h "${FILE_CONTENTS}")
endif()

################################################################################

find_package(GLESv2)
if(GLESv2_FOUND)
    target_link_libraries(gstqml6gl PUBLIC GLESv2::GLESv2)
endif()

if(GStreamer_X11_FOUND)
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(gstqml6gl PUBLIC X11::X11)
    endif()
    find_package(XCB COMPONENTS XCB GLX)
    if(XCB_FOUND)
        target_link_libraries(gstqml6gl PUBLIC XCB::XCB XCB::GLX)
    endif()
    find_package(X11_XCB)
    if(X11_XCB_FOUND)
        target_link_libraries(gstqml6gl PUBLIC X11::XCB)
    endif()
    target_link_libraries(gstqml6gl PUBLIC GStreamer::X11)
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_X11)
endif()

if(GStreamer_EGL_FOUND)
    find_package(EGL)
    if(EGL_FOUND)
        target_link_libraries(gstqml6gl PUBLIC EGL::EGL)
    endif()
    target_link_libraries(gstqml6gl PUBLIC GStreamer::EGL)
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_EGLFS)
endif()

if(GStreamer_Wayland_FOUND)
    find_package(Wayland COMPONENTS Client Cursor Egl)
    if(Wayland_FOUND)
        target_link_libraries(gstqml6gl PUBLIC Wayland::Client Wayland::Cursor Wayland::Egl)
    endif()
    find_package(WaylandProtocols)
    if(WaylandProtocols_FOUND)
        # WaylandProtocols_DATADIR
    endif()
    find_package(WaylandScanner)
    if(WaylandScanner_FOUND)
        # target_link_libraries(gstqml6gl PUBLIC Wayland::Scanner)
    endif()
    find_package(Qt6 COMPONENTS WaylandClient)
    if(Qt6WaylandClient_FOUND)
        target_link_libraries(gstqml6gl PUBLIC GStreamer::Wayland Qt6::WaylandClient)
        target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_WAYLAND)
    endif()
endif()

if(ANDROID)
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_ANDROID)
elseif(WIN32)
    find_package(OpenGL)
    if(OpenGL_FOUND)
        target_link_libraries(gstqml6gl PUBLIC OpenGL::GL)
    endif()
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_WIN32)
elseif(MACOS)
    # appleframeworks
    # Foundation, QuartzCore, CoreFoundation
    # Cocoa
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_MAC)
elseif(IOS)
    # appleframeworks
    # Foundation, CoreFoundation, CoreGraphics, QuartzCore, UIkit
    target_compile_definitions(gstqml6gl PRIVATE HAVE_QT_IOS)
endif()

target_compile_definitions(gstqml6gl
    PRIVATE
        HAVE_QT_QPA_HEADER
        QT_QPA_HEADER=<QtGui/qpa/qplatformnativeinterface.h>
)

################################################################################

if(EXISTS "${QGC_GST_QT6_PLUGIN_PATH}/resources.qrc")
    find_package(Qt6 REQUIRED COMPONENTS ShaderTools)

    set(SHADERS
        ${QGC_GST_QT6_PLUGIN_PATH}/vertex.vert
        ${QGC_GST_QT6_PLUGIN_PATH}/YUV_TRIPLANAR.frag
        ${QGC_GST_QT6_PLUGIN_PATH}/RGBA.frag
    )
    set(OUTPUTS
        vertex.vert.qsb
        YUV_TRIPLANAR.frag.qsb
        RGBA.frag.qsb
    )
    if(EXISTS "${QGC_GST_QT6_PLUGIN_PATH}/YUV_BIPLANAR.frag")
        LIST(APPEND SHADERS ${QGC_GST_QT6_PLUGIN_PATH}/YUV_BIPLANAR.frag)
        LIST(APPEND OUTPUTS YUV_BIPLANAR.frag.qsb)
    endif()
    if(EXISTS "${QGC_GST_QT6_PLUGIN_PATH}/RGBA_gles.frag")
        LIST(APPEND SHADERS ${QGC_GST_QT6_PLUGIN_PATH}/RGBA.frag@glsl,100es,${QGC_GST_QT6_PLUGIN_PATH}/RGBA_gles.frag)
        LIST(APPEND OUTPUTS RGBA_gles.frag.qsb)
        # file(COPY_FILE RGBA_gles.frag RGBA_gles.frag.qsb)
        # LIST(APPEND OUTPUTS RGBA_gles.frag.qsb.external)
    endif()
    qt6_add_shaders(gstqml6gl "gstqml6gl_shaders"
        PREFIX "/org/freedesktop/gstreamer/qml6"
        GLSL "100 es,120,330"
        OUTPUT_TARGETS gstqml6gl_shaders
        FILES ${SHADERS}
        OUTPUTS ${OUTPUTS}
        BATCHABLE
    )
endif()

################################################################################

if(UNIX)
    target_compile_options(gstqml6gl
        PRIVATE
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            -Wno-unused-private-field
    )
endif()

if(LINUX)
    install(DIRECTORY ${GSTREAMER_PREFIX}/lib/x86_64-linux-gnu/gstreamer1.0 DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY ${GSTREAMER_PREFIX}/lib/x86_64-linux-gnu/gio DESTINATION ${CMAKE_INSTALL_LIBDIR})
elseif(WIN32)
    cmake_path(CONVERT "${GSTREAMER_PREFIX}/libexec/gstreamer-1.0/*.exe" TO_CMAKE_PATH_LIST GST_HELPER_BINS_PATH)
    file(GLOB GST_HELPER_BINS ${GST_HELPER_BINS_PATH})
    install(FILES ${GST_HELPER_BINS} DESTINATION ${CMAKE_INSTALL_BINDIR})

    cmake_path(CONVERT "${GSTREAMER_PREFIX}/lib/gio" TO_CMAKE_PATH_LIST GST_GIO_PATH)
    install(DIRECTORY ${GST_GIO_PATH} DESTINATION ${CMAKE_INSTALL_BINDIR})

    # cmake_path(CONVERT "${GSTREAMER_PREFIX}/share/gstreamer/gst-env" TO_CMAKE_PATH_LIST GST_SHARE_ENV_PATH)
    # TODO: update GSTREAMER_ROOT & install
    # install(CODE GST_SHARE_ENV_PATH)
endif()

if(WIN32)
    cmake_path(CONVERT "${GSTREAMER_PREFIX}/bin/*.dll" TO_CMAKE_PATH_LIST GST_WIN_BINS_PATH)
    file(GLOB GST_WIN_BINS ${GST_WIN_BINS_PATH})
    install(FILES ${GST_WIN_BINS} DESTINATION ${CMAKE_INSTALL_BINDIR})

    cmake_path(CONVERT "${GSTREAMER_PREFIX}/lib/*.dll" TO_CMAKE_PATH_LIST GST_WIN_LIBS_PATH)
    file(GLOB GST_WIN_LIBS ${GST_WIN_LIBS_PATH})
    install(FILES ${GST_WIN_LIBS} DESTINATION ${CMAKE_INSTALL_BINDIR})

    cmake_path(CONVERT "${GSTREAMER_PREFIX}/lib/gstreamer-1.0/*.dll" TO_CMAKE_PATH_LIST GST_WIN_PLUGINS_PATH)
    file(GLOB GST_WIN_PLUGINS ${GST_WIN_PLUGINS_PATH})
    install(FILES ${GST_WIN_PLUGINS} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

################################################################################
