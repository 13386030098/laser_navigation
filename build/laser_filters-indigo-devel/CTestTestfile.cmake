# CMake generated Testfile for 
# Source directory: /home/zzz/laser_navigation/src/laser_filters-indigo-devel
# Build directory: /home/zzz/laser_navigation/build/laser_filters-indigo-devel
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(_ctest_laser_filters_rostest_test_test_scan_filter_chain.launch "/home/zzz/laser_navigation/build/catkin_generated/env_cached.sh" "/usr/bin/python" "/opt/ros/kinetic/share/catkin/cmake/test/run_tests.py" "/home/zzz/laser_navigation/build/test_results/laser_filters/rostest-test_test_scan_filter_chain.xml" "--return-code" "/opt/ros/kinetic/share/rostest/cmake/../../../bin/rostest --pkgdir=/home/zzz/laser_navigation/src/laser_filters-indigo-devel --package=laser_filters --results-filename test_test_scan_filter_chain.xml --results-base-dir \"/home/zzz/laser_navigation/build/test_results\" /home/zzz/laser_navigation/src/laser_filters-indigo-devel/test/test_scan_filter_chain.launch ")
add_test(_ctest_laser_filters_gtest_test_shadow_detector "/home/zzz/laser_navigation/build/catkin_generated/env_cached.sh" "/usr/bin/python" "/opt/ros/kinetic/share/catkin/cmake/test/run_tests.py" "/home/zzz/laser_navigation/build/test_results/laser_filters/gtest-test_shadow_detector.xml" "--return-code" "/home/zzz/laser_navigation/devel/lib/laser_filters/test_shadow_detector --gtest_output=xml:/home/zzz/laser_navigation/build/test_results/laser_filters/gtest-test_shadow_detector.xml")
