#include <QCoreApplication>
#include <QHostAddress>
#include <QtGlobal>

#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QHostAddress wildcard(QHostAddress::AnyIPv4);
    if (wildcard.isNull())
        return 2;

    std::cout << "RUNTIME_QT_VERSION=" << qVersion() << '\n';
    std::cout << "PUBLIC_LINK_PROBE=PASS\n";
    return 0;
}
