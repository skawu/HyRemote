#include "input_probe.hpp"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>

#include <iostream>

namespace {

class InputProbe final : public QObject
{
public:
    explicit InputProbe(QWidget *root)
        : QObject(root)
        , m_root(root)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || !m_root || (widget != m_root && !m_root->isAncestorOf(widget)))
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

private:
    QWidget *m_root = nullptr;
};

}  // namespace

void installInputProbe(QApplication &app, QWidget &root)
{
    app.installEventFilter(new InputProbe(&root));
}
