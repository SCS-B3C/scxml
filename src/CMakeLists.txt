
SET(KIT_SRCS
  qscxml.cpp
  qscxml.h
    )
  
SET(KIT_MOC_SRCS
  qscxml.h
  )
  
QT4_WRAP_CPP(KIT_MOC_CPP ${KIT_MOC_SRCS})

SET(libname ${PROJECT_NAME})
ADD_LIBRARY(${libname} STATIC
  ${KIT_SRCS}
  ${KIT_MOC_CPP}
  )
  
SET(${PROJECT_NAME}_LINK_LIBRARIES ${QT_LIBRARIES})
TARGET_LINK_LIBRARIES(
  ${libname}
  ${${PROJECT_NAME}_LINK_LIBRARIES}
  )
  
#-----------------------------------------------------------------------------
#
# To fix compilation problem: relocation R_X86_64_32 against `a local symbol' can not be
# used when making a shared object; recompile with -fPIC
# See http://www.cmake.org/pipermail/cmake/2007-May/014350.html
#
IF( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

