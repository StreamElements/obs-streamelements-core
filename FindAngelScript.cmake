include(FindPackageHandleStandardArgs)

SET(ANGELSCRIPT_ROOT_DIR "" CACHE PATH "Path to AngelScript SDK")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	# 64 bits
	message("Looking for angelscript.lib in ${ANGELSCRIPT_ROOT_DIR}/sdk/angelscript/lib/64bit")
	find_library(ANGELSCRIPT_LIBRARY
		NAMES angelscript.lib "AngelScript"
		PATHS ${ANGELSCRIPT_ROOT_DIR}/sdk/angelscript/lib/64bit)
	message("ANGELSCRIPT_LIBRARY: ${ANGELSCRIPT_LIBRARY}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	# 32 bits
	message("Looking for angelscript.lib in ${ANGELSCRIPT_ROOT_DIR}/sdk/angelscript/lib/32bit")
	find_library(ANGELSCRIPT_LIBRARY
		NAMES angelscript.lib "AngelScript"
		PATHS ${ANGELSCRIPT_ROOT_DIR}/sdk/angelscript/lib/32bit)
	message("ANGELSCRIPT_LIBRARY: ${ANGELSCRIPT_LIBRARY}")
else()
	message(WARNING "Could not detect CPU architecture")
	set(ANGELSCRIPT_FOUND FALSE)
	return()
endif()

if(NOT ANGELSCRIPT_LIBRARY)
	message(WARNING "Could not find the AngelScript library" )
	set(ANGELSCRIPT_FOUND FALSE)
	return()
else()
	message("ANGELSCRIPT_LIBRARY: ${ANGELSCRIPT_LIBRARY}")
endif()

set(ANGELSCRIPT_INCLUDE_DIR "${ANGELSCRIPT_ROOT_DIR}/sdk/angelscript/include")
set(ANGELSCRIPT_ADDON_INCLUDE_DIR "${ANGELSCRIPT_ROOT_DIR}/sdk/add_on")

set(ANGELSCRIPT_LIBRARIES
		optimized ${ANGELSCRIPT_LIBRARY})

message("ANGELSCRIPT_LIBRARIES: ${ANGELSCRIPT_LIBRARIES}")

find_package_handle_standard_args(AngelScript DEFAULT_MSG ANGELSCRIPT_LIBRARY ANGELSCRIPT_INCLUDE_DIR)
mark_as_advanced(ANGELSCRIPT_LIBRARY ANGELSCRIPT_LIBRARIES ANGELSCRIPT_INCLUDE_DIR)
