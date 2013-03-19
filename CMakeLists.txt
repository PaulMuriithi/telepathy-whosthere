cmake_minimum_required(VERSION 2.8.7)
project(telepathy-whosthere)

find_package(Qt5Core)
find_package(Qt5DBus)

include_directories(${Qt5Core_INCLUDE_DIRS})
include_directories(${Qt5DBus_INCLUDE_DIRS})
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5DBus_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PYTHON REQUIRED python)

if(NOT DAEMON_DIR)
  set(DAEMON_DIR lib/telepathy)
endif(NOT DAEMON_DIR)

set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON) 
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.45.0 COMPONENTS python REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x -g -O0")

find_path(TELEPATHY_QT5_INCLUDE_DIR TelepathyQt/Constants TelepathyQt/BaseConnectionManager PATHS /usr/include/telepathy-qt5)
find_library(TELEPATHY_QT5_LIBRARIES telepathy-qt5)
find_library(TELEPATHY_QT5_SERVICE_LIBRARIES telepathy-qt5-service)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${TELEPATHY_QT5_INCLUDE_DIR})
include_directories(${PYTHON_INCLUDE_DIRS})

add_executable(telepathy-whosthere connection.cpp  main.cpp protocol.cpp  pythoninterface.cpp)
#qt5_use_modules(telepathy-whosthere Core DBus)
target_link_libraries(telepathy-whosthere ${Qt5Core_LIBRARIES} ${Qt5DBus_LIBRARIES})
target_link_libraries(telepathy-whosthere ${PYTHON_LIBRARIES} ${Boost_LIBRARIES} ${TELEPATHY_QT5_LIBRARIES} ${TELEPATHY_QT5_SERVICE_LIBRARIES})
install(TARGETS telepathy-whosthere DESTINATION ${DAEMON_DIR})

subdirs(data)
