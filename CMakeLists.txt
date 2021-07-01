cmake_minimum_required(VERSION 3.1)

project (DxfViewer LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

add_subdirectory(Lib)




option (FORCE_STATIC_RUNTIME "Force MSVC to use static runtime (use with static builds)" FALSE)




# Initialize Qt:

find_package (Qt5 5.12 COMPONENTS Widgets LinguistTools REQUIRED)

if (MSVC)
	# Make build use multiple threads under MSVC:
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# Make release builds use link-time code generation:
	set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
	set (CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   /GL")
	set (CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}    /LTCG")
	set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
	set (CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
	set (CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

	if (FORCE_STATIC_RUNTIME)
		message(STATUS "Forcing a static runtime")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

		# For some reason the Qt modules don't have their dependencies on other libraries set right, we need to explicitly add them:
		set (QT_LIB_PATH ${CMAKE_PREFIX_PATH}/lib)
		set (ADDITIONAL_LIBRARIES
			ws2_32.lib
			opengl32.lib
			imm32.lib
			winmm.lib
			iphlpapi.lib
			crypt32.lib
			netapi32.lib
			userenv.lib
			version.lib
			dwmapi.lib
			strmiids.lib
			${QT_LIB_PATH}/qtfreetype.lib
			${QT_LIB_PATH}/qtharfbuzz.lib
			${QT_LIB_PATH}/qtlibpng.lib
			${QT_LIB_PATH}/qtpcre2.lib
			${QT_LIB_PATH}/Qt5FontDatabaseSupport.lib
			${QT_LIB_PATH}/Qt5EventDispatcherSupport.lib
			${QT_LIB_PATH}/Qt5ThemeSupport.lib
			${QT_LIB_PATH}/Qt5AccessibilitySupport.lib
			${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows.lib
		)
		add_definitions(-DFORCE_STATIC_RUNTIME)
	endif ()
endif ()





# Qt5-specific setup (adapted from https://www.kdab.com/using-cmake-with-qt-5/ ):

# Tell CMake to run moc / uic / rcc when necessary:
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTORCC ON)

# moc files are generated in the binary dir, tell CMake to look for includes there:
set (CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories (
	${Qt5Widgets_INCLUDES}
)
add_definitions (
	${Qt5Widgets_DEFINITIONS}
)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")




# Add the preprocessor macros used for distinguishing between debug and release builds (CMake does this automatically for MSVC):
if (NOT MSVC)
	set (CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -D_DEBUG")
	set (CMAKE_C_FLAGS_DEBUG      "${CMAKE_C_FLAGS_DEBUG}      -D_DEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE}  -DNDEBUG")
	set (CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}    -DNDEBUG")
endif ()





# Set up extra compiler warnings:
if (MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
else ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunknown-pragmas")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wundef")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdisabled-optimization")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winit-self")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpointer-arith")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch")
endif ()





add_definitions(-DQT_DEPRECATED_WARNINGS)

# Enable message log context information even in Release builds:
add_definitions(-DQT_MESSAGELOGCONTEXT)





set (SOURCES
	Src/Main.cpp
	Src/Ui/CadView.cpp
	Src/Ui/DxfWindow.cpp
)





set (HEADERS
	Src/Ui/CadView.hpp
	Src/Ui/DxfWindow.hpp
)





set (UI
	Src/Ui/DxfWindow.ui
)

set (RESOURCES
	Res/DxfViewer.qrc
)

if (WIN32)
	list (APPEND RESOURCES Res/win.rc)
endif ()

set (TSFILES
	# TODO: Add Translations/*.ts files here when we get translations
)





# Process translations (update, compile):
set(ts_lst_file "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_lst_file")
set(lst_file_srcs)
foreach(lst_file_src ${SOURCES} ${HEADERS} ${UI})
	set(lst_file_srcs "${lst_file_src}\n${lst_file_srcs}")
endforeach()
file (WRITE ${ts_lst_file} "${lst_file_srcs}")

add_custom_target(tsfiles
	COMMENT "Updating TS files"
	COMMAND "${Qt5_LUPDATE_EXECUTABLE}" ${_lupdate_options} @${ts_lst_file} -ts ${TSFILES}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_byproduct_file"
	DEPENDS ${SOURCES} ${HEADERS} ${TSFILES} VERBATIM
	SOURCES ${TSFILES}
)
set (QMFILES "")
make_directory("${CMAKE_CURRENT_BINARY_DIR}/translations")
foreach(_current_FILE ${TSFILES})
	get_filename_component(_abs_FILE ${_current_FILE} ABSOLUTE)
	get_filename_component(qm ${_abs_FILE} NAME)
	# everything before the last dot has to be considered the file name (including other dots)
	string(REGEX REPLACE "\\.[^.]*$" "" FILE_NAME ${qm})
	set(qm "${CMAKE_CURRENT_BINARY_DIR}/translations/${FILE_NAME}.qm")

	add_custom_command(OUTPUT ${qm}
		COMMENT "Compiling QM files"
		COMMAND ${Qt5_LRELEASE_EXECUTABLE}
		ARGS ${_abs_FILE} -qm ${qm}
		DEPENDS ${_abs_FILE} VERBATIM
	)
	list(APPEND QMFILES ${qm})
endforeach()





add_executable (${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${UI} ${RESOURCES} ${QMFILES})

target_link_libraries (${PROJECT_NAME}
	Qt5::Widgets
	DxfLib
	${ADDITIONAL_LIBRARIES}
)

add_dependencies(${PROJECT_NAME} tsfiles)
