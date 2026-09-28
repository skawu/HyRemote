// Occupies a selected root that deliberately does not provide the Qt runtime, so the "no candidate from the
// selected root" case has a real directory to point at instead of a path that does not exist.
extern "C" int hyremote_placeholder()
{
    return 0x1;
}
