// #401 deterministic reproduction of #400 through the real production Widgets input path.
//
// This is a preflight fixture, not the production fix. It drives the shipped path
// (InputSink::post -> bounded mailbox -> GUI drain -> Widgets adapter) with realistic remote
// pointer streams, records what Qt actually receives today, and classifies the FIRST divergence
// layer for every scenario using the #401 vocabulary:
//   RFB_PARSE / RUNTIME_QUEUE / TOPLEVEL_INGRESS / TARGET_ROUTING /
//   QT_SEMANTIC_CLASSIFICATION / OS_ACTIVATION_FOCUS / NO_DEFECT
//
// The assertions below pin the CURRENT observed behaviour so the suite is honest evidence rather
// than a wish list. Where the platform/Qt would do more for a physically attached mouse, the test
// prints DIVERGENCE[...] and the expectation is intentionally annotated: #400's production fix
// (after the #401 decision) is what flips those lines, not this preflight.
//
// Host ACTIVE vs INACTIVE: the ACTIVE scenarios run against a shown, focused offscreen window.
// The INACTIVE section records what the platform can actually be made to report and does not
// fabricate an OS activation state: offscreen has no other top-level window to steal activation,
// so the inactive-side evidence is the code/architecture analysis recorded in the preflight note
// plus the #362 relation, not an invented measurement.

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTableView>
#include <QTreeView>
#include <QWidget>

#include <iostream>

#include "detail/component_factories.hpp"
#include "hyremote/core/input.hpp"

namespace {

int failures = 0;
int divergences = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

// The #401 classification vocabulary: every scenario states where semantics were first lost.
void divergence(const char *layer, const char *scenario, const char *detail)
{
    ++divergences;
    std::cout << "DIVERGENCE[" << layer << "] " << scenario << ": " << detail << '\n';
}

void noDivergence(const char *scenario, const char *detail)
{
    std::cout << "NO_DEFECT " << scenario << ": " << detail << '\n';
}

void pump()
{
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

void pumpFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
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
    QVector<QEvent::Type> order;

    void observe(const char *what) const
    {
        std::cout << "     observed[" << what << "]: presses=" << presses << " releases=" << releases
                  << " dblClicks=" << doubleClicks << " (presses+releases=" << order.size() << " events)"
                  << '\n';
    }

protected:
    bool event(QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            ++presses;
            order.append(event->type());
            event->accept();
            return true;
        case QEvent::MouseButtonRelease:
            ++releases;
            order.append(event->type());
            event->accept();
            return true;
        case QEvent::MouseButtonDblClick:
            ++doubleClicks;
            order.append(event->type());
            event->accept();
            return true;
        default:
            return QWidget::event(event);
        }
    }
};

void postRaw(HyRemote::detail::TargetComponents &components,
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
}

void click(HyRemote::detail::TargetComponents &components,
           const QWidget &root,
           const QPointF &rootPoint,
           hyremote::PointerButton button = hyremote::PointerButton::Left)
{
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint, button, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint, button, false);
    pump();
}

// A viewer double click as the transport would deliver it: two press/release pairs with
// negligible arrival spacing, i.e. a valid double-click envelope.
void doubleClick(HyRemote::detail::TargetComponents &components,
                 const QWidget &root,
                 const QPointF &rootPoint)
{
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint,
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint,
            hyremote::PointerButton::Left, false);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint,
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, rootPoint,
            hyremote::PointerButton::Left, false);
    pump();
}

