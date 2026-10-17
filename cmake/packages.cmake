if(BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.18.0
    VERSION 1.18.0
    OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
  )
endif()