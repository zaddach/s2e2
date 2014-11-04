function(CheckEmitLlvmFlag)
  set ( CLANG_TEST_SOURCE "${CMAKE_BINARY_DIR}/emitllvm.test.c" )
  set ( CLANGXX_TEST_SOURCE "${CMAKE_BINARY_DIR}/emitllvm.test.cpp" ) 
  set ( CLANG_TEST_BIN "${CLANG_TEST_SOURCE}.bc" )
  set ( CLANGXX_TEST_BIN "${CLANGXX_TEST_SOURCE}.bc" )
  
  file(WRITE ${CLANG_TEST_SOURCE} "int main(int argc, char* argv[]){return 0;}\n\n")
  file(WRITE ${CLANGXX_TEST_SOURCE} "int main(int argc, char* argv[]){return 0;}\n\n")
 
  message(STATUS "Checking for C LLVM compiler...")
  execute_process(COMMAND "${LLVM_BC_C_COMPILER}" "-emit-llvm" "-c" ${CLANG_TEST_SOURCE} "-o" ${CLANG_TEST_BIN}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  OUTPUT_QUIET ERROR_QUIET)
  execute_process(COMMAND "${LLVM_BC_ANALYZER}" ${CLANG_TEST_BIN}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  RESULT_VARIABLE AOUT_IS_NOT_BC
                  OUTPUT_QUIET ERROR_QUIET)
  if(AOUT_IS_NOT_BC)
    message(FATAL_ERROR "${LLVM_BC_C_COMPILER} is not valid LLVM compiler")
  endif()
  message(STATUS "Checking for C LLVM compiler... works.")
  file ( REMOVE ${CLANG_TEST_SOURCE} ${CLANG_TEST_BIN} )

  message(STATUS "Checking for CXX LLVM compiler...")
  execute_process(COMMAND "${LLVM_BC_CXX_COMPILER}" "-emit-llvm" "-c" ${CLANGXX_TEST_SOURCE} "-o" ${CLANGXX_TEST_BIN}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  OUTPUT_QUIET ERROR_QUIET)
  execute_process(COMMAND "${LLVM_BC_ANALYZER}" ${CLANGXX_TEST_BIN}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  RESULT_VARIABLE AOUT_IS_NOT_BC
                  OUTPUT_QUIET ERROR_QUIET)
  if(AOUT_IS_NOT_BC)
    message(FATAL_ERROR "${LLVM_BC_CXX_COMPILER} is not valid LLVM compiler")
  endif()
  message(STATUS "Checking for CXX LLVM compiler... works.")
  file ( REMOVE ${CLANGXX_TEST_SOURCE} ${CLANGXX_TEST_BIN} )
endfunction(CheckEmitLlvmFlag)

## ADD_BITCODE ##
macro(add_bitcode target)
    
## build all the bitcode files
add_custom_target(${target} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target}.bc)
set_property(TARGET ${target} PROPERTY LOCATION ${CMAKE_CURRENT_BINARY_DIR}/${target}.bc)

set(bcfiles "")
foreach(srcfile ${ARGN})
    ## get the definitions, flags, and includes to use when compiling this file
    set(srcdefs "")
    get_directory_property(COMPILE_DEFINITIONS COMPILE_DEFINITIONS)
    foreach(DEFINITION ${COMPILE_DEFINITIONS})
        list(APPEND srcdefs -D${DEFINITION})
    endforeach()

    set(srcflags "")
    if(${srcfile} MATCHES "(.*).cpp")
        separate_arguments(srcflags UNIX_COMMAND ${CMAKE_CXX_FLAGS})
        set(src_bc_compiler ${LLVM_BC_CXX_COMPILER})
    else()
        separate_arguments(srcflags UNIX_COMMAND ${CMAKE_C_FLAGS})
        set(src_bc_compiler ${LLVM_BC_C_COMPILER} )
    endif()
#    if(NOT ${CMAKE_C_FLAGS} STREQUAL "")
#        string(REPLACE " " ";" srcflags ${CMAKE_C_FLAGS})
#    endif()

    set(srcincludes "")
