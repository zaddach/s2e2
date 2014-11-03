# Set up variables for QEMU build and source dir
# QEMU build dir needs to be specified with QEMU_BUILD

IF ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )
MESSAGE ( "ERROR: Cannot find config-host.mak in your Qemu build directory. Cannot configure Qemu." )
SET ( QEMU_FOUND false )
RETURN ()
ELSE ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )
SET ( QEMU_FOUND true )
ENDIF ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )

# Find QEMU source path
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "SRC_PATH=" )
STRING ( REGEX REPLACE "^SRC_PATH=(.*)$" "\\1" QEMU_SOURCE ${QEMU_LINES} )

# Find architecture for which TCG is built
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "QEMU_INCLUDES=" )
STRING ( REGEX REPLACE "^.*/tcg/([0-9a-z_-]+) -I.*$" "\\1" QEMU_TCG_ARCH ${QEMU_LINES} ) 

# Find targets for which are built
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "TARGET_DIRS=" )
STRING ( REGEX MATCHALL "([0-9a-zA-Z_-]+)-softmmu" QEMU_TARGET_DIRS ${QEMU_LINES})
FOREACH ( QEMU_TARGET_DIR ${QEMU_TARGET_DIRS} )
	STRING ( REGEX REPLACE "-softmmu" "" QEMU_TARGET_ARCH ${QEMU_TARGET_DIR} )
    IF ( ${QEMU_TARGET_ARCH} STREQUAL "i386" OR ${QEMU_TARGET_ARCH} STREQUAL "arm" )
        LIST ( APPEND QEMU_TARGET_INCLUDE_DIRECTORIES "-I${QEMU_BUILD}/${QEMU_TARGET_DIR} -I${QEMU_SOURCE}/target-${QEMU_TARGET_ARCH}")
        LIST ( APPEND QEMU_TARGET_ARCHITECTURES ${QEMU_TARGET_ARCH} )
    ENDIF ( ${QEMU_TARGET_ARCH} STREQUAL "i386" OR ${QEMU_TARGET_ARCH} STREQUAL "arm" )
ENDFOREACH ( QEMU_TARGET_DIR ${QEMU_TARGET_DIRS} )

#TODO: Improve above to get right source and build directories for each target
SET ( QEMU_TARGET_ARCHITECTURES "i386" "arm" )

SET ( QEMU_INCLUDE_DIRECTORIES ${QEMU_SOURCE}/tcg ${QEMU_SOURCE}/include ${QEMU_BUILD} ${QEMU_SOURCE}/tcg/${QEMU_TCG_ARCH} )

MARK_AS_ADVANCED ( 
        QEMU_LINES
        QEMU_TCG_ARCH
        QEMU_TARGET_DIRS
        QEMU_TARGET_DIR
        QEMU_TARGET_ARCH )