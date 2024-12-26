# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

    ############## Add definations ###############
    # list(APPEND ADD_DEFINATION
    #     "-D__FREERTOS__"
    #     "-DBUILDING_API_IMPL_SOURCE"
    #     "-DBUILD_TARGET_ARCHIVE"
    #     )
list(APPEND compile_definitions "-D__FREERTOS__")
# list(APPEND compile_options "-fasynchronous-unwind-tables")
# list(APPEND link_options "-fno-lto")



include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(videodoorbell_esp32)