#    get_directory_property(INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES)
    get_property ( INCLUDE_DIRECTORIES TARGET ${target} PROPERTY INCLUDE_DIRECTORIES )
    foreach(DIRECTORY ${INCLUDE_DIRECTORIES})
        list(APPEND srcincludes -I${DIRECTORY})
    endforeach()
    
    get_property ( INCLUDE_DIRECTORIES SOURCE ${srcfile} PROPERTY INCLUDE_DIRECTORIES )
    foreach(DIRECTORY ${INCLUDE_DIRECTORIES})
        list(APPEND srcincludes -I${DIRECTORY})
    endforeach()
    
#    list ( REMOVE_DUPLICATES INCLUDE_DIRECTORIES )
    
    get_filename_component(outfile ${srcfile} NAME)
    get_filename_component(infile ${srcfile} ABSOLUTE)
    
    set ( outfile CMakeFiles/${target}.dir/${outfile}.bc )

    ## the command to generate the bitcode for this file
    add_custom_command(OUTPUT ${outfile}
      COMMAND ${src_bc_compiler} -emit-llvm ${srcdefs} ${srcflags} ${srcincludes}
        -c ${infile} -o ${outfile}
      DEPENDS ${infile}
      COMMENT "Building LLVM bitcode ${outfile}.bc"
      VERBATIM
    )

    ## keep track of every bitcode file we need to create
    list(APPEND bcfiles ${outfile})
endforeach(srcfile)

