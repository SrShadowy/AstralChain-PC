#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AstralChainLauncher");
    app.setOrganizationName("AstralChain");

    ac::launcher::MainWindow window;
    window.show();

    return app.exec();
}
