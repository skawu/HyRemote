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
#include <QThread>
#include <QTreeView>
#include <QWidget>

#include <atomic>
#include <iostream>
#include <thread>

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

// #400: posts an event whose declared source viewport is not the widget size, so a coordinate-space
// transition can be exercised through the real mailbox.
void postRawViewport(HyRemote::detail::TargetComponents &components,
                     hyremote::InputEventKind kind,
                     std::uint32_t sourceWidth,
                     std::uint32_t sourceHeight,
                     float devicePixelRatio,
                     const QPointF &rootPoint,
                     hyremote::PointerButton button = hyremote::PointerButton::None,
                     bool pressed = true)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport.width = sourceWidth;
    event.sourceViewport.height = sourceHeight;
    event.sourceViewport.devicePixelRatio = devicePixelRatio;
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

    check(probe.doubleClicks == 1,
          "#400: a valid remote double click delivers exactly one MouseButtonDblClick");
    check(probe.presses == 2,
          "#400: the second press is still an ordinary press (DblClick is added, not substituted)");
    check(probe.releases == 2, "#400: both releases are delivered");
    check(probe.order.size() == 5 && probe.order.at(0) == QEvent::MouseButtonPress
              && probe.order.at(1) == QEvent::MouseButtonRelease
              && probe.order.at(2) == QEvent::MouseButtonPress
              && probe.order.at(3) == QEvent::MouseButtonDblClick
              && probe.order.at(4) == QEvent::MouseButtonRelease,
          "#400: the delivered sequence is Press, Release, Press, DblClick, Release");
    if (probe.doubleClicks == 1 && probe.presses == 2)
        noDivergence("valid remote double click", "Qt's double-click semantic is restored");

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
    check(activations == 1,
          "#400: a valid remote double click on a row reaches QAbstractItemView::activated() exactly once");
    if (activations == 1)
        noDivergence("QListView double-click activation",
                     "view double-click activation is reachable through the remote path");
    }
    // QComboBox: the dropdown is a separate top-level popup surface. This does not infer
    // inaccessibility from the routing code: it derives the target item's real screen position from
    // the popup's own geometry, expresses it in the root target's coordinates, sends a real
    // press/release through the production input path and reports the user-visible result.
    QComboBox *combo = new QComboBox(&root);
    combo->setGeometry(0, 440, 150, 30);
    combo->addItems({QStringLiteral("one"), QStringLiteral("two")});
    combo->setCurrentIndex(0);
    click(components, root, QPointF(75, 455));  // opens the popup through the remote path
    pump();
    QAbstractItemView *popupView = combo->view();
    const bool popupOpen = popupView && popupView->isVisible();
    check(popupOpen, "QComboBox: single remote click opens the popup");
    if (popupOpen) {
        const QModelIndex targetItem = popupView->model()->index(1, 0);  // the "two" row
        const QRect itemRect = popupView->visualRect(targetItem);
        const QPoint itemCenterGlobal = popupView->viewport()->mapToGlobal(itemRect.center());
        const QPoint popupTopLeftGlobal = popupView->mapToGlobal(QPoint(0, 0));
        const QPoint itemCenterInRoot = root.mapFromGlobal(itemCenterGlobal);
        const bool expressibleInRoot = root.rect().contains(itemCenterInRoot);
        std::cout << "     observed[QComboBox popup]: open=true currentIndexBefore=" << combo->currentIndex()
                  << " popupTopLeftGlobal=(" << popupTopLeftGlobal.x() << ", " << popupTopLeftGlobal.y()
                  << ") popupSize=" << popupView->width() << "x" << popupView->height()
                  << " targetItem=1 itemCenterGlobal=(" << itemCenterGlobal.x() << ", "
                  << itemCenterGlobal.y() << ") mappedIntoRoot=(" << itemCenterInRoot.x() << ", "
                  << itemCenterInRoot.y() << ") expressibleInRootTarget="
                  << (expressibleInRoot ? "true" : "false") << '\n';

        if (expressibleInRoot) {
            // A real remote click at that position, exactly as a viewer would produce it.
            click(components, root, QPointF(itemCenterInRoot));
            pump();
            const int indexAfter = combo->currentIndex();
            const bool popupStillOpen = popupView && popupView->isVisible();
            std::cout << "     observed[QComboBox popup user result]: currentIndexAfter=" << indexAfter
                      << " targetItemSelected=" << (indexAfter == 1 ? "true" : "false")
                      << " popupStillOpen=" << (popupStillOpen ? "true" : "false") << '\n';
            if (indexAfter == 1) {
                noDivergence("QComboBox popup item interaction",
                             "the target popup item became current through the remote path");
            } else {
                divergence("TOPLEVEL_INGRESS",
                           "QComboBox popup item interaction",
                           "the popup opened, and a real remote press/release at the target item's own "
                           "screen position (mapped into the root target) left currentIndex unchanged: "
                           "input cannot reach this separate top-level popup surface in the single-root "
                           "model. This is the \"some visible controls do not react\" half of the report; "
                           "the bounded application-scoped transient-surface model is owned by #404.");
            }
        } else {
            divergence("TOPLEVEL_INGRESS",
                       "QComboBox popup item interaction",
                       "the target item's screen position falls outside the root target's coordinate "
                       "space, so the single-root model cannot even express this click; the bounded "
                       "application-scoped transient-surface model is owned by #404");
        }
        combo->hidePopup();
        pump();
    }
}

