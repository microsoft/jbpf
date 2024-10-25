## ignore the tests in submodule
set(CTEST_CUSTOM_TESTS_IGNORE
  stress_test
  unit_test
  local_primary_test
  concurrency_test
  system_test
  local_ipc_test
)
