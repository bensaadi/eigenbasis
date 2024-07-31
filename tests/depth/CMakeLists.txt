include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

file(GLOB tests_SRC "*.cpp")
file(GLOB fixtures_SRC "fixtures/*.cpp")

add_executable(
  depth_test
  ${fixtures_SRC}
  ${tests_SRC}
)

add_test(depth_test depth_test)