## link all the bitcode files together to the target
add_custom_command(OUTPUT ${target}.bc
    COMMAND ${LLVM_BC_LINK} ${BC_LD_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${target}.bc ${bcfiles}
    DEPENDS ${bcfiles}
    COMMENT "Linking LLVM bitcode ${target}.bc"
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${target}.bc)

endmacro(add_bitcode)

## ADD_PLUGIN ##
macro(add_plugin target)

set(bctargets "")
foreach(bctarget ${ARGN})
    set(bcpath "")
    get_property(bcpath TARGET ${bctarget} PROPERTY LOCATION)
    if(${bcpath} STREQUAL "")
        message(FATAL_ERROR "Can't find property path for target '${bctarget}'")
    endif()
    list(APPEND bctargets ${bcpath})
endforeach(bctarget)

## link all the bitcode targets together to the target, then hoist the globals
add_custom_command(OUTPUT ${target}.bc
    COMMAND ${LLVM_BC_LINK} ${BC_LD_FLAGS} -o ${target}.bc ${bctargets}
    DEPENDS ${bctargets}
    COMMENT "Linking LLVM bitcode ${target}.bc"
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${target}.bc)

add_custom_command(OUTPUT ${target}.hoisted.bc
    COMMAND ${LLVM_BC_OPT} -load=${LLVMHoistGlobalsPATH} -hoist-globals ${target}.bc -o ${target}.hoisted.bc
    DEPENDS ${target}.bc LLVMHoistGlobals
    COMMENT "Hoisting globals from ${target}.bc to ${target}.hoisted.bc"
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${target}.hoisted.bc)

## now we need the actual .so to be built
add_library(${target} MODULE ${target}.hoisted.bc)
add_dependencies(${target} ${target}.hoisted.bc)

## trick cmake so it builds the bitcode into a shared library
set_property(TARGET ${target} PROPERTY LINKER_LANGUAGE C)
set_property(SOURCE ${target}.hoisted.bc PROPERTY EXTERNAL_OBJECT TRUE)

## make sure we have the bitcode we need before building the .so
foreach(bctarget ${ARGN})
    add_dependencies(${target} ${bctarget})
endforeach(bctarget)

endmacro(add_plugin)

#####

SET ( LLVM_EXECUTABLES_PATHS /opt/local/bin )

find_program(LLVM_BC_C_COMPILER NAMES clang PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_CXX_COMPILER NAMES clang++  PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_AR NAMES llvm-ar llvm-ar-mp-3.6 llvm-ar-mp-3.5 llvm-ar-mp-3.4 
    llvm-ar-mp-3.3 llvm-ar-mp-3.2 llvm-ar-mp-3.1 llvm-ar-mp-3.0
    llvm-ar-3.6 llvm-ar-3.5 llvm-ar-3.4 llvm-ar-3.3 llvm-ar-3.2 llvm-ar-3.1 llvm-ar-3.0
     PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_RANLIB NAMES llvm-ranlib llvm-ranlib-mp-3.6 llvm-ranlib-mp-3.5
    llvm-ranlib-mp-3.4 llvm-ranlib-mp-3.3 llvm-ranlib-mp-3.2 llvm-ranlib-mp-3.1
    llvm-ranlib-mp-3.0
    llvm-ranlib-3.6 llvm-ranlib-3.5 llvm-ranlib-3.4 llvm-ranlib-3.3 llvm-ranlib-3.2
    llvm-ranlib-3.1 llvm-ranlib-3.0
     PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_LINK NAMES llvm-link llvm-link-mp-3.6 llvm-link-mp-3.5 llvm-link-mp-3.4
    llvm-link-mp-3.3 llvm-link-mp-3.2 llvm-link-mp-3.1 llvm-link-mp-3.0
    llvm-link-3.6 llvm-link-3.5 llvm-link-3.4 llvm-link-3.3 llvm-link-3.2 llvm-link-3.1 
    llvm-link-3.0
     PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_ANALYZER NAMES llvm-bcanalyzer llvm-bcanalyzer-mp-3.6 llvm-bcanalyzer-mp-3.5
    llvm-bcanalyzer-mp-3.4 llvm-bcanalyzer-mp-3.3 llvm-bcanalyzer-mp-3.2 llvm-bcanalyzer-mp-3.1
    llvm-bcanalyzer-mp-3.0
    llvm-bcanalyzer-3.6 llvm-bcanalyzer-3.5 llvm-bcanalyzer-3.4 llvm-bcanalyzer-3.3
    llvm-bcanalyzer-3.2 llvm-bcanalyzer-3.1 llvm-bcanalyzer-3.0
    PATHS ${LLVM_EXECUTABLES_PATHS} )
find_program(LLVM_BC_OPT NAMES opt opt-mp-3.6 opt-mp-3.5 opt-mp-3.4 opt-mp-3.3 opt-mp-3.2
    opt-mp-3.1 opt-mp-3.0 
    opt-3.6 opt-3.5 opt-3.4 opt-3.3 opt-3.2 opt-3.1 opt-3.0
    PATHS ${LLVM_EXECUTABLES_PATHS} )

if (NOT (LLVM_BC_C_COMPILER AND LLVM_BC_CXX_COMPILER AND LLVM_BC_AR AND
        LLVM_BC_RANLIB AND LLVM_BC_LINK AND LLVM_BC_ANALYZER AND LLVM_BC_OPT))
  message(SEND_ERROR "Some of following tools have not been found:")
  if (NOT LLVM_BC_C_COMPILER)
     message(SEND_ERROR "LLVM_BC_C_COMPILER") 
  endif()
  if (NOT LLVM_BC_CXX_COMPILER) 
     message(SEND_ERROR "LLVM_BC_CXX_COMPILER")
  endif()
  if (NOT LLVM_BC_AR) 
     message(SEND_ERROR "LLVM_BC_AR") 
  endif()
  if (NOT LLVM_BC_RANLIB) 
     message(SEND_ERROR "LLVM_BC_RANLIB") 
  endif()
  if (NOT LLVM_BC_LINK) 
     message(SEND_ERROR "LLVM_BC_LINK") 
  endif()
  if (NOT LLVM_BC_ANALYZER) 
     message(SEND_ERROR "LLVM_BC_ANALYZER") 
  endif()
  if (NOT LLVM_BC_OPT) 
     message(SEND_ERROR "LLVM_BC_OPT") 
  endif()
endif()

CheckEmitLlvmFlag()

#####
