cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32-SpectrumAnalyzer)
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp-dsp)
