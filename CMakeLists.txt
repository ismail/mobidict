cmake_minimum_required(VERSION 3.9.2)

project(mobidict)

include(CheckIPOSupported)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()

if (LINUX)
  option(ASAN "Enable AddressSanitizer (Linux only)" OFF)
endif()
option(AUTOTEST "Enable Automatic testing" OFF)

find_package(Qt5 COMPONENTS Concurrent Svg Widgets REQUIRED)
find_package(ZLIB REQUIRED)

set(LIBMOBI_SRCS libmobi/src/buffer.c libmobi/src/compression.c
                 libmobi/src/encryption.c libmobi/src/index.c libmobi/src/memory.c
                 libmobi/src/meta.c libmobi/src/parse_rawml.c libmobi/src/sha1.c
                 libmobi/src/read.c libmobi/src/structure.c libmobi/tools/common.c
                 libmobi/src/util.c)

set(SOURCES htmlbrowser.cpp main.cpp mainwindow.cpp settings.cpp
            mobidict.cpp resources.qrc ${LIBMOBI_SRCS})

qt5_wrap_ui(UI_HEADERS mainwindow.ui settings.ui)

include_directories(BEFORE "libmobi/src" ${ZLIB_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_ENCRYPTION")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_ENCRYPTION")

if (APPLE)
  set(OS_BUNDLE MACOSX_BUNDLE)
  set(RES_FILES "res/app.icns")
  set_source_files_properties("res/app.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif (WIN32)
  ENABLE_LANGUAGE(RC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
  set(CMAKE_EXE_LINKER_FLAGS "/NODEFAULTLIB:libcmt")
  set(OS_BUNDLE WIN32)
  set(RES_FILES "mobidict.rc")
endif()

if (UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEBUG_OUTPUT")
endif()

if (ASAN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if (AUTOTEST)
  find_package(Qt5 COMPONENTS Test REQUIRED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUTOTEST")
  set(TEST_LIB "Qt5::Test")
endif()

add_executable(mobidict ${OS_BUNDLE} ${SOURCES} ${UI_HEADERS} ${RES_FILES})
target_link_libraries(mobidict Qt5::Concurrent Qt5::Svg Qt5::Widgets ${TEST_LIB} ${ZLIB_LIBRARIES})

# Disabled until fix https://gitlab.kitware.com/cmake/cmake/commit/4e1ea02bb86f40d8ba0c247869a508b1da2c84b1
# is available
#check_ipo_supported(RESULT ipo_supported OUTPUT output)
#if(ipo_supported AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#  set_property(TARGET mobidict PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#  message(STATUS "Enabling Interprocedural optimization")
#else()
#    message(STATUS "Interprocedural optimization is not supported with ${CMAKE_CXX_COMPILER_ID}")
#endif()

if (LINUX)
    install(TARGETS mobidict RUNTIME DESTINATION bin)
    install(FILES res/mobidict.desktop DESTINATION share/applications)
    install(FILES res/mobidict.png DESTINATION share/pixmaps)
elseif (APPLE OR WIN32)
  include(DeployQt5)

  if (WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    set(BUNDLE_NAME "mobidict.exe")
  elseif (APPLE)
    set(BUNDLE_NAME "mobidict.app")
  endif()

  get_property(QT_GIF_PLUGIN TARGET Qt5::QGifPlugin PROPERTY LOCATION_RELEASE)
  get_property(QT_JPEG_PLUGIN TARGET Qt5::QJpegPlugin PROPERTY LOCATION_RELEASE)
  get_property(QT_SVG_PLUGIN TARGET Qt5::QSvgPlugin PROPERTY LOCATION_RELEASE)

  if (APPLE)
      get_property(QT_UI_PLUGIN TARGET Qt5::QMacStylePlugin PROPERTY LOCATION_RELEASE)
  elseif (WIN32)
      get_property(QT_UI_PLUGIN TARGET Qt5::QWindowsVistaStylePlugin PROPERTY LOCATION_RELEASE)
  endif()

  install(TARGETS mobidict
          RUNTIME DESTINATION .
          BUNDLE DESTINATION  . )

  install_qt5_executable(${BUNDLE_NAME} "${QT_GIF_PLUGIN};${QT_JPEG_PLUGIN};${QT_SVG_PLUGIN};${QT_UI_PLUGIN}" "" "" "plugins" TRUE)

  set(CPACK_PACKAGE_VENDOR "i10z")
  set(CPACK_PACKAGE_VERSION "1.99")
  set(CPACK_PACKAGE_VERSION_MAJOR "1")
  set(CPACK_PACKAGE_VERSION_MINOR "99")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")

  if (APPLE)
      set(CPACK_GENERATOR DragNDrop)
      set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/res/app.icns")
  elseif (WIN32)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_DISPLAY_NAME "Mobidict")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MobiDict.lnk' '$INSTDIR\\\\mobidict.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
      "Delete '$SMPROGRAMS\\\\$START_MENU\\\\MobiDict.lnk'")
    set(CPACK_NSIS_DEFINES "ManifestDPIAware true")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/res/app.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "mobidict.exe")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mobidict dictionary")
    set(CPACK_PACKAGE_FILE_NAME "mobidict-1.3-win64")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Mobidict")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Mobidict")
  endif()

  include(CPack)
endif()
