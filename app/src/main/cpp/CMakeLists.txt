cmake_minimum_required(VERSION 3.22.1)
project(charter_gapi_android LANGUAGES C)

add_library(charter_gapi SHARED charter_gapi_jni.c
    ../../../../components/CharterGAPI/src/charter_gapi_core.c)
target_include_directories(charter_gapi PRIVATE
    ../../../../components/CharterGAPI/include)
set_target_properties(charter_gapi PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
