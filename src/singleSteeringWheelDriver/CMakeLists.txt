cmake_minimum_required(VERSION 2.8.3)
project(singleSteeringWheelDriver)

## Compile as C++11, supported in ROS Kinetic and newer
add_compile_options(-std=c++11)

find_package(Eigen3 REQUIRED)
if(NOT EIGEN3_INCLUDE_DIRS)
  set(EIGEN3_INCLUDE_DIRS "${EIGEN3_INCLUDE_DIR};/usr/include/eigen3/Eigen/Eigen")
endif()

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  tf
  std_msgs
  robot_msgs
)

catkin_package(
  CATKIN_DEPENDS roscpp rospy std_msgs tf
)

include_directories(SYSTEM ${Boost_INCLUDE_DIR} ${EIGEN3_INCLUDE_DIRS})
link_directories(${catkin_LIBRARY_DIRS}
"include/singleSteeringWheelDriver")

include_directories(
# include
  ${catkin_INCLUDE_DIRS}
  "include/singleSteeringWheelDriver"
)
#add_executable(driver_node src/driver_node.cpp)
#target_link_libraries(driver_node ${catkin_LIBRARIES} ${Boost_LIBRARIES})

#add_executable(demo_node src/demo.cpp)
#target_link_libraries(demo_node ${catkin_LIBRARIES} ${Boost_LIBRARIES})

add_executable(singleSteeringWheelDriver src/singleSteeringWheeldriver.cpp)
target_link_libraries(singleSteeringWheelDriver ${catkin_LIBRARIES} ${Boost_LIBRARIES} libcontrolcan.so pthread)

#target_link_libraries(driver_node libcontrolcan.so pthread)
#target_link_libraries(demo_node libcontrolcan.so pthread)
#target_link_libraries(singleSteeringWheelDriver libcontrolcan.so pthread)
















































