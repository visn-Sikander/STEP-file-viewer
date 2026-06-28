cmake_minimum_required(VERSION 3.16)
project(AiCadPlatform)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OCCT_ROOT $ENV{CASROOT})
message(STATUS "OCCT_ROOT=${OCCT_ROOT}")

add_executable(AiCadPlatform main.cpp)

target_include_directories(AiCadPlatform PRIVATE
    "${OCCT_ROOT}/inc"
    "C:/opencascade-7.9.3-vc14-64/3rdparty-vc14-64/rapidjson-1.10/include"
)

target_link_directories(AiCadPlatform PRIVATE
    "${OCCT_ROOT}/win64/vc14/lib"
)

target_link_libraries(AiCadPlatform PRIVATE
    # Geometry
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKGeomAlgo
    TKBRep
    TKTopAlgo
    TKPrim
    TKBO

    # STEP
    TKDESTEP
    TKXSBase          # <-- XSControl_Reader base
    TKXSDRAWSTEP      # <-- STEPControl_Reader concrete class

    # Viewer
    TKService
    TKV3d
    TKOpenGl
    TKMesh
    TKShHealing

    # Windows OpenGL
    opengl32
    glu32
)

# Add Unicode definition
target_compile_definitions(AiCadPlatform PRIVATE UNICODE _UNICODE)