cmake_minimum_required (VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)

# set project's name
set (PROJECT easypaint)

message(STATUS "System: " ${CMAKE_HOST_SYSTEM_NAME} " " ${CMAKE_HOST_SYSTEM_VERSION})
message(STATUS "Processor: " ${CMAKE_HOST_SYSTEM_PROCESSOR})

# add version define
set(EASYPAINT_VERSION 0.1.2)

add_compile_definitions(EASYPAINT_VERSION="${EASYPAINT_VERSION}")

#------- headers --------
set (HEADERS
    sources/mainwindow.h
    sources/easypaintenums.h
    sources/imagearea.h
    sources/datasingleton.h
    sources/autorun_utils.h
    sources/effects/effectruncallback.h
    sources/effects/abstracteffect.h
    sources/effects/negativeeffect.h
    sources/effects/grayeffect.h
    sources/effects/binarizationeffect.h
    sources/effects/customeffect.h
    sources/effects/effectwithsettings.h
    sources/effects/gammaeffect.h
    sources/effects/gaussianblureffect.h
    sources/effects/scripteffect.h
    sources/effects/scripteffectwithsettings.h
    sources/effects/sharpeneffect.h
    sources/makeguard.h
    sources/qtsingleapplication/qtlocalpeer.h
    sources/qtsingleapplication/qtsingleapplication.h
    sources/qtsingleapplication/qtsinglecoreapplication.h
    sources/set_dark_theme.h
    sources/ScriptInfo.h
    sources/ScriptModel.h
    sources/undocommand.h
    sources/widgets/toolbar.h
    sources/widgets/colorchooser.h
    sources/widgets/palettebar.h
    sources/widgets/palettebutton.h
    sources/widgets/shortcutedit.h
    sources/widgets/abstracteffectsettings.h
    sources/widgets/customfiltersettings.h
    sources/widgets/sharpenfiltersettings.h
    sources/widgets/gaussianblurfiltersettings.h
    sources/widgets/scripteffectsettings.h
    sources/dialogs/resizedialog.h
    sources/dialogs/settingsdialog.h
    sources/dialogs/textdialog.h
    sources/dialogs/effectsettingsdialog.h
    sources/dialogs/SpinnerOverlay.h
    sources/instruments/abstractinstrument.h
    sources/instruments/abstractselection.h
    sources/instruments/selectioninstrument.h
    sources/instruments/pencilinstrument.h
    sources/instruments/lineinstrument.h
    sources/instruments/eraserinstrument.h
    sources/instruments/rectangleinstrument.h
    sources/instruments/ellipseinstrument.h
    sources/instruments/fillinstrument.h
    sources/instruments/sprayinstrument.h
    sources/instruments/magnifierinstrument.h
    sources/instruments/colorpickerinstrument.h
    sources/instruments/curvelineinstrument.h
    sources/instruments/textinstrument.h)

#------- sources --------
set (SOURCES
    sources/main.cpp
    sources/mainwindow.cpp
    sources/imagearea.cpp
    sources/datasingleton.cpp
    sources/autorun_utils.cpp
    sources/effects/abstracteffect.cpp
    sources/effects/customeffect.cpp
    sources/effects/negativeeffect.cpp
    sources/effects/grayeffect.cpp
    sources/effects/binarizationeffect.cpp
    sources/effects/effectwithsettings.cpp
    sources/effects/gammaeffect.cpp
    sources/effects/scripteffect.cpp
    sources/effects/scripteffectwithsettings.cpp
    sources/qtsingleapplication/qtlocalpeer.cpp
    sources/qtsingleapplication/qtsingleapplication.cpp
    sources/qtsingleapplication/qtsinglecoreapplication.cpp
    sources/set_dark_theme.cpp
    sources/ScriptModel.cpp
    sources/undocommand.cpp
    sources/widgets/toolbar.cpp
    sources/widgets/colorchooser.cpp
    sources/widgets/palettebar.cpp
    sources/widgets/palettebutton.cpp
    sources/widgets/shortcutedit.cpp
    sources/widgets/customfiltersettings.cpp
    sources/widgets/sharpenfiltersettings.cpp
    sources/widgets/gaussianblurfiltersettings.cpp
    sources/widgets/scripteffectsettings.cpp
    sources/dialogs/resizedialog.cpp
    sources/dialogs/settingsdialog.cpp
    sources/dialogs/textdialog.cpp
    sources/dialogs/effectsettingsdialog.cpp
    sources/instruments/abstractinstrument.cpp
    sources/instruments/abstractselection.cpp
    sources/instruments/selectioninstrument.cpp
    sources/instruments/pencilinstrument.cpp
    sources/instruments/lineinstrument.cpp
    sources/instruments/eraserinstrument.cpp
    sources/instruments/rectangleinstrument.cpp
    sources/instruments/ellipseinstrument.cpp
    sources/instruments/fillinstrument.cpp
    sources/instruments/sprayinstrument.cpp
    sources/instruments/magnifierinstrument.cpp
    sources/instruments/colorpickerinstrument.cpp
    sources/instruments/curvelineinstrument.cpp
    sources/instruments/textinstrument.cpp)

