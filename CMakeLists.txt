cmake_minimum_required(VERSION 3.26)
project(utm-dashboard
        VERSION 0.3.0
        LANGUAGES CXX
        HOMEPAGE_URL "https://github.com/khromenokroman/UTMDashboard")

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/utm_dashboard.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt cpp-httplib)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES configuration/utms.json
        DESTINATION /etc)

install(FILES systemd/utm-dashboard.service
        DESTINATION /usr/lib/systemd/system)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "khromenokroman")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "khromenokroman")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "UTM dashboard server")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "khromenokroman")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
