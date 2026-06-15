add_executable(somni_tests test_kernel.cpp)
target_link_libraries(somni_tests PRIVATE SOMNI::kernel Catch2::Catch2WithMain)
add_test(NAME SomniKernelTests COMMAND somni_tests)