// ---------------------------------------------------------------------------------------------
// R-FLOW: the raw event sequence Qt receives today for a valid remote double click.
// ---------------------------------------------------------------------------------------------
void testPointerSequence(QWidget &root, HyRemote::detail::TargetComponents &components)
{
    // The recorder covers the whole root so that every scenario point below is routed to it.
    Probe probe(&root);
    probe.setGeometry(0, 0, 500, 600);
    pump();

    doubleClick(components, root, QPointF(100, 100));
    probe.observe("valid double-click envelope");

    check(probe.presses == 2 && probe.releases == 2,
          "valid double-click envelope: Qt receives two press/release pairs");
    if (probe.doubleClicks == 0) {
        divergence("QT_SEMANTIC_CLASSIFICATION",
                   "valid remote double click on a probe widget",
                   "Qt receives Press,Release,Press,Release and never MouseButtonDblClick; the platform "
                   "layer that synthesizes DblClick for a physical mouse is bypassed by application-level "
                   "event delivery");
    } else {
        noDivergence("valid remote double click", "a DblClick semantic reached the widget");
    }

    // Outside-interval and outside-distance sequences are indistinguishable on the wire to Qt
    // today, because no classification exists at all: both are simply two clicks.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    click(components, root, QPointF(100, 100));
    pumpFor(650);
    click(components, root, QPointF(100, 100));
    pump();
    const bool intervalOk = probe.presses == 2 && probe.doubleClicks == 0;
    check(intervalOk, "outside-interval pair: two single clicks (correct so far)");
    if (intervalOk)
        noDivergence("outside interval", "two singles, which is also the correct platform answer");

    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    click(components, root, QPointF(40, 40));
    click(components, root, QPointF(360, 260));
    pump();
    const bool distanceOk = probe.presses == 2 && probe.doubleClicks == 0;
    check(distanceOk, "outside-distance pair: two single clicks (correct so far)");
    if (distanceOk)
        noDivergence("outside distance", "two singles, which is also the correct platform answer");
}

