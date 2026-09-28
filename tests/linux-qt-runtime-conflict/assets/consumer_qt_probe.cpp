// A consumer that additionally NEEDs a non-Qt library present in two foreign roots, so the non-Qt conflict
// case is produced by the dependency graph rather than by editing the fixture's expectations.
extern "C" int hyremote_qt_payload();
extern "C" int hyremote_probe();

extern "C" int hyremote_consumer_uses_qt()
{
    return hyremote_qt_payload();
}

extern "C" int hyremote_consumer_uses_probe()
{
    return hyremote_probe();
}
