"""Give the showcase a scriptable policy entry: --toggle-input-at-ms <ms,ms,...>.

Why: section 5's E5 row claims a safe initial policy and explicit control enablement, and the release input cannot
exercise either - it launches with `--remote-input` and never toggles. The interactive `Ctrl+I` entry cannot serve a
headless CI run, so the payload needs a non-interactive one. Timings are given as a comma-separated millisecond list
from start; each entry toggles the same checkbox the user clicks, so the policy path is unchanged.
"""
import sys
from pathlib import Path

ROOT = Path(".").resolve()
APPLY = "--apply" in sys.argv
MAIN = ROOT / "examples" / "remote-support-showcase" / "main.cpp"
README = ROOT / "examples" / "remote-support-showcase" / "README.md"

OPTION_ANCHOR = """    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
"""
OPTION_INSERT = OPTION_ANCHOR + """    QCommandLineOption toggleInputOption(
        QStringLiteral("toggle-input-at-ms"),
        QStringLiteral("Toggle remote input at the given times (comma-separated milliseconds from start). "
                       "Deterministic entry for the physical acceptance runbook and the product-fit harness."),
        QStringLiteral("milliseconds"));
"""

ADD_OPTION_ANCHOR = """    parser.addOption(secondsOption);
"""
ADD_OPTION_INSERT = ADD_OPTION_ANCHOR + """    parser.addOption(toggleInputOption);
"""

SCHEDULE_ANCHOR = """    window.show();
"""
SCHEDULE_INSERT = SCHEDULE_ANCHOR + """
    for (const QString &token : parser.value(toggleInputOption).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int at = token.trimmed().toInt(&ok);
        if (!ok || at <= 0)
            continue;
        QTimer::singleShot(at, &window, [&window] { window.toggleRemoteInputPolicy(); });
    }
"""

PUBLIC_ANCHOR = """    bool startRemoteAccess()
"""
PUBLIC_INSERT = """    // Deterministic policy entry shared by Ctrl+I, the physical runbook and the headless product-fit harness.
    void toggleRemoteInputPolicy()
    {
        m_input->setChecked(!m_input->isChecked());
    }

    bool startRemoteAccess()
"""

README_LINE = ("\n- **`--toggle-input-at-ms <ms,ms,...>`** toggles remote control automatically at the given millisecond\n"
               "  offsets from start (for example `--toggle-input-at-ms 8000,20000`). It drives the same checkbox as\n"
               "  **Ctrl+I**, so scripted and headless runs exercise the same policy path as a person clicking it.\n")


def main() -> int:
    print(("APPLYING" if APPLY else "DRY RUN") + ": scriptable policy entry for the showcase")
    raw = MAIN.read_bytes().decode("utf-8")
    newline = "\r\n" if "\r\n" in raw else "\n"
    text = raw.replace("\r\n", "\n")
    if "toggle-input-at-ms" in text:
        print("  main.cpp: option already present")
    else:
        for anchor, insert, what in ((OPTION_ANCHOR, OPTION_INSERT, "option declaration"),
                                     (ADD_OPTION_ANCHOR, ADD_OPTION_INSERT, "parser registration"),
                                     (SCHEDULE_ANCHOR, SCHEDULE_INSERT, "shot schedule"),
                                     (PUBLIC_ANCHOR, PUBLIC_INSERT, "public toggle method")):
            if text.count(anchor) != 1:
                print(f"!! main.cpp: anchor for {what} not unique/found ({text.count(anchor)})")
                return 1
            text = text.replace(anchor, insert, 1)
        if APPLY:
            MAIN.write_bytes(text.replace("\n", newline).encode("utf-8"))
        print("  main.cpp: --toggle-input-at-ms wired to the checkbox through a public toggle method")

    rraw = README.read_bytes().decode("utf-8")
    rnewline = "\r\n" if "\r\n" in rraw else "\n"
    rtext = rraw.replace("\r\n", "\n")
    if "toggle-input-at-ms" in rtext:
        print("  README.md: option already documented")
    else:
        if APPLY:
            README.write_bytes((rtext.rstrip("\n") + "\n" + README_LINE).replace("\n", rnewline).encode("utf-8"))
        print("  README.md: option documented")
    if not APPLY:
        print("\nre-run with --apply")
    return 0


if __name__ == "__main__":
    sys.exit(main())
