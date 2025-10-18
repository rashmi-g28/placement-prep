cmake_minimum_required(VERSION 3.10)
project(ai_feedback_dashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS bedrock-runtime sts)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE SQLite::SQLite3 CURL::libcurl ${AWSSDK_LINK_LIBRARIES})