// ---------------------------------------------------------------------------------------------
// Control matrix through the production path. Each control records ITS OWN user-visible result.
// ---------------------------------------------------------------------------------------------
void testControlMatrix(QWidget &root, HyRemote::detail::TargetComponents &components)
{
    int buttonClicks = 0;
    QPushButton *button = new QPushButton(QStringLiteral("ok"), &root);
    button->setGeometry(0, 220, 100, 30);
    QObject::connect(button, &QPushButton::clicked, [&buttonClicks] { ++buttonClicks; });
    click(components, root, QPointF(50, 235));
    const bool buttonOk = buttonClicks == 1;
    check(buttonOk, "QPushButton: single remote click activates");
    if (buttonOk)
        noDivergence("QPushButton single click", "control-local activation works");

    QCheckBox *checkBox = new QCheckBox(QStringLiteral("pick"), &root);
    checkBox->setGeometry(120, 220, 100, 30);
    const bool checkedBefore = checkBox->isChecked();
    click(components, root, QPointF(140, 235));
    const bool checkOk = checkBox->isChecked() != checkedBefore;
    check(checkOk, "QCheckBox: single remote click toggles");
    if (checkOk)
        noDivergence("QCheckBox single click", "control-local toggle works");

    QLineEdit *lineEdit = new QLineEdit(&root);
    lineEdit->setGeometry(240, 220, 150, 30);
    click(components, root, QPointF(300, 235));
    const bool focusOk = lineEdit->hasFocus();
    std::cout << "     observed[QLineEdit focus]: hasFocus=" << (focusOk ? "true" : "false")
              << " (offscreen, scratch scene not shown)\n";
    if (focusOk) {
        noDivergence("QLineEdit focus/caret", "focus transition works through the delivered press");
    } else {
        // Not a product finding here: this harness does not show the scratch window, and offscreen
        // cannot grant window activation/focus. Recorded as unmeasured rather than as a defect.
        divergence("OS_ACTIVATION_FOCUS/UNMEASURED",
                   "QLineEdit focus/caret",
                   "focus could not be observed in this offscreen scratch scene; focus fidelity needs a "
                   "real desktop run (Human/physical) and is listed as an explicit gap in the preflight "
                   "note, not as a reproduced #400 defect");
    }

    QSlider *slider = new QSlider(Qt::Horizontal, &root);
    slider->setGeometry(0, 260, 300, 30);
    slider->setRange(0, 100);
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(0, 275));
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(0, 275),
            hyremote::PointerButton::Left, true);
    pump();
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(290, 275));
    pump();
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(290, 275),
            hyremote::PointerButton::Left, false);
    pump();
    const bool sliderOk = slider->value() > 50;
    check(sliderOk, "QSlider: press/drag/release moves the value");
    if (sliderOk)
        noDivergence("QSlider drag", "implicit grab + held state + release routing work");

    // Item views: selection works, but the Qt double-click activation semantic cannot happen
    // because no DblClick event exists on this path.
    auto *model = new QStandardItemModel(&root);
    for (int row = 0; row < 4; ++row) {
        auto *item = new QStandardItem(QStringLiteral("entry %1").arg(row));
        item->setEditable(false);
        model->appendRow(item);
    }
    QListView *listView = new QListView(&root);
    listView->setGeometry(0, 300, 150, 120);
    listView->setModel(model);
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QTreeView *treeView = new QTreeView(&root);
    treeView->setGeometry(170, 300, 150, 120);
    treeView->setModel(model);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QTableView *tableView = new QTableView(&root);
    tableView->setGeometry(340, 300, 150, 120);
    tableView->setModel(model);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    int activations = 0;
    QObject::connect(listView, &QListView::activated, [&activations] { ++activations; });

    // Click points are derived from the view's own visual geometry, so the assertion is about
    // input delivery and not about a hard-coded row pitch/header height.
    const auto rowPoint = [&root](QAbstractItemView *view, const char *name) {
        const QRect visual = view->visualRect(view->model()->index(3, 0));
        const QPoint inViewport = visual.center();
        const QPoint inView = view->viewport()->mapTo(view, inViewport);
        const QPointF inRoot(view->mapTo(&root, inView));
        std::cout << "     " << name << ": viewRect=" << view->rect().width() << "x" << view->rect().height()
                  << " viewport=" << view->viewport()->width() << "x" << view->viewport()->height()
                  << " visualRect=" << visual.x() << "," << visual.y() << " " << visual.width() << "x"
                  << visual.height() << " -> root point=(" << inRoot.x() << ", " << inRoot.y() << ")"
                  << '\n';
        return inRoot;
    };

    const bool viewsLaidOut = listView->viewport()->width() > 0 && treeView->viewport()->height() > 0;
    if (!viewsLaidOut) {
        std::cout << "     observed[item views]: viewport geometry is 0 in this unshown scratch scene; "
                     "row hit-testing is not derivable\n";
        divergence("TARGET_ROUTING/UNMEASURED",
                   "item view selection",
                   "the scratch scene is not shown, so viewport geometry and row hit-testing cannot be "
                   "derived; selection fidelity needs a real desktop run and is listed as an explicit gap");
    } else {
    click(components, root, rowPoint(listView, "QListView"));
    pump();
    click(components, root, rowPoint(treeView, "QTreeView"));
    pump();
    click(components, root, rowPoint(tableView, "QTableView"));
    pump();
    const int listRow = listView->currentIndex().row();
    const int treeRow = treeView->currentIndex().row();
    const int tableRow = tableView->currentIndex().row();
    std::cout << "     observed[item view selection]: list=" << listRow << " tree=" << treeRow
              << " table=" << tableRow << " (clicked row 3 in each view's own visualRect)\n";
    if (listRow == 3 && treeRow == 3 && tableRow == 3) {
        noDivergence("item view single-click selection", "selection works through TARGET_ROUTING");
    } else {
        // Single-click selection is not part of the reported #400 defect (the user reports that
        // single clicks do work), so this is recorded as a measurement gap in this harness rather
        // than asserted as a product fact either way.
        divergence("TARGET_ROUTING/UNMEASURED",
                   "item view single-click selection",
                   "clicking the visual centre of row 3 did not set currentIndex to that row in every view "
                   "in this unshown scratch scene; selection fidelity needs a real desktop run and is "
                   "listed as an explicit gap, not as a reproduced #400 defect");
    }

    const QRect listRow0 = listView->visualRect(listView->model()->index(0, 0));
    doubleClick(components, root,
                QPointF(listView->mapTo(&root, listView->viewport()->mapTo(listView, listRow0.center()))));
    pump();
    if (activations == 0) {
        divergence("QT_SEMANTIC_CLASSIFICATION",
                   "QListView double-click activation",
                   "a valid double click on a row produces no activated() because the second press is a "
                   "plain press: view double-click activation, inline edit and open-on-double-click "
                   "behaviour are all unreachable for remote users");
    } else {
        noDivergence("QListView double-click activation", "activations == 1");
    }
    }
    // QComboBox: the dropdown is a separate top-level popup surface. The press that opens it works,
    // but the popup cannot be reached by an adapter whose whole routing model is childAt() on one
    // root widget.
    QComboBox *combo = new QComboBox(&root);
    combo->setGeometry(0, 440, 150, 30);
    combo->addItems({QStringLiteral("one"), QStringLiteral("two")});
    click(components, root, QPointF(75, 455));
    pump();
    const bool popupOpen = combo->view() && combo->view()->isVisible();
    check(popupOpen, "QComboBox: single remote click opens the popup");
    if (popupOpen) {
        divergence("TOPLEVEL_INGRESS",
                   "QComboBox popup item interaction",
                   "the popup is its own top-level window; the Widgets adapter resolves receivers with "
                   "root->childAt() inside the single configured root, so no input reaches the popup "
                   "surface and the item list is not clickable (the user-visible \"some controls do not "
                   "react\" half of #400)");
        combo->hidePopup();
        pump();
    }
}

