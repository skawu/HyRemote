// Selected consumer Qt runtime root payload. The return value is a distinguishable marker so a case can
// prove which root's bytes were deployed instead of only proving that some library was.
extern "C" int hyremote_qt_payload()
{
    return 0xAAAA;
}
