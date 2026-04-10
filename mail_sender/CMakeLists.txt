cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(mail_sender VERSION 1.0.0 LANGUAGES CXX)
endif()

find_package(OpenSSL REQUIRED)

add_library(mail_sender STATIC
  mail_sender.cpp
)

target_compile_features(mail_sender PUBLIC cxx_std_17)
target_include_directories(mail_sender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mail_sender PUBLIC OpenSSL::SSL OpenSSL::Crypto)
