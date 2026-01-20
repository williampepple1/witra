cmake_minimum_required(VERSION 3.16)

project(witra VERSION 1.0.0 LANGUAGES CXX)

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

# Create executable
if(WIN32)
    add_executable(witra WIN32
        ${SOURCES}
        ${HEADERS}
        ${RESOURCES}
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
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
