if(BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.18.0
    VERSION 1.18.0
    OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
  )
endif()

if(BUILD_BENCHMARKS)
  CPMAddPackage(
    NAME CLI11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG v2.5.0
    VERSION 2.5.0
    OPTIONS "CLI11_BUILD_TESTS OFF" "CLI11_BUILD_EXAMPLES OFF"
  )
endif()
