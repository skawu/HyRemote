// Foreign same-SONAME payload. It must never be deployed beside the selected lineage.
extern "C" int hyremote_qt_payload()
{
    return 0xBBBB;
}
