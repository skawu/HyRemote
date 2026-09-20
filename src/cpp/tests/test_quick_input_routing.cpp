// #162 criterion 2: prove Quick/QML input reaches the intended item and the focused text target, not merely
// the QQuickWindow. The adapter sends every event to the window (quick_target.cpp:597/622/647/657) and relies
// on Qt Quick's own routing, and until now the tests only proved window-level arrival. This file states the
// item-level expectations and measures them.
//
// The baseline check comes first and on purpose: if a window-delivered synthetic event never reaches an item in
// this harness, then every later assertion would be vacuous rather than meaningful (the same trap the DPR and
// Widgets-routing increments hit).

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

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
    for (int i = 0; i < 30; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

class ProbeItem final : public QQuickItem
{
public:
    explicit ProbeItem(QQuickItem *parent = nullptr)
        : QQuickItem(parent)
    {
        setAcceptedMouseButtons(Qt::AllButtons);
        // In Qt Quick a move without a held button is a hover event; without this it never reaches the item.
        setAcceptHoverEvents(true);
        setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    }

    int moves = 0;
    int presses = 0;
    int releases = 0;
    int keys = 0;
    int imeEvents = 0;
    QString committedText;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        ++presses;
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++moves;
        event->accept();
    }

    void hoverMoveEvent(QHoverEvent *event) override
    {
        ++moves;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++releases;
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        ++keys;
        event->accept();
    }

    // QQuickItem has no inputMethodEvent virtual; the event arrives through event().
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::InputMethod) {
            auto *ime = static_cast<QInputMethodEvent *>(event);
            ++imeEvents;
            committedText += ime->commitString();
            event->accept();
            return true;
        }
        return QQuickItem::event(event);
    }
};

void postEvent(HyRemote::detail::TargetComponents &components,
               const QQuickWindow &window,
               hyremote::InputEventKind kind,
               const QPointF &windowPoint,
               hyremote::PointerButton button = hyremote::PointerButton::None,
               bool pressed = true,
               hyremote::KeyCode key = hyremote::KeyCode::Unknown,
               const char *text = nullptr)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport.width = static_cast<std::uint32_t>(window.width());
    event.sourceViewport.height = static_cast<std::uint32_t>(window.height());
    event.sourceViewport.devicePixelRatio = 1.0F;
    event.x = static_cast<float>(windowPoint.x());
    event.y = static_cast<float>(windowPoint.y());
    event.button = button;
    event.pressed = pressed;
    event.key = key;
    if (text)
        event.textUtf8 = text;
    components.input->post(event);
    pump();
}

void testQuickItemLevelRouting()
{
    HyRemote::detail::resetFactories();

    QQuickWindow window;
    window.resize(400, 300);

    ProbeItem a(window.contentItem());
    a.setPosition(QPointF(0, 0));
    a.setSize(QSizeF(200, 150));

    ProbeItem b(window.contentItem());
    b.setPosition(QPointF(200, 0));
    b.setSize(QSizeF(200, 150));

    window.show();
    pump();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&window, true);
    check(components.input != nullptr, "quick input component is available");
    if (!components.input)
        return;

    // 0. Baseline: a window-delivered synthetic move must reach the item under the pointer, otherwise the
    //    item-level claims below cannot be tested at all.
    postEvent(components, window, hyremote::InputEventKind::PointerMove, QPointF(100, 100));
    check(a.moves >= 1, "baseline: a move inside item A reaches item A (not only the window)");

    // 1. Item-level mouse delivery: the press goes to A, not to B and not nowhere.
    postEvent(components, window, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
              hyremote::PointerButton::Left);
    check(a.presses >= 1, "item-level routing: the press reaches the item under the pointer");
    check(b.presses == 0, "item-level routing: the other item does not receive it");
    postEvent(components, window, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
              hyremote::PointerButton::Left, false);
    check(a.releases >= 1, "item-level routing: the release reaches the same item");

    // 1b. Drag while held: with the button down, moves must keep reaching the same item even outside it.
    postEvent(components, window, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
              hyremote::PointerButton::Left);
    const int aMovesBeforeDrag = a.moves;
    const int bMovesBeforeDrag = b.moves;
    postEvent(components, window, hyremote::InputEventKind::PointerMove, QPointF(320, 240));
    check(a.moves > aMovesBeforeDrag, "drag: the held move keeps reaching the pressed item");
    check(b.moves == bMovesBeforeDrag, "drag: the item under the pointer does not steal the held move");
    postEvent(components, window, hyremote::InputEventKind::PointerButton, QPointF(320, 240),
              hyremote::PointerButton::Left, false);
    check(a.releases >= 2, "drag: the release still reaches the pressed item");

    // 2. Focus change: after moving active focus to B, keys must reach B and not A.
    b.forceActiveFocus();
    pump();
    check(window.activeFocusItem() == &b, "focus change: item B is the active focus item");
    const int aKeysBefore = a.keys;
    postEvent(components, window, hyremote::InputEventKind::Key, QPointF(0, 0),
              hyremote::PointerButton::None, true, hyremote::KeyCode::A);
    check(b.keys >= 1, "focus routing: the key reaches the focused item");
    check(a.keys == aKeysBefore, "focus routing: the previously focused item does not receive the key");

    // 3. Text: the committed text must reach the focused item, not only the window.
    postEvent(components, window, hyremote::InputEventKind::Text, QPointF(0, 0),
              hyremote::PointerButton::None, true, hyremote::KeyCode::Unknown, "hy");
    check(b.committedText.contains(QStringLiteral("hy")),
          "text routing: committed text reaches the focused item");

    components.input.reset();
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    testQuickItemLevelRouting();

    std::cout << (failures == 0 ? "PASS: quick item-level input routing"
                                : "FAIL: quick item-level input routing")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
