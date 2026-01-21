#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QSettings>
#include <QDir>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    // Enable high DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    QApplication app(argc, argv);
    
    // Application metadata
    app.setApplicationName("Witra");
    app.setApplicationDisplayName("Witra - Wireless Transfer");
    app.setOrganizationName("Witra");
    app.setOrganizationDomain("witra.app");
    app.setApplicationVersion("1.1.0");
    
    // Set application icon
    app.setWindowIcon(QIcon(":/icons/app.svg"));
    
    // Set default font
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(defaultFont);
    
    // Load global stylesheet
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = styleFile.readAll();
        app.setStyleSheet(style);
        styleFile.close();
    }
    
    // Ensure download directory exists
    QString downloadPath = QDir::homePath() + "/Downloads/Witra";
    QDir().mkpath(downloadPath);
    
    // Create and show main window
    Witra::MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
