cmake_minimum_required(VERSION 3.16)

project(mail_sender_sample VERSION 1.0.0 LANGUAGES CXX)

find_package(yaml-cpp REQUIRED)

add_subdirectory(mail_sender)

add_executable(sample_mail_sender
  sample_mail_sender_main.cpp
)

target_compile_features(sample_mail_sender PRIVATE cxx_std_17)
target_link_libraries(sample_mail_sender PRIVATE mail_sender yaml-cpp)
