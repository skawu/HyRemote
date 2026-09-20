## Why

A product review of the consumer surface found the C++ facade had **no change notification at all**: `state()`,
`connectedClientCount()` and `lastError()` had to be polled. The shipped code polls accordingly - the QML wrapper on
a 100 ms timer (`QmlRemoteAccess.cpp:88-90`), the widgets example on another - and the QML wrapper could at least
expose signals on top of that polling. A C++ consumer could not.

## Shape, and why it is not `QObject`

`RemoteAccess` stays a plain, movable value type. `docs/v1-api-stability.md:68-70` freezes that ("remains
non-copyable and **movable**... must not... remove move support"), and a `QObject` base class is not movable - so the
notifications live on a separate object:

```cpp
QObject::connect(remote.notifier(), &HyRemote::RemoteAccessNotifier::clientCountChanged, &window, [&] { ... });
```

`RemoteAccessNotifier` is added to the stable surface in `docs/v1-api-stability.md`, together with a sentence saying
1.x must not fold it into the facade, for the reason above.

## What each signal actually reports

| Signal | Source | Not a poll because |
| --- | --- | --- |
| `clientCountChanged()` | the transport's own `ClientConnected`/`ClientDisconnected` events | the decorator already observes them |
| `stateChanged()` | facade transitions plus everything Core reports | Core pushes the asynchronous ones |
| `errorChanged()` | every write or clear of the diagnostic behind `lastError()` | including a recurrence of the same error |

`Session::setChangeCallback()` is the enabler. It is deliberately minimal: only changes the owner **cannot** infer by
driving the Session itself are notified, because everything else happens inside a call the owner made. It hands the
new state and diagnostic to the callback rather than letting observers query them - see below.

## Two defects found by running, not reading

- **Deadlock.** The first revision had the facade query the Session from inside Core's callback, which runs with
  Core's state lock held; every getter takes that non-recursive lock. Three facade tests hung until their timeouts
  exposed it. Fixed by passing the values, and the callback paths now never touch the Session.
- **Stale values in a slot.** The connection decorator used to count and then forward the event, so a signal emitted
  from there could let an observer read pre-event state. It forwards first now.

## Evidence

- Full suite **50/50** and **10/10** release gates.
- New `testNotifierReportsRealChanges`: a signal for every real change (a rejected configuration, `clearError()`,
  start, viewer connect, viewer disconnect, stop) and **no** signal when nothing changed.
- Two of that case's expectations were wrong at first, and measurement said so rather than code review: an earlier
  publication had already reported the client count, and `Running -> Stopping -> Stopped` is genuinely two
  notifications. Both are now asserted as they are, with the reason in the test.
- `AUTOMOC` was needed on the library target **and** the public header had to be listed in its sources - without the
  second half, moc never saw the header and the link failed with `undefined reference to ...::stateChanged()` and a
  missing vtable.

## Follow-up, not in this PR

The QML wrapper can now drop its 100 ms polling timer and forward the notifier's signals instead. That is a separate
change against `QmlRemoteAccess`, and it is worth doing only after this API is accepted.
