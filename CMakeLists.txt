cmake_minimum_required(VERSION 3.16)

project(Automative_DashBoard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)
set(CMAKE_AUTOMOC ON)      # Tự động chạy moc cho Q_OBJECT
set(CMAKE_AUTORCC ON)      # Tự động xử lý .qrc resource files
find_package(Qt6 REQUIRED COMPONENTS Quick SerialPort)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appAutomative_DashBoard
    main.cpp

    backend/DashboardController.cpp
    backend/SerialWorker.cpp
    resources.qrc
)


target_sources(appAutomative_DashBoard PRIVATE
    backend/DashboardController.h
    backend/SerialWorker.h
)
qt_add_qml_module(appAutomative_DashBoard
    URI Automative_DashBoard
    QML_FILES
        qml/main.qml
        qml/Speedometer.qml
        qml/Tachometer.qml
        qml/GearDisplay.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appAutomative_DashBoard PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appAutomative_DashBoard
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appAutomative_DashBoard
    PRIVATE Qt6::Quick
            Qt6::SerialPort
            Qt6::Core
            Qt6::Qml
)

install(TARGETS appAutomative_DashBoard
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
