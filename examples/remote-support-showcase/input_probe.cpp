#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <iostream>

namespace {

QByteArray widgetName(const QWidget *widget)
{
    if (!widget)
        return QByteArrayLiteral("-");
    if (!widget->objectName().isEmpty())
        return widget->objectName().toUtf8();
    return QByteArray(widget->metaObject()->className());
}

const char *mouseTypeName(QEvent::Type type)
{
    switch (type) {
    case QEvent::MouseButtonPress: return "MouseButtonPress";
    case QEvent::MouseButtonDblClick: return "MouseButtonDblClick";
    case QEvent::MouseButtonRelease: return "MouseButtonRelease";
    case QEvent::MouseMove: return "MouseMove";
    default: return "MouseUnknown";
    }
}

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

        const QByteArray name = widgetName(widget);
        const char *className = widget->metaObject()->className();
        const int spontaneous = event->spontaneous() ? 1 : 0;

        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove: {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (event->type() == QEvent::MouseMove && mouse->buttons() == Qt::NoButton)
                break;
            std::cout << "SHOWCASE_INPUT type=" << mouseTypeName(event->type())
                      << " widget=" << name.constData()
                      << " class=" << className
                      << " x=" << mouse->position().x()
                      << " y=" << mouse->position().y()
                      << " button=" << static_cast<int>(mouse->button())
                      << " buttons=" << static_cast<int>(mouse->buttons())
                      << " spontaneous=" << spontaneous
                      << std::endl;
            break;
        }
        case QEvent::Wheel: {
            const auto *wheel = static_cast<QWheelEvent *>(event);
            std::cout << "SHOWCASE_INPUT type=Wheel"
                      << " widget=" << name.constData()
                      << " class=" << className
                      << " angleX=" << wheel->angleDelta().x()
                      << " angleY=" << wheel->angleDelta().y()
                      << " pixelX=" << wheel->pixelDelta().x()
                      << " pixelY=" << wheel->pixelDelta().y()
                      << " spontaneous=" << spontaneous
                      << std::endl;
            break;
        }
        case QEvent::ContextMenu: {
            const auto *context = static_cast<QContextMenuEvent *>(event);
            std::cout << "SHOWCASE_INPUT type=ContextMenu"
                      << " widget=" << name.constData()
                      << " class=" << className
                      << " reason=" << static_cast<int>(context->reason())
                      << " spontaneous=" << spontaneous
                      << std::endl;
            break;
        }
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            const auto *key = static_cast<QKeyEvent *>(event);
            std::cout << "SHOWCASE_INPUT type="
                      << (event->type() == QEvent::KeyPress ? "KeyPress" : "KeyRelease")
                      << " widget=" << name.constData()
                      << " class=" << className
                      << " key=" << key->key()
                      << " modifiers=" << static_cast<int>(key->modifiers())
                      << " autorepeat=" << (key->isAutoRepeat() ? 1 : 0)
                      << " spontaneous=" << spontaneous
                      << std::endl;
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