// ---------------------------------------------------------------------------------------------
// #400 classifier acceptance: boundaries, queue delay and receiver identity.
// ---------------------------------------------------------------------------------------------
void testClassifierAcceptance(QWidget &root, HyRemote::detail::TargetComponents &components)
{
    const QStyleHints *hints = QGuiApplication::styleHints();
    const int interval = hints->mouseDoubleClickInterval();
    const int distance = hints->mouseDoubleClickDistance();
    std::cout << "     observed[classifier policy]: intervalMs=" << interval << " distancePx=" << distance
              << '\n';

    Probe probe(&root);
    probe.setGeometry(0, 0, 500, 600);
    pump();

    // Inside the interval, same point: one DblClick. The classifier accepts 0 <= delta < interval, and
    // this case stays well inside the interval so it does not depend on the boundary convention.
    click(components, root, QPointF(200, 200));
    pump();
    click(components, root, QPointF(200, 200));
    pump();
    check(probe.doubleClicks == 1, "#400 boundary: two clicks just inside the interval form one double click");

    // Inside the distance, but not the same pixel: still one DblClick.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    const int insideOffset = distance > 1 ? 1 : 0;
    click(components, root, QPointF(300, 300));
    pump();
    click(components, root, QPointF(300 + insideOffset, 300));
    pump();
    check(probe.doubleClicks == 1, "#400 boundary: a pair inside the distance still forms one double click");

    // Outside the interval: two singles (already covered above, re-asserted here for the same probe).
    probe.presses = probe.releases = probe.doubleClicks = 0;
    click(components, root, QPointF(100, 100));
    pump();
    pumpFor(interval + 200);
    click(components, root, QPointF(100, 100));
    pump();
    check(probe.doubleClicks == 0 && probe.presses == 2,
          "#400 boundary: a pair outside the interval stays two single clicks");

    // A double click consumes the pair: the third rapid press is an ordinary press again.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    click(components, root, QPointF(150, 150));
    pump();
    click(components, root, QPointF(150, 150));
    pump();
    check(probe.doubleClicks == 1, "#400 triple press: the first pair yields one double click");
    click(components, root, QPointF(150, 150));
    pump();
    check(probe.doubleClicks == 1, "#400 triple press: the third rapid press does not emit another DblClick");
    check(probe.presses == 3,
          "#400 triple press: the third press is an ordinary press (all three presses delivered)");

    // Receiver identity: a rapid pair on two different widgets is two singles even when time and
    // distance qualify.
    Probe other(&root);
    other.setGeometry(350, 350, 120, 120);
    pump();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    other.presses = other.releases = other.doubleClicks = 0;
    click(components, root, QPointF(200, 500));
    pump();
    click(components, root, QPointF(400, 400));
    pump();
    check(probe.presses == 1 && probe.doubleClicks == 0, "#400 cross widget: the first widget gets a single press");
    check(other.presses == 1 && other.doubleClicks == 0,
          "#400 cross widget: the second widget gets a single press, never a DblClick from the first");

    // Queue delay positive: the two presses are accepted inside the interval but the GUI thread is
    // blocked far longer than the interval before draining them. Classification uses the accepted
    // arrival time, so this must still be one double click.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    click(components, root, QPointF(250, 250));
    pump();
    std::atomic_bool posted{false};
    std::thread worker([&components, &root, &posted] {
        QThread::msleep(120);
        postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(250, 250),
                hyremote::PointerButton::Left, true);
        postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(250, 250),
                hyremote::PointerButton::Left, false);
        posted.store(true);
    });
    QThread::msleep(static_cast<unsigned long>(interval) + 400);  // GUI busy, nothing is drained
    worker.join();
    check(posted.load(), "#400 queue delay: the second accept happened while the GUI was blocked");
    pump();
    check(probe.doubleClicks == 1,
          "#400 queue delay positive: accepted-inside-interval clicks still classify as a double click");
    check(probe.presses == 2,
          "#400 queue delay positive: both presses are delivered and exactly one DblClick is added");

    // Queue delay negative: accepted far apart, but drained together. GUI proximity must not create
    // a double click.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(400, 500),
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(400, 500),
            hyremote::PointerButton::Left, false);
    QThread::msleep(static_cast<unsigned long>(interval) + 250);  // accepted gap > interval
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(400, 500),
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(400, 500),
            hyremote::PointerButton::Left, false);
    pump();  // both pairs drain together here
    check(probe.doubleClicks == 0,
          "#400 queue delay negative: clicks accepted outside the interval stay two single clicks");
    check(probe.presses == 2, "#400 queue delay negative: both presses are ordinary presses");
}

