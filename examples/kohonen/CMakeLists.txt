# All #includes in test programs have to be #include <morph/header.h>
include_directories(BEFORE ${PROJECT_SOURCE_DIR})

add_executable(kohonen kohonen.cpp)
target_compile_definitions(kohonen PUBLIC FLT=float)
if(APPLE)
  target_link_libraries(kohonen OpenMP::OpenMP_CXX ${ARMADILLO_LIBRARY} ${ARMADILLO_LIBRARIES} ${LAPACK_LIBRARIES} ${OpenCV_LIBS} OpenGL::GL glfw Freetype::Freetype jsoncpp_lib ${HDF5_C_LIBRARIES})
else()
  target_link_libraries(kohonen ${ARMADILLO_LIBRARY} ${ARMADILLO_LIBRARIES} ${LAPACK_LIBRARIES} ${OpenCV_LIBS} OpenGL::GL glfw Freetype::Freetype jsoncpp_lib ${HDF5_C_LIBRARIES})
endif()
