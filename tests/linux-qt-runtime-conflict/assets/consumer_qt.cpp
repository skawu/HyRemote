// The deployed application's own runtime dependency source. It NEEDs the Qt SONAME and nothing else, so the
// only conflict a case can produce from it is the Qt runtime conflict itself.
extern "C" int hyremote_qt_payload();

extern "C" int hyremote_consumer_uses_qt()
{
    return hyremote_qt_payload();
}
