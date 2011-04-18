###########################################################################
#
#  Library:   MAF
#
###########################################################################

#-----------------------------------------------------------------------------
# Settings shared between the build tree and install tree.


#-----------------------------------------------------------------------------
# Settings specific to the build tree.

# The "use" file.
SET(scxml_USE_FILE ${scxml_BINARY_DIR}/Usescxml.cmake)

# Determine the include directories needed.
SET(scxml_INCLUDE_DIRS_CONFIG
  ${scxml_SOURCE_DIR}
)

# Library directory.
SET(scxml_LIBRARY_DIRS_CONFIG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# Runtime library directory.
SET(scxml_RUNTIME_LIBRARY_DIRS_CONFIG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Build configuration information.
SET(scxml_CONFIGURATION_TYPES_CONFIG ${CMAKE_CONFIGURATION_TYPES})
SET(scxml_BUILD_TYPE_CONFIG ${CMAKE_BUILD_TYPE})

#-----------------------------------------------------------------------------
# Configure scxmlConfig.cmake for the build tree.
CONFIGURE_FILE(${scxml_SOURCE_DIR}/scxmlConfig.cmake.in
               ${scxml_BINARY_DIR}/scxmlConfig.cmake @ONLY IMMEDIATE)

#-----------------------------------------------------------------------------
# Settings specific to the install tree.

# TODO

#-----------------------------------------------------------------------------
# Configure scxmlConfig.cmake for the install tree.

# TODO
