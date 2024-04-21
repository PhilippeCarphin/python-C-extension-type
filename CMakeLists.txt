cmake_minimum_required(VERSION 3.10)

project(python_C_extension_class C)

# Youcompleteme
# https://bastian.rieck.me/blog/posts/2015/ycm_cmake/
SET( CMAKE_EXPORT_COMPILE_COMMANDS ON )

find_package(Python 3 REQUIRED Development NumPy)
Python_add_library(mymodule MODULE mymodule.c)

configure_file(setup.in.sh setup.sh @ONLY)
