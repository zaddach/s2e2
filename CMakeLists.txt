PROJECT ( llvm-translator )
CMAKE_MINIMUM_REQUIRED ( VERSION 2.8.8 FATAL_ERROR )


# Configurable variables
########################

SET ( LIBRARY_NAME "llvm_translator" )
SET ( DEFAULT_BUILD_TYPE "Debug" )

# Find requisites
#################

SET ( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake" )

FIND_PACKAGE ( LLVM 3.4 REQUIRED )
FIND_PACKAGE ( GLIB REQUIRED )
FIND_PACKAGE ( Curses REQUIRED )
FIND_PACKAGE ( SigC++ REQUIRED )
FIND_PACKAGE ( Lua REQUIRED )

INCLUDE ( ExternalProject REQUIRED )
INCLUDE ( cmake/LLVMTools.cmake )

# Add subprojects
#################

ADD_SUBDIRECTORY ( klee )
INCLUDE_DIRECTORIES ( klee/include ${PROJECT_BINARY_DIR}/klee/include )

SET ( QEMU_TARGET_LIST
    microblaze-softmmu
    ppc64-softmmu
    sh4-softmmu
    aarch64-softmmu
    mips-softmmu
    ppcemb-softmmu
    unicore32-softmmu
    i386-softmmu
    mips64el-softmmu
    moxie-softmmu
    alpha-softmmu
    lm32-softmmu
    ppc-softmmu
    mips64-softmmu
    or32-softmmu
    m68k-softmmu
    s390x-softmmu
    sparc64-softmmu
    x86_64-softmmu
    arm-softmmu
    mipsel-softmmu
    xtensaeb-softmmu
    microblazeel-softmmu
    sh4eb-softmmu
    xtensa-softmmu
    cris-softmmu
    sparc-softmmu )


INCLUDE ( cmake/Qemu.cmake )

# Build instructions
####################

INCLUDE_DIRECTORIES ( include ${QEMU_INCLUDE_DIRECTORIES} ${LLVM_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ${SIGC++_INCLUDE_DIRS} ${LUA_INCLUDE_DIR} )
ADD_DEFINITIONS ( -DNEED_CPU_H -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DCONFIG_LLVM )
SET ( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup" )
SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

SET ( LLVM_TRANSLATOR_SRC 
	src/tcgplugin/tcg-llvm.cpp
	src/tcgplugin/tcg-plugin-main.cpp)

SET ( S2E_SRC
	src/s2e/ConfigFile.cpp
	src/s2e/ExprInterface.cpp
#	src/s2e/MMUFunctionHandlers.cpp
	src/s2e/Plugin.cpp
	src/s2e/S2E.cpp
	src/s2e/S2EDeviceState.cpp
	src/s2e/S2EExecutionState.cpp
	src/s2e/S2EExecutor.cpp
	src/s2e/S2EStatsTracker.cpp
	src/s2e/SelectRemovalPass.cpp
	src/s2e/Slab.cpp
	src/s2e/Synchronization.cpp )

MACRO  ( GENERATE_HELPER_FILE helper_file target_arch target_base_arch )
    IF ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/helper_lib/target-${target_base_arch}/op_helper_llvm.c )
        SET ( HELPER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/helper_lib/target-${target_base_arch}/op_helper_llvm.c )
        ADD_CUSTOM_TARGET ( generate-helper-source-${QEMU_TARGET_DIRECTORY} 
            DEPENDS ${HELPER_SOURCE})
    ELSE ()
        SET ( OP_HELPER_DIR ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${OP_HELPER_LIB_NAME}.dir )
        SET ( HELPER_LIST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/helper_lib/helper_list.c )
        SET ( HELPER_LIST_PROCESSED ${OP_HELPER_DIR}/helper_list.i )
        SET ( HELPER_SOURCE ${OP_HELPER_DIR}/op_helper_llvm.c )

        #Build list of include directories
        GET_PROPERTY ( INC_DIRS DIRECTORY PROPERTY INCLUDE_DIRECTORIES )
        SET ( INCLUDE_DIRECTORIES )
        FOREACH (DIRECTORY ${INC_DIRS})
            LIST ( APPEND INCLUDE_DIRECTORIES -I${DIRECTORY} )
        ENDFOREACH ()
        LIST ( APPEND INCLUDE_DIRECTORIES -I${QEMU_SOURCE}/target-${target_base_arch} )
        LIST ( APPEND INCLUDE_DIRECTORIES -I${QEMU_BUILD}/${target_arch} )

        #Build list of definitions
        GET_PROPERTY ( COMP_DEFS DIRECTORY PROPERTY COMPILE_DEFINITIONS )
        SET ( COMPILE_DEFINITIONS )
        FOREACH ( DEFINITION ${COMP_DEFS} )
            LIST ( APPEND COMPILE_DEFINITIONS -D${DEFINITION} )
        ENDFOREACH ()

        ADD_CUSTOM_COMMAND (
            OUTPUT ${HELPER_LIST_PROCESSED}
            COMMAND ${CMAKE_C_COMPILER} ${INCLUDE_DIRECTORIES} ${COMPILE_DEFINITIONS} -E -o "${HELPER_LIST_PROCESSED}" "${HELPER_LIST_SOURCE}" 
            DEPENDS src/helper_lib/helper_list.c ${QEMU_SOURCE}/target-${target_base_arch}/helper.h
            VERBATIM )
        ADD_CUSTOM_TARGET ( preprocess-helpers-${QEMU_TARGET_DIRECTORY}
            DEPENDS ${HELPER_LIST_PROCESSED})
        ADD_DEPENDENCIES ( preprocess-helpers-${QEMU_TARGET_DIRECTORY} build-qemu )

        ADD_CUSTOM_COMMAND ( 
            OUTPUT "${HELPER_SOURCE}"
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_helper.sh "${HELPER_LIST_PROCESSED}" "${HELPER_SOURCE}"
            WORKING_DIRECTORY "${QEMU_SOURCE}/target-${target_base_arch}"
            DEPENDS ${HELPER_LIST_PROCESSED} )
        ADD_CUSTOM_TARGET ( generate-helper-source-${QEMU_TARGET_DIRECTORY} 
            DEPENDS "${HELPER_SOURCE}")
        ADD_DEPENDENCIES ( generate-helper-source-${QEMU_TARGET_DIRECTORY} preprocess-helpers-${QEMU_TARGET_DIRECTORY} )
    ENDIF ()

    SET_PROPERTY ( SOURCE ${HELPER_SOURCE} APPEND PROPERTY INCLUDE_DIRECTORIES 
        "${QEMU_SOURCE}/target-${QEMU_TARGET_BASE_ARCH}" "${QEMU_BUILD}/${QEMU_TARGET_DIRECTORY}" )
    ADD_BITCODE (${helper_file}   ${HELPER_SOURCE})
    ADD_DEPENDENCIES ( ${helper_file} generate-helper-source-${QEMU_TARGET_DIRECTORY} )
    
ENDMACRO ()

    
    
FOREACH ( QEMU_TARGET ${QEMU_TARGETS} )
    STRING ( REGEX MATCH "^[^-]+" QEMU_TARGET_BASE_ARCH ${QEMU_TARGET} )

    STRING ( REPLACE "," ";" QEMU_TARGET_LIST ${QEMU_TARGET} )

    LIST ( GET QEMU_TARGET_LIST 0 QEMU_TARGET_BASE_ARCH )
    LIST ( GET QEMU_TARGET_LIST 1 QEMU_TARGET_DIRECTORY )
    LIST ( GET QEMU_TARGET_LIST 2 QEMU_TARGET_ARCH )
    
    # Build the TCG plugin
	  SET ( CURRENT_LIB_NAME ${LIBRARY_NAME}-${QEMU_TARGET_DIRECTORY} )

    ADD_LIBRARY ( ${CURRENT_LIB_NAME} SHARED ${LLVM_TRANSLATOR_SRC}  ${S2E_SRC} )
	TARGET_LINK_LIBRARIES ( ${CURRENT_LIB_NAME} ${LLVM_LIBRARIES} ${CURSES_LIBRARY} )
    ADD_DEPENDENCIES ( ${CURRENT_LIB_NAME} build-qemu )
	SET_PROPERTY ( TARGET ${CURRENT_LIB_NAME} APPEND PROPERTY INCLUDE_DIRECTORIES 
   	    "${QEMU_SOURCE}/target-${QEMU_TARGET_BASE_ARCH}" "${QEMU_BUILD}/${QEMU_TARGET_DIRECTORY}" )
	SET_PROPERTY ( TARGET ${CURRENT_LIB_NAME} APPEND PROPERTY COMPILE_FLAGS 
  	    "-DTARGET_EMULATION_MODE=\\\"${QEMU_TARGET_DIRECTORY}\\\" -DTARGET_NAME=\\\"${QEMU_TARGET_ARCH}\\\"" ) 
    
    # Build the LLVM helper library
    SET ( OP_HELPER_LIB_NAME op_helper-${QEMU_TARGET_DIRECTORY} )
    SET ( OP_HELPER_DIR ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${OP_HELPER_LIB_NAME}.dir )
    SET ( OP_HELPER_SOURCE${OP_HELPER_DIR}/op_helper.c )
    
    #Generate C file including all helper implementation files and compile it to bitcode
    GENERATE_HELPER_FILE (${OP_HELPER_LIB_NAME} "${OP_HELPER_DIRECTORY}" "${QEMU_TARGET_BASE_ARCH}" )
        
ENDFOREACH ()
