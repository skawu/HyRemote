// Behavioral acceptance for the remote pointer path after the input ingress moved to Qt's
// window-system boundary. This fixture intentionally does not reproduce Qt semantics in HyRemote:
// it only proves that raw remote facts reach ordinary Widgets with native Qt behavior.

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QThread>
#include <QWidget>

#include <iostream>

#include "detail/component_factories.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

void pump()
{
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

class Probe final : public QWidget
{
public:
    explicit Probe(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
    }

    int presses = 0;
    int releases = 0;
    int doubleClicks = 0;
    int moves = 0;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        ++presses;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++releases;
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        ++doubleClicks;
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++moves;
        event->accept();
    }
};

void postPointer(HyRemote::detail::TargetComponents &components,
                 const QWidget &root,
                 hyremote::InputEventKind kind,
                 const QPointF &point,
                 hyremote::PointerButton button = hyremote::PointerButton::None,
                 bool pressed = true,
                 float scrollY = 0.0F)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport = {static_cast<std::uint32_t>(root.width()),
                            static_cast<std::uint32_t>(root.height()),
                            1.0F};
    event.x = static_cast<float>(point.x());
    event.y = static_cast<float>(point.y());
    event.button = button;
    event.pressed = pressed;
    event.scrollY = scrollY;
    components.input->post(event);
}

void click(HyRemote::detail::TargetComponents &components,
           const QWidget &root,
           const QPointF &point,
           hyremote::PointerButton button = hyremote::PointerButton::Left)
{
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point, button, true);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point, button, false);
    pump();
}

void doubleClick(HyRemote::detail::TargetComponents &components,
                 const QWidget &root,
                 const QPointF &point)
{
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, true);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, false);
    pump();
    QThread::msleep(5);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, true);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, point,
                hyremote::PointerButton::Left, false);
    pump();
}

void runAcceptance()
{
    HyRemote::detail::resetFactories();

    QWidget root;
    root.resize(520, 520);

    Probe probe(&root);
    probe.setGeometry(0, 0, 200, 180);

    QPushButton button(QStringLiteral("click"), &root);
    button.setGeometry(220, 10, 120, 40);
    int buttonClicks = 0;
    QObject::connect(&button, &QPushButton::clicked, [&] { ++buttonClicks; });

    QStandardItemModel model(&root);
    model.appendRow(new QStandardItem(QStringLiteral("one")));
    model.appendRow(new QStandardItem(QStringLiteral("two")));
    QListView list(&root);
    list.setGeometry(220, 70, 180, 120);
    list.setModel(&model);
    int activations = 0;
    QObject::connect(&list, &QListView::activated, [&] { ++activations; });

    QScrollArea scroll(&root);
    scroll.setGeometry(0, 220, 220, 120);
    auto *scrollContent = new QWidget;
    scrollContent->resize(200, 800);
    scroll.setWidget(scrollContent);

    root.show();
    pump();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    check(components.input != nullptr, "widgets input component is available");
    if (!components.input)
        return;

    click(components, root, button.geometry().center());
    check(buttonClicks == 1, "Qt Widgets owns normal child hit testing and QPushButton activation");

    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    probe.doubleClicks = 0;
    doubleClick(components, root, QPointF(100, 100));
    check(probe.doubleClicks == 1,
          "Qt classifies one remote press/release pair sequence as one double click");

    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    probe.doubleClicks = 0;
    click(components, root, QPointF(100, 100));
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(500, 500));
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(100, 100));
    pump();
    QThread::msleep(5);
    click(components, root, QPointF(100, 100));
    check(probe.doubleClicks == 0,
          "Qt itself invalidates a double click after a far pointer excursion");

    // Queue delay must not change click timing: the Runtime records acceptance time, then Qt applies
    // its own classifier when the queued facts finally reach the GUI thread.
    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    probe.doubleClicks = 0;
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
                hyremote::PointerButton::Left, true);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
                hyremote::PointerButton::Left, false);
    QThread::msleep(5);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
                hyremote::PointerButton::Left, true);
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
                hyremote::PointerButton::Left, false);
    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 50UL);
    pump();
    check(probe.doubleClicks == 1,
          "GUI queue delay does not alter the accepted remote click interval");

    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    const QRect rowRect = list.visualRect(model.index(1, 0));
    const QPoint rowInList = list.viewport()->mapTo(&list, rowRect.center());
    const QPoint rowInRoot = list.mapTo(&root, rowInList);
    doubleClick(components, root, rowInRoot);
    check(activations == 1,
          "QAbstractItemView activation comes from Qt's normal double-click path");

    const QPoint scrollPoint = scroll.mapTo(&root, scroll.viewport()->rect().center());
    const int beforeScroll = scroll.verticalScrollBar()->value();
    postPointer(components, root, hyremote::InputEventKind::PointerScroll, scrollPoint,
                hyremote::PointerButton::None, true, -1.0F);
    pump();
    check(scroll.verticalScrollBar()->value() > beforeScroll,
          "wheel delivery reaches QScrollArea through Qt's normal propagation path");

    components.input.reset();
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    runAcceptance();
    HyRemote::detail::resetFactories();

    std::cout << (failures == 0 ? "PASS: widgets window-system input acceptance"
                                : "FAIL: widgets window-system input acceptance")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
