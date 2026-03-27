# CMake generated Testfile for 
# Source directory: /home/ph/work/myiot/build-linux-local/deps/Poco/Util/testsuite
# Build directory: /home/ph/work/myiot/build-linux-local/poco-official/Util/testsuite
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(Util "/home/ph/work/myiot/build-linux-local/bin/Util-testrunner" "-ignore" "/home/ph/work/myiot/cppignore.lnx" "-all")
set_tests_properties(Util PROPERTIES  ENVIRONMENT "POCO_BASE=/home/ph/work/myiot" WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/poco-official/Util/testsuite" _BACKTRACE_TRIPLES "/home/ph/work/myiot/build-linux-local/deps/Poco/Util/testsuite/CMakeLists.txt;32;add_test;/home/ph/work/myiot/build-linux-local/deps/Poco/Util/testsuite/CMakeLists.txt;0;")
