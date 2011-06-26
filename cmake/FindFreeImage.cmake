#
# Try to find the FreeImage library and include path.
# Once done this will define
#
# FREEIMAGE_FOUND
# FREEIMAGE_INCLUDE_PATH
# FREEIMAGE_LIBRARY
# 

IF (WIN32)
	FIND_PATH( FREEIMAGE_INCLUDE_PATH FreeImage.h
		${FREEIMAGE_ROOT_DIR}/include
		${FREEIMAGE_ROOT_DIR}
		DOC "The directory where FreeImage.h resides")
	FIND_LIBRARY( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS
		${FREEIMAGE_ROOT_DIR}/lib
		${FREEIMAGE_ROOT_DIR}
		DOC "The FreeImage library")
ELSE (WIN32)
	FIND_PATH(FREEIMAGE_INCLUDE_PATH # OSX run, if not succesful, next FIND_PATH takes effect
		freeimage.h
		PATHS
		${OSX_DEPENDENCY_ROOT}/include
		NO_DEFAULT_PATH)
	FIND_PATH( FREEIMAGE_INCLUDE_PATH FreeImage.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where FreeImage.h resides")
	FIND_LIBRARY(FREEIMAGE_LIBRARY # OSX run, if not succesful, next FIND_PATH takes effect
		libfreeimage.a
		PATHS
		${OSX_DEPENDENCY_ROOT}/lib
		NO_DEFAULT_PATH)
	FIND_LIBRARY( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The FreeImage library")
ENDIF (WIN32)

SET(FREEIMAGE_LIBRARIES ${FREEIMAGE_LIBRARY})

IF (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)
	SET(FREEIMAGE_FOUND TRUE)
	IF(NOT APPLE)
########################### PNG   LIBRARIES SETUP ###########################
		FIND_PACKAGE(PNG)
		IF(PNG_FOUND)
			TRY_COMPILE(FREEIMAGE_PROVIDES_PNG ${CMAKE_BINARY_DIR}
				${CMAKE_SOURCE_DIR}/cmake/FindFreeImage.cxx
				CMAKE_FLAGS
				"-DINCLUDE_DIRECTORIES:STRING=${PNG_INCLUDES}"
				"-DLINK_LIBRARIES:STRING=${FREEIMAGE_LIBRARY}"
				COMPILE_DEFINITIONS -D__TEST_PNG__)
			IF (NOT FREEIMAGE_PROVIDES_PNG)
				SET(FREEIMAGE_LIBRARIES ${FREEIMAGE_LIBRARIES} ${PNG_LIBRARIES})
			ENDIF(NOT FREEIMAGE_PROVIDES_PNG)
		ELSE(PNG_FOUND)
			MESSAGE(STATUS "Warning : could not find PNG - building without png support")
		ENDIF(PNG_FOUND)
######################### OPENEXR LIBRARIES SETUP ###########################
		FIND_PATH(OPENEXR_INCLUDE_DIRS
			ImfXdr.h
			PATHS
			/usr/local/include/OpenEXR
			/usr/include/OpenEXR
			/sw/include/OpenEXR
			/opt/local/include/OpenEXR
			/opt/csw/include/OpenEXR
			/opt/include/OpenEXR
		)
		IF (OPENEXR_INCLUDE_DIRS)
			TRY_COMPILE(FREEIMAGE_PROVIDES_OPENEXR ${CMAKE_BINARY_DIR}
				${CMAKE_SOURCE_DIR}/cmake/FindFreeImage.cxx
				CMAKE_FLAGS
				"-DINCLUDE_DIRECTORIES:STRING=${OPENEXR_INCLUDE_DIRS}"
				"-DLINK_LIBRARIES:STRING=${FREEIMAGE_LIBRARY}"
				COMPILE_DEFINITIONS -D__TEST_OPENEXR__)
			IF (NOT FREEIMAGE_PROVIDES_OPENEXR)
				SET(FREEIMAGE_LIBRARIES ${FREEIMAGE_LIBRARIES} Half IlmImf Iex Imath)
			ENDIF(NOT FREEIMAGE_PROVIDES_OPENEXR)
		ENDIF (OPENEXR_INCLUDE_DIRS)
	ENDIF(NOT APPLE)
ELSE (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)
    SET(FREEIMAGE_FOUND FALSE)
ENDIF (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)

MARK_AS_ADVANCED(FREEIMAGE_LIBRARY FREEIMAGE_LIBRARIES FREEIMAGE_INCLUDE_PATH)