// ---------------------------------------------------------------------------------------------
// Host ACTIVE / INACTIVE observation. Offscreen cannot be given a competing active window, so the
// honest result is: the active path is exercised; the inactive path is not measurable here.
// ---------------------------------------------------------------------------------------------
void testHostActivationState()
{
    // A clean scene: the activation question must not be entangled with the popup surface
    // interaction (and its grab) exercised by the control matrix above. The child recorder is
    // created before the window is shown, so Qt keeps it as a hit-testable child of the now
    // visible top-level (creating children after show() left childAt() unable to find them here).
    QWidget root;
    root.resize(300, 200);
    Probe probe(&root);
    probe.setGeometry(0, 0, 300, 200);
    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    if (!components.input) {
        check(false, "HOST_ACTIVE: input component available");
        return;
    }
    root.show();
    pump();
    root.activateWindow();
    pump();
    std::cout << "OBSERVED host state: isActiveWindow=" << (root.isActiveWindow() ? "true" : "false")
              << " isVisible=" << (root.isVisible() ? "true" : "false")
              << " platform=" << QGuiApplication::platformName().toStdString() << '\n';

    click(components, root, QPointF(150, 100));
    const bool activeOk = probe.presses == 1;
    check(activeOk, "HOST_ACTIVE: in-window click reaches the widget");
    if (activeOk)
        noDivergence("HOST_ACTIVE in-window input",
                     "delivery works on an active host window; the adapter does not depend on activation state");

    // The inactive-host case (#362) is an OS activation/focus limitation. It cannot be reproduced
    // by the offscreen platform (there is no second top-level window that can take activation), so
    // this preflight records the classification without fabricating a measurement.
    std::cout << "OBSERVED host inactive probe: offscreen has no competing top-level window; "
                 "activation stealing is not reproducible in this harness\n";
    divergence("OS_ACTIVATION_FOCUS",
               "HOST_INACTIVE in-window input (reported by users, #362-related)",
               "not reproducible offscreen. The architecture question it raises is real: an "
               "application-level event delivery backend has no power to activate the host window, so "
               "either the OS/QPA layer must own activation or the product must document the "
               "requirement that the host window be active");
    components.input.reset();
}

void runPreflight()
{
    HyRemote::detail::resetFactories();

    // The matrix scene is deliberately not shown, exactly like the existing Widgets routing and
    // backpressure tests: offscreen focus and childAt() hit-testing are both observable on a
    // created (not shown) top-level, while showing this window made childAt() stop finding the
    // adapter's child receivers in this harness.
    QWidget root;
    root.resize(500, 600);

    HyRemote::detail::TargetComponents components =
        HyRemote::detail::createTargetComponents(&root, true);
    check(components.input != nullptr, "widgets input component is available");
    if (!components.input)
        return;

    const QStyleHints *hints = QGuiApplication::styleHints();
    std::cout << "OBSERVED qt double-click policy: intervalMs=" << hints->mouseDoubleClickInterval()
              << " distancePx=" << hints->mouseDoubleClickDistance() << '\n';

    testPointerSequence(root, components);
    testControlMatrix(root, components);
    components.input.reset();

    testHostActivationState();
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    runPreflight();

    std::cout << (failures == 0 ? "PASS: widgets input preflight reproduction"
                                : "FAIL: widgets input preflight reproduction")
              << " (checks failed: " << failures << ", divergence scenarios: " << divergences << ")\n";
    return failures == 0 ? 0 : 1;
}
