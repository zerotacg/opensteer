cmake_minimum_required(VERSION 3.5)
project(opensteer)


find_package(OpenGL)
find_package(GLUT)

#set(CMAKE_CXX_STANDARD 11)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()


if (DEFINED OVERRIDE_EXECUTABLE_SUFFIX)
    set(CMAKE_EXECUTABLE_SUFFIX ${OVERRIDE_EXECUTABLE_SUFFIX})
endif ()
if (DEFINED OVERRIDE_SHARED_LIBRARY_SUFFIX)
    set(CMAKE_SHARED_LIBRARY_SUFFIX ${OVERRIDE_SHARED_LIBRARY_SUFFIX})
endif ()


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")

add_definitions(-DOPENSTEER -DUSEOpenGL)

include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
include_directories(include)

set(HEADER_FILES
        include/OpenSteer/AbstractVehicle.h
        include/OpenSteer/Annotation.h
        include/OpenSteer/Camera.h
        include/OpenSteer/Clock.h
        include/OpenSteer/Color.h
        include/OpenSteer/Draw.h
        include/OpenSteer/LocalSpace.h
        include/OpenSteer/lq.h
        include/OpenSteer/Obstacle.h
        include/OpenSteer/OldPathway.h
        include/OpenSteer/OpenSteerDemo.h
        include/OpenSteer/Path.h
        include/OpenSteer/Pathway.h
        include/OpenSteer/PlugIn.h
        include/OpenSteer/PolylineSegmentedPath.h
        include/OpenSteer/PolylineSegmentedPathwaySegmentRadii.h
        include/OpenSteer/PolylineSegmentedPathwaySingleRadius.h
        include/OpenSteer/Proximity.h
        include/OpenSteer/QueryPathAlikeBaseDataExtractionPolicies.h
        include/OpenSteer/QueryPathAlike.h
        include/OpenSteer/QueryPathAlikeMappings.h
        include/OpenSteer/QueryPathAlikeUtilities.h
        include/OpenSteer/SegmentedPathAlikeUtilities.h
        include/OpenSteer/SegmentedPath.h
        include/OpenSteer/SegmentedPathway.h
        include/OpenSteer/SharedPointer.h
        include/OpenSteer/SimpleVehicle.h
        include/OpenSteer/StandardTypes.h
        include/OpenSteer/SteerLibrary.h
        include/OpenSteer/UnusedParameter.h
        include/OpenSteer/Utilities.h
        include/OpenSteer/Vec3.h
        include/OpenSteer/Vec3Utilities.h
        )

if (WITH_OPENGL_ES)
    set(DRAW_SOURCE_FILES src/Draw_gles.cpp)
else()
    set(DRAW_SOURCE_FILES src/Draw.cpp)
endif()

set(SOURCE_FILES
        src/Camera.cpp
        src/Clock.cpp
        src/Color.cpp
        ${DRAW_SOURCE_FILES}
        src/lq.c
        src/main.cpp
        src/Obstacle.cpp
        src/OldPathway.cpp
        src/OpenSteerDemo.cpp
        src/Path.cpp
        src/Pathway.cpp
        src/PlugIn.cpp
        src/PolylineSegmentedPath.cpp
        src/PolylineSegmentedPathwaySegmentRadii.cpp
        src/PolylineSegmentedPathwaySingleRadius.cpp
        src/SegmentedPath.cpp
        src/SegmentedPathway.cpp
        src/SimpleVehicle.cpp
        src/TerrainRayTest.cpp
        src/Vec3.cpp
        src/Vec3Utilities.cpp

        plugins/Boids.cpp
        plugins/CaptureTheFlag.cpp
        plugins/LowSpeedTurn.cpp
        plugins/MapDrive.cpp
        plugins/MultiplePursuit.cpp
        plugins/OneTurning.cpp
        plugins/Pedestrian.cpp
        plugins/PedestriansWalkingAnEight.cpp
        plugins/Soccer.cpp
        )


add_executable(OpenSteerDemo ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(OpenSteerDemo ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
