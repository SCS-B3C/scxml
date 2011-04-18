
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.2)

PROJECT(scxml)

OPTION(BUILD_SCC "Enable scc" OFF)
OPTION(scxml_BUILD_EXAMPLES "Build examples" OFF)


#-----------------------------------------------------------------------------
# Output directories.
#
SET(output_dir ${scxml_BINARY_DIR}/bin)
FOREACH(type LIBRARY RUNTIME ARCHIVE)
  IF(NOT DEFINED CMAKE_${type}_OUTPUT_DIRECTORY)
    SET(CMAKE_${type}_OUTPUT_DIRECTORY ${output_dir} CACHE INTERNAL "Single output directory for building all libraries.")
  ENDIF()
ENDFOREACH()

#-----------------------------------------------------------------------------
# Qt
#
FIND_PACKAGE(Qt4 4.6 REQUIRED)
SET(QT_USE_QTSCRIPT ON)
INCLUDE(${QT_USE_FILE})


#-----------------------------------------------------------------------------
# Subdirectories
#

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/src
  )
ADD_SUBDIRECTORY(src)



IF(BUILD_SCC)
#  ADD_SUBDIRECTORY(scc)
ENDIF()

IF(scxml_BUILD_EXAMPLES)
#  ADD_SUBDIRECTORY(examples)
ENDIF()

#-----------------------------------------------------------------------------
# Generate scxml file
#
CONFIGURE_FILE(Usescxml.cmake.in
               ${scxml_BINARY_DIR}/Usescxml.cmake COPYONLY)

INCLUDE(GeneratescxmlConfig.cmake)

