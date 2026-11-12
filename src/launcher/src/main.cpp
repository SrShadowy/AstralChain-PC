#include <QApplication>
#include <QIcon>
#include <cstdio>
#include <cstring>
#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::printf("Astral Chain PC %s (%s)\n", AC_APP_VERSION, AC_DEVELOPER_NAME);
            return 0;
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("AstralChainLauncher");
    app.setApplicationDisplayName("Astral Chain PC");
    app.setApplicationVersion(AC_APP_VERSION);
    app.setOrganizationName("AstralChain");
    app.setDesktopFileName(AC_APP_ID);
    app.setWindowIcon(QIcon(":/icons/astral-chain-pc.png"));

    ac::launcher::MainWindow window;
    window.show();

    return app.exec();
}