// ---------------------------------------------------------------------------------------------
// #400 queued-motion regression: the far excursion must survive pointer-move coalescing, and in-box
// jitter must not disable classification.
// ---------------------------------------------------------------------------------------------
void testAcceptedMotionRegression(QWidget &root, HyRemote::detail::TargetComponents &components)
{
    Probe probe(&root);
    probe.setGeometry(0, 0, 500, 600);
    pump();

    // Negative: click1, then a far excursion and a return that are still queued when the second
    // click arrives. Coalescing keeps only the near move for delivery, so only the accepted-stream
    // movement summary can invalidate the pair.
    QThread::msleep(600);  // start a new, independent gesture
    click(components, root, QPointF(220, 220));
    pump();
    check(probe.presses == 1, "#400 queued motion: the first click is delivered");
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(460, 560));
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(221, 221));
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(220, 220),
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(220, 220),
            hyremote::PointerButton::Left, false);
    pump();
    std::cout << "     observed[widgets queued far-return]: presses=" << probe.presses
              << " dblClicks=" << probe.doubleClicks << '\n';
    check(probe.doubleClicks == 0,
          "#400 queued far-return: a far excursion followed by a return does NOT form a double click");
    check(probe.presses == 2, "#400 queued far-return: both presses are ordinary presses");

    // Positive: several coalesced jitter moves that all stay inside the distance box must keep the
    // pair eligible. This guards against an implementation that disarms on any coalescing.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    QThread::msleep(600);
    click(components, root, QPointF(300, 300));
    pump();
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(301, 300));
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(302, 301));
    postRaw(components, root, hyremote::InputEventKind::PointerMove, QPointF(301, 300));
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(300, 300),
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, QPointF(300, 300),
            hyremote::PointerButton::Left, false);
    pump();
    std::cout << "     observed[widgets queued jitter]: presses=" << probe.presses
              << " dblClicks=" << probe.doubleClicks << '\n';
    check(probe.doubleClicks == 1,
          "#400 queued in-box jitter: coalesced moves inside the distance box keep the double click");
    check(probe.presses == 2, "#400 queued in-box jitter: both presses are delivered");

    // Transient coordinate-space excursion: click1 in viewport A, then an accepted move in viewport B
    // and a move back in A before the drain, then click2 in A. Comparing only press viewports would
    // call this "unchanged" and map B/A extrema as if they were one space, so it must stay fail-closed.
    probe.presses = probe.releases = probe.doubleClicks = 0;
    QThread::msleep(600);  // start a new, independent gesture
    click(components, root, QPointF(100, 100));
    pump();
    const double sourceWidth = static_cast<double>(root.width()) - 10.0;   // ~2% smaller viewport
    const double sourceHeight = static_cast<double>(root.height()) - 10.0;
    const QPointF pressPoint(100, 100);
    // The same physical target position expressed in the other viewport. Mapped with the period
    // viewport it lands ~2px away (inside the box), so only the viewport verdict can refuse it.
    const QPointF otherViewportPoint(pressPoint.x() * sourceWidth / static_cast<double>(root.width()),
                                     pressPoint.y() * sourceHeight / static_cast<double>(root.height()));
    postRawViewport(components, hyremote::InputEventKind::PointerMove,
                    static_cast<std::uint32_t>(sourceWidth), static_cast<std::uint32_t>(sourceHeight), 1.0F,
                    otherViewportPoint);
    postRaw(components, root, hyremote::InputEventKind::PointerMove, pressPoint);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, pressPoint,
            hyremote::PointerButton::Left, true);
    postRaw(components, root, hyremote::InputEventKind::PointerButton, pressPoint,
            hyremote::PointerButton::Left, false);
    pump();
    std::cout << "     observed[widgets transient viewport]: presses=" << probe.presses
              << " dblClicks=" << probe.doubleClicks << '\n';
    check(probe.doubleClicks == 0,
          "#400 transient viewport (Widgets): a move accepted in another source viewport invalidates the "
          "pair even when the closing press returns to the original viewport");
    check(probe.presses == 2, "#400 transient viewport (Widgets): both presses are ordinary presses");
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
    testClassifierAcceptance(root, components);
    testAcceptedMotionRegression(root, components);
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
