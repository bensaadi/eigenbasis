include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

file(GLOB book_SRC "*.cpp" "../../src/utils/*.cpp")
file(GLOB fixtures_SRC "fixtures/*.cpp")

add_executable(
  book_test
  ${fixtures_SRC}
  ${book_SRC}
)

target_link_libraries(book_test ${CMAKE_THREAD_LIBS_INIT} book)

add_test(book_test book_test)
