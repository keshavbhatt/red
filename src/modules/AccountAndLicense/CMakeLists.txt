# SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
# SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

cmake_minimum_required(VERSION 3.21)

# Find Qt6 if not already found by a parent project.
if(NOT Qt6_FOUND)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network)
endif()

add_library(account_license STATIC
    AccountLicenseConfig.h
    AccountLicenseStore.h
    AccountLicenseStore.cpp
    AccountLicenseManager.h
    AccountLicenseManager.cpp
)

# AccountLicenseManager uses Q_OBJECT — must be processed by MOC regardless
# of what the parent project sets for CMAKE_AUTOMOC.
set_target_properties(account_license PROPERTIES AUTOMOC ON)

target_include_directories(account_license PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(account_license PUBLIC
    Qt6::Core
    Qt6::Network
)

# Suppress debug output in non-debug builds (consistent with the rest of WonderWall)
target_compile_definitions(account_license PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:QT_NO_DEBUG_OUTPUT>
)

if(MSVC)
    target_compile_options(account_license PRIVATE /utf-8)
endif()

