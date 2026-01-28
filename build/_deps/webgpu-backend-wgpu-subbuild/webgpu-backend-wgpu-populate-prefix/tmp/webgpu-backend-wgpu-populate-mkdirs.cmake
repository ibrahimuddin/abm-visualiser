# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-src"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-build"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/tmp"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/src/webgpu-backend-wgpu-populate-stamp"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/src"
  "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/src/webgpu-backend-wgpu-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/src/webgpu-backend-wgpu-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ibrahim/PRJ/ABM_Visualiser/build/_deps/webgpu-backend-wgpu-subbuild/webgpu-backend-wgpu-populate-prefix/src/webgpu-backend-wgpu-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
