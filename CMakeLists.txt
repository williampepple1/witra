cmake_minimum_required(VERSION 3.16)

project(witra VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt packages
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets 
    Network
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets 
    Network
)

# Source files
set(SOURCES
    main.cpp
    
    # Network
    src/network/NetworkDiscovery.cpp
    src/network/FileTransferServer.cpp
    src/network/FileTransferClient.cpp
    src/network/TransferSession.cpp
    
    # Core
    src/core/PeerManager.cpp
    src/core/TransferManager.cpp
    src/core/Peer.cpp
    src/core/TransferItem.cpp
    
    # UI
    src/ui/MainWindow.cpp
    src/ui/LobbyPage.cpp
    src/ui/TransferPage.cpp
    src/ui/PeerWidget.cpp
    src/ui/TransferWidget.cpp
    src/ui/ConnectionDialog.cpp
)

# Header files
set(HEADERS
    # Network
    src/network/NetworkDiscovery.h
    src/network/FileTransferServer.h
    src/network/FileTransferClient.h
    src/network/TransferSession.h
    src/network/Protocol.h
    
    # Core
    src/core/PeerManager.h
    src/core/TransferManager.h
    src/core/Peer.h
    src/core/TransferItem.h
    
    # UI
    src/ui/MainWindow.h
    src/ui/LobbyPage.h
    src/ui/TransferPage.h
    src/ui/PeerWidget.h
    src/ui/TransferWidget.h
    src/ui/ConnectionDialog.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Platform-specific resources
if(WIN32)
    # Windows resource file (for app icon in taskbar/explorer)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        set(WIN_RESOURCES resources/app.rc)
    endif()
endif()

if(APPLE)
    # macOS app icon
    set(MACOS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/witra.icns)
    if(EXISTS ${MACOS_ICON})
        set_source_files_properties(${MACOS_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set(MACOS_RESOURCES ${MACOS_ICON})
    endif()
endif()

# Create executable
if(WIN32)
    add_executable(witra WIN32
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${WIN_RESOURCES}
    )
elseif(APPLE)
    add_executable(witra MACOSX_BUNDLE
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
        ${MACOS_RESOURCES}
    )
    set_target_properties(witra PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Witra"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.witra.Witra"
        MACOSX_BUNDLE_INFO_STRING "Witra - Wireless Transfer"
        MACOSX_BUNDLE_ICON_FILE "witra.icns"
    )
else()
    add_executable(witra
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
    )
endif()

# Link Qt libraries
target_link_libraries(witra PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# Include directories
target_include_directories(witra PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows-specific settings
if(WIN32)
    target_link_libraries(witra PRIVATE ws2_32 iphlpapi)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS witra
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Test executable
add_executable(test_witra
    tests/test_suite.cpp
    src/network/NetworkDiscovery.cpp
    src/network/FileTransferServer.cpp
    src/network/FileTransferClient.cpp
    src/network/TransferSession.cpp
    src/core/PeerManager.cpp
    src/core/TransferManager.cpp
    src/core/Peer.cpp
    src/core/TransferItem.cpp
    ${RESOURCES}
)

target_link_libraries(test_witra PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

target_include_directories(test_witra PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_witra PRIVATE ws2_32 iphlpapi)
endif()

