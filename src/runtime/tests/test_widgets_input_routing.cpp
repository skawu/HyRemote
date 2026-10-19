// #162 criterion 1: deterministic coverage for the Widgets input receiver semantics that the product
// promise depends on - nested child geometry, WA_TransparentForMouseEvents, disabled children, and the
// implicit mouse grab during a drag. The adapter currently selects a receiver per event with
// QWidget::childAt(rootPoint) (widget_target.cpp:503), so this file states what Qt itself would do and
// checks the adapter against it. A failure here is evidence, not a nuisance: it names a real routing
// deviation exactly where the repository had no coverage at all before.
//
// The expectations encoded below are Qt's own semantics:
//   - childAt() ignores widgets with WA_TransparentForMouseEvents, so the parent receives;
//   - childAt() does NOT skip disabled widgets, but disabled widgets do not receive mouse events;
//   - the deepest visible child wins for nested geometry;
//   - a widget that received a press keeps receiving move/release until the press is released
//     (Qt's implicit mouse grab), even while the pointer is outside it.

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMouseEvent>
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

class Recorder final : public QWidget
{
public:
    explicit Recorder(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // Qt delivers mouse-move events without a held button only when tracking is enabled. Without this
        // the baseline cannot pass and every later routing assertion would be vacuous.
        setMouseTracking(true);
    }

    int moves = 0;
    int presses = 0;
    int releases = 0;
    QPointF lastPosition;

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        ++moves;
        lastPosition = event->position();
        event->accept();
    }

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
};

// Posts a pointer event whose source grid is the root's logical size, so source coordinates and root
// logical coordinates coincide and the assertion is about routing rather than about scaling (which the
// DPR registrations cover separately).
void postPointer(HyRemote::detail::TargetComponents &components,
                 const QWidget &root,
                 hyremote::InputEventKind kind,
                 const QPointF &rootPoint,
                 hyremote::PointerButton button = hyremote::PointerButton::None,
                 bool pressed = true)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport.width = static_cast<std::uint32_t>(root.width());
    event.sourceViewport.height = static_cast<std::uint32_t>(root.height());
    event.sourceViewport.devicePixelRatio = 1.0F;
    event.x = static_cast<float>(rootPoint.x());
    event.y = static_cast<float>(rootPoint.y());
    event.button = button;
    event.pressed = pressed;
    components.input->post(event);
    pump();
}

void testWidgetsRoutingSemantics()
{
    HyRemote::detail::resetFactories();

    Recorder root;
    root.resize(400, 300);

    // A: a plain child, the drag source.
    auto *childA = new Recorder(&root);
    childA->setGeometry(0, 0, 200, 150);

    // Transparent overlay covering part of A: childAt() must ignore it, so A receives.
    auto *transparent = new Recorder(&root);
    transparent->setGeometry(20, 20, 60, 60);
    transparent->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Nested geometry: B owns an inner child, and the innermost visible one must win.
    auto *childB = new Recorder(&root);
    childB->setGeometry(200, 0, 200, 150);
    auto *innerB = new Recorder(childB);
    innerB->setGeometry(20, 20, 100, 60);

    // Disabled child: Qt does not deliver mouse events to a disabled widget.
    auto *disabled = new Recorder(&root);
    disabled->setGeometry(280, 180, 100, 80);
    disabled->setEnabled(false);

    root.show();
    pump();

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    check(components.input != nullptr, "widgets input component is available");
    if (!components.input)
        return;

    // 0. Baseline: a move inside A must reach A, otherwise the later assertions prove nothing.
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(100, 100));
    check(childA->moves >= 1, "baseline: a move inside child A reaches child A");

    // 1. Transparent child: the parent (A) receives, the transparent widget does not.
    const int movesBeforeTransparent = childA->moves;
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(50, 50));
    check(childA->moves > movesBeforeTransparent,
          "transparent child: the widget under WA_TransparentForMouseEvents is skipped, parent receives");
    check(transparent->moves == 0, "transparent child never receives a move");

    // 2. Nested geometry: the innermost visible child wins.
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(250, 50));
    check(innerB->moves >= 1, "nested geometry: the innermost child receives the move");
    check(childB->moves == 0, "nested geometry: the outer child does not also receive it");

    // 3. Disabled child: Qt delivers no mouse events to it.
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(320, 220));
    check(disabled->moves == 0, "disabled child does not receive mouse events");

    // 4. Implicit mouse grab: after a press inside A, A keeps receiving while the pointer is held
    //    outside it, until the button is released.
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(100, 100),
                hyremote::PointerButton::Left);
    check(childA->presses >= 1, "press inside child A reaches child A");
    const int movesBeforeDrag = childA->moves;
    postPointer(components, root, hyremote::InputEventKind::PointerMove, QPointF(380, 280));
    check(childA->moves > movesBeforeDrag,
          "implicit grab: child A keeps receiving while the button is held outside it");
    postPointer(components, root, hyremote::InputEventKind::PointerButton, QPointF(380, 280),
                hyremote::PointerButton::Left, false);
    check(childA->releases >= 1, "implicit grab: the release is delivered to child A");

    components.input.reset();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testWidgetsRoutingSemantics();

    std::cout << (failures == 0 ? "PASS: widgets input routing semantics"
                                : "FAIL: widgets input routing semantics")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
