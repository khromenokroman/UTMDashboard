cmake_minimum_required(VERSION 4.2)
project(utm-dashboard
        VERSION 4.2.6
        LANGUAGES CXX
        HOMEPAGE_URL "https://github.com/khromenokroman/UTMDashboard")

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/utm_dashboard.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE curl fmt::fmt)
