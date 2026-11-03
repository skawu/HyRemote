# RC5 `dfbc6cda` local G6 gate — evidence

`RC5_SHA=dfbc6cda9a6d407fdc496d2f2e7dca0c15ab2637` · `RC5_TREE=c1506bf1184bfe3b66f611474831c4c89003bc7e`

| File | Content |
| --- | --- |
| `first-crash-evidence.txt` | The 0xC0000409 crash signature, its deterministic reproduction, the runner PATH rule that causes it, and the four observed runs |
| `install-prefix-platform-plugins.txt` | `build/install/plugins/platforms` content: `qhyremote.dll`, `qwindows.dll` — no `qoffscreen.dll` |

Summary: the canonical runner prepends every directory in the build tree that holds
`HyRemoteRemoteAccess.dll` to the test `PATH`; after `build.cmd install` that includes
`build/install/bin`, whose sibling Qt deployment carries no offscreen platform plugin. All 19
`QT_QPA_PLATFORM=offscreen` tests then abort with `0xC0000409`. With the install directory absent from the build
tree the same unchanged command runs **79/79 in 479.27 s**. This evidence folder belongs to an unmerged
evidence branch.
