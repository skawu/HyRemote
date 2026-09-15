#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

#include <iostream>

namespace {

class InputProbe final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget)
            return false;

        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            std::cout << "SHOWCASE_POINTER x=" << mouse->position().x()
                      << " y=" << mouse->position().y() << std::endl;
            break;
        }
        case QEvent::KeyPress: {
            const auto *key = static_cast<QKeyEvent *>(event);
            std::cout << "SHOWCASE_KEY key=" << key->key() << std::endl;
            break;
        }
        default:
            break;
        }
        return false;
    }
};

void scheduleGlobalInputProbe()
{
    QTimer::singleShot(0, [] {
        auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
        if (app)
            app->installEventFilter(new InputProbe(app));
    });
}

}  // namespace

Q_COREAPP_STARTUP_FUNCTION(scheduleGlobalInputProbe)