#------- resources -------
set (RESOURCE_PATH
    sources)

set (RESOURCES
    ${RESOURCE_PATH}/resources.qrc)

#------- languages -------
if(NOT NO_TRANSLATIONS)
    set (TRANSLATIONS
        sources/translations/easypaint_ru_RU.ts
        sources/translations/easypaint_cs_CZ.ts
        sources/translations/easypaint_zh_CN.ts
        sources/translations/easypaint_fr_FR.ts)
endif(NOT NO_TRANSLATIONS)

project (${PROJECT})

include_directories (./)

find_package(QT NAMES Qt5 COMPONENTS Widgets Concurrent Network PrintSupport LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Concurrent Network PrintSupport LinguistTools REQUIRED)
#include (${QT_USE_FILE})
qt5_add_resources (QRC_SOURCES ${RESOURCES})
qt5_wrap_cpp (MOC_SOURCES ${HEADERS})
qt5_add_translation( TRANSLATIONS_QM ${TRANSLATIONS} )

add_definitions (-Wall)

source_group ("Header Files" FILES ${HEADERS})
source_group ("Source Files" FILES ${SOURCES})
source_group ("Generated Files" FILES ${MOC_SOURCES})
source_group ("Resource Files" FILES ${QRC_SOURCES})

find_package (Python3 COMPONENTS Interpreter Development REQUIRED)

message ("    *** Python: (${Python3_INCLUDE_DIRS})")

find_package(pybind11 CONFIG REQUIRED)

#-------  build executable from sources ----------
add_executable (${PROJECT} ${HEADERS} ${SOURCES} ${MOC_SOURCES} ${QRC_SOURCES} ${TRANSLATIONS_QM})

target_link_libraries (${PROJECT} Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::PrintSupport ${Python3_LIBRARIES} pybind11::pybind11)

#-------  installing ----------
if(UNIX AND NOT APPLE)
    # installing
    INSTALL(TARGETS easypaint RUNTIME DESTINATION bin)

    # install desktop files
    INSTALL(FILES "sources/media/easypaint.desktop" DESTINATION share/applications)

    # install pixmap
    INSTALL(FILES "sources/media/logo/easypaint_64.png" DESTINATION share/pixmaps)

    INSTALL(FILES ${TRANSLATIONS_QM} DESTINATION share/easypaint/translations)
endif(UNIX AND NOT APPLE)

if(MSVC)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bin")

	set(BINARY_INSTALL_DIR .)

	install(TARGETS ${PROJECT_NAME} DESTINATION ${BINARY_INSTALL_DIR})


	set(BINARY_FILE "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe")

	message( STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}" )

	get_filename_component(CMAKE_TOOLCHAIN_DIRECTORY ${CMAKE_TOOLCHAIN_FILE} DIRECTORY)

	#install(CODE "execute_process(COMMAND \"powershell\" \"-noprofile\" \"-executionpolicy\" \"Bypass\" \"-file\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/msbuild/applocal.ps1\" \"-targetBinary\" \"${BINARY_FILE}\" \"-installedDir\" \"$ENV{GSTREAMER_ROOT_X86_64}/bin\" \"-OutVariable\" \"out\")")


	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	    set( CMAKE_INSTALL_PLATFORM "x64-windows" )
	else ()
	    set( CMAKE_INSTALL_PLATFORM "x86-windows" )
	endif ()

	install(CODE "execute_process(COMMAND \"powershell\" \"-noprofile\" \"-executionpolicy\" \"Bypass\" \"-file\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/msbuild/applocal.ps1\" \"-targetBinary\" \"${BINARY_FILE}\" \"-installedDir\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/../../installed/${CMAKE_INSTALL_PLATFORM}/bin\" \"-OutVariable\" \"out\")")

	# Install CRT
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
	#if(DEVELOPER_FEATURES)
	#	set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)
	#endif(DEVELOPER_FEATURES)
	set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
	set(CMAKE_INSTALL_OPENMP_LIBRARIES TRUE)
	include (InstallRequiredSystemLibraries)

# Retrieve the absolute path to qmake and then use that path to find
# the windeployqt binary
	get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
	get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
	find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

	install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"  \"${BINARY_FILE}\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")


	#if (QT_QMAKE_EXECUTABLE)
	#	get_filename_component(QT_BIN_DIRECTORY ${QT_QMAKE_EXECUTABLE} DIRECTORY)
	#	install(CODE "execute_process(COMMAND \"${QT_BIN_DIRECTORY}/windeployqt\"  \"${BINARY_FILE}\" WORKING_DIRECTORY \"${QT_BIN_DIRECTORY}\")")
	#else()
	#	install(CODE "execute_process(COMMAND \"windeployqt\"  \"${BINARY_FILE}\" )")
	#endif()

    INSTALL(FILES ${TRANSLATIONS_QM} DESTINATION translations)

endif()
