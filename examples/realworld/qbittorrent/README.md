# qBittorrent real-world verification

**Role:** Qt Widgets / Qt 6.8.3 representative.

This example replays the proven #137 integration against the final V1 candidate without modifying qBittorrent source.

## Frozen upstream input

- repository: `https://github.com/qbittorrent/qBittorrent.git`
- revision: `9b0e9311f3e8aa511e46605d7e1a156e101adac9`
- Qt: **6.8.3 exact** for the V1 QPA lane
- integration: `-platform hyremote` with QPA deployed externally

#137 already demonstrated Windows/Linux build/install plus real RFB framebuffer/reconnect on this class of integration. The final V1 run must replay the bounded verification against the final candidate rather than treating the historical green run as final-release evidence.

## Contract

The qBittorrent checkout is pristine. HyRemote must not be included or linked by qBittorrent application source. Deployment/wrapping belongs to HyRemote-owned harness code outside the upstream checkout.

Keep qBittorrent's own application name, icons and branding unchanged.

Minimum final evidence:

1. verify exact upstream revision and clean working tree;
2. build with the recorded Qt/toolchain prerequisites;
3. install the application into an isolated prefix;
4. deploy the exact-Qt `qhyremote` payload and shared HyRemote runtime without patching application source;
5. launch with `-platform hyremote`;
6. complete RFB 3.8 framebuffer/reconnect and applicable remote-input checks;
7. verify the upstream checkout remains byte-for-byte clean after the run.

**This is a third-party verification example, not a HyRemote support claim for qBittorrent.**

Refs: #41 #57 #109 #134 #137 #176 #209.
