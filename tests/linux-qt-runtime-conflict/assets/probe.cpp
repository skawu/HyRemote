// A same-SONAME conflict on a library that is not the Qt runtime. HyRemote must refuse it instead of
// choosing a non-Qt library on the caller's behalf.
extern "C" int hyremote_probe()
{
    return 0x42;
}
