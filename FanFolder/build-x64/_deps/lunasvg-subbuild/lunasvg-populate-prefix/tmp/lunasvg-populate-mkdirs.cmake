# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-src")
  file(MAKE_DIRECTORY "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-src")
endif()
file(MAKE_DIRECTORY
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-build"
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix"
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/tmp"
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/src/lunasvg-populate-stamp"
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/src"
  "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/src/lunasvg-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/src/lunasvg-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "//wsl.localhost/Ubuntu/home/au19277/projekter/win-dir-fan/FanFolder/build-x64/_deps/lunasvg-subbuild/lunasvg-populate-prefix/src/lunasvg-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
