use rustvncserver::server::ServerEvent;
use rustvncserver::VncServer;
use std::collections::VecDeque;
use std::ffi::c_void;
use std::io::Read;
use std::net::{Ipv4Addr, SocketAddr, TcpStream};
use std::slice;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};
use tokio::runtime::{Builder, Runtime};
use tokio::task::JoinHandle;

const ABI_VERSION: u32 = 2;
const LOCAL_EVENT_CAPACITY: usize = 256;

const EVENT_CLIENT_CONNECTED: u32 = 1;
const EVENT_CLIENT_DISCONNECTED: u32 = 2;
const EVENT_KEY: u32 = 3;
const EVENT_POINTER: u32 = 4;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct HyRemoteVncProbeEvent {
    pub kind: u32,
    pub client_id: u64,
    pub x: u32,
    pub y: u32,
    pub button_mask: u32,
    pub key: u32,
    pub pressed: u32,
}

struct Probe {
    runtime: Runtime,
    server: Arc<VncServer>,
    listener: Mutex<Option<JoinHandle<Result<(), std::io::Error>>>>,
    events: Arc<Mutex<VecDeque<HyRemoteVncProbeEvent>>>,
    dropped_events: Arc<AtomicU64>,
    width: u16,
    height: u16,
}

fn map_event(event: ServerEvent) -> Option<HyRemoteVncProbeEvent> {
    match event {
        ServerEvent::ClientConnected { client_id } => Some(HyRemoteVncProbeEvent {
            kind: EVENT_CLIENT_CONNECTED,
            client_id: client_id as u64,
            ..Default::default()
        }),
        ServerEvent::ClientDisconnected { client_id } => Some(HyRemoteVncProbeEvent {
            kind: EVENT_CLIENT_DISCONNECTED,
            client_id: client_id as u64,
            ..Default::default()
        }),
        ServerEvent::KeyPress {
            client_id,
            down,
            key,
        } => Some(HyRemoteVncProbeEvent {
            kind: EVENT_KEY,
            client_id: client_id as u64,
            key,
            pressed: if down { 1 } else { 0 },
            ..Default::default()
        }),
        ServerEvent::PointerMove {
            client_id,
            x,
            y,
            button_mask,
        } => Some(HyRemoteVncProbeEvent {
            kind: EVENT_POINTER,
            client_id: client_id as u64,
            x: u32::from(x),
            y: u32::from(y),
            button_mask: u32::from(button_mask),
            ..Default::default()
        }),
        ServerEvent::CutText { .. }
        | ServerEvent::RfbMessageSent { .. }
        | ServerEvent::HandshakeComplete { .. } => None,
    }
}

fn push_bounded_event(
    queue: &mut VecDeque<HyRemoteVncProbeEvent>,
    event: HyRemoteVncProbeEvent,
    dropped: &AtomicU64,
) {
    // Pointer motion is freshness-oriented. Replace the newest queued pointer event from the same
    // client rather than growing the boundary queue. Key/button/connect lifecycle events remain
    // ordered; when the fixed queue is full the oldest event is dropped and counted explicitly.
    if event.kind == EVENT_POINTER {
        if let Some(existing) = queue
            .iter_mut()
            .rev()
            .find(|queued| queued.kind == EVENT_POINTER && queued.client_id == event.client_id)
        {
            *existing = event;
            return;
        }
    }

    if queue.len() >= LOCAL_EVENT_CAPACITY {
        queue.pop_front();
        dropped.fetch_add(1, Ordering::Relaxed);
    }
    queue.push_back(event);
}

unsafe fn probe_from_handle<'a>(handle: *mut c_void) -> Option<&'a Probe> {
    (handle as *mut Probe).as_ref()
}

fn stop_probe(probe: &Probe) -> i32 {
    let listener = match probe.listener.lock() {
        Ok(mut listener) => listener.take(),
        Err(_) => return -2,
    };

    probe.runtime.block_on(async {
        if let Some(task) = listener {
            task.abort();
            let _ = task.await;
        }
        probe.server.disconnect_all_clients().await;
    });

    0
}

fn listener_failed(probe: &Probe) -> Option<i32> {
    let mut listener = probe.listener.lock().ok()?;
    let task = listener.as_ref()?;
    if !task.is_finished() {
        return None;
    }

    let task = listener.take()?;
    drop(listener);
    let result = probe.runtime.block_on(task);
    Some(match result {
        Ok(Err(_)) => -5, // bind/accept path returned an I/O error
        Ok(Ok(())) => -6, // unexpected clean exit from an infinite listener
        Err(_) => -7,     // task cancelled/panicked
    })
}

fn speaks_rfb(address: &SocketAddr) -> bool {
    let Ok(mut stream) = TcpStream::connect_timeout(address, Duration::from_millis(50)) else {
        return false;
    };
    let _ = stream.set_read_timeout(Some(Duration::from_millis(100)));
    let mut banner = [0_u8; 12];
    stream.read_exact(&mut banner).is_ok() && banner.starts_with(b"RFB ")
}

#[no_mangle]
pub extern "C" fn hyremote_vnc_probe_abi_version() -> u32 {
    ABI_VERSION
}

#[no_mangle]
pub extern "C" fn hyremote_vnc_probe_create(width: u16, height: u16) -> *mut c_void {
    if width == 0 || height == 0 {
        return std::ptr::null_mut();
    }

    let runtime = match Builder::new_multi_thread()
        .worker_threads(2)
        .enable_all()
        .build()
    {
        Ok(runtime) => runtime,
        Err(_) => return std::ptr::null_mut(),
    };

    let (server, mut event_rx) = VncServer::new(
        width,
        height,
        "HyRemote rustvncserver product-fit probe".to_string(),
        None,
    );
    let server = Arc::new(server);
    let events = Arc::new(Mutex::new(VecDeque::with_capacity(LOCAL_EVENT_CAPACITY)));
    let dropped_events = Arc::new(AtomicU64::new(0));
    let event_queue = Arc::clone(&events);
    let event_drops = Arc::clone(&dropped_events);

    runtime.spawn(async move {
        while let Some(event) = event_rx.recv().await {
            if let Some(mapped) = map_event(event) {
                if let Ok(mut queue) = event_queue.lock() {
                    push_bounded_event(&mut queue, mapped, &event_drops);
                } else {
                    break;
                }
            }
        }
    });

    let probe = Probe {
        runtime,
        server,
        listener: Mutex::new(None),
        events,
        dropped_events,
        width,
        height,
    };

    Box::into_raw(Box::new(probe)).cast::<c_void>()
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_update_rgba(
    handle: *mut c_void,
    data: *const u8,
    len: usize,
) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return -1;
    };
    if data.is_null() {
        return -1;
    }

    let expected = usize::from(probe.width) * usize::from(probe.height) * 4;
    if len != expected {
        return -3;
    }

    let bytes = slice::from_raw_parts(data, len);
    match probe
        .runtime
        .block_on(probe.server.framebuffer().update_from_slice(bytes))
    {
        Ok(()) => 0,
        Err(_) => -4,
    }
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_start_ipv4(
    handle: *mut c_void,
    a: u8,
    b: u8,
    c: u8,
    d: u8,
    port: u16,
) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return -1;
    };
    if port == 0 {
        // PR #29 does not expose the bound local address, so an ephemeral port cannot be reported
        // deterministically through this API. Product code must not guess it.
        return -8;
    }

    let address = SocketAddr::new(Ipv4Addr::new(a, b, c, d).into(), port);
    {
        let mut listener = match probe.listener.lock() {
            Ok(listener) => listener,
            Err(_) => return -2,
        };
        if listener.is_some() {
            return 1;
        }

        let server = Arc::clone(&probe.server);
        *listener = Some(probe.runtime.spawn(async move { server.listen_on(address).await }));
    }

    // The candidate API binds inside the spawned future. Do not report success because an arbitrary
    // service accepts the port: the startup probe must observe an actual RFB protocol banner from
    // the requested endpoint. This also makes the occupied-port test meaningful. A pre-bound
    // listener/explicit ready result remains the preferred production API and is recorded as an
    // upstream gap rather than hidden by this probe.
    let deadline = Instant::now() + Duration::from_secs(2);
    while Instant::now() < deadline {
        if let Some(code) = listener_failed(probe) {
            return code;
        }

        if speaks_rfb(&address) {
            return 0;
        }
        thread::sleep(Duration::from_millis(10));
    }

    let _ = stop_probe(probe);
    -9
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_start(handle: *mut c_void, port: u16) -> i32 {
    // HyRemote's safe default is loopback. The wildcard behavior of upstream v2.2.1 is never used
    // by this product-fit probe.
    hyremote_vnc_probe_start_ipv4(handle, 127, 0, 0, 1, port)
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_running(handle: *mut c_void) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return 0;
    };

    match probe.listener.lock() {
        Ok(listener) => match listener.as_ref() {
            Some(task) if !task.is_finished() => 1,
            _ => 0,
        },
        Err(_) => 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_poll_event(
    handle: *mut c_void,
    out_event: *mut HyRemoteVncProbeEvent,
) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return -1;
    };
    if out_event.is_null() {
        return -1;
    }

    let mut queue = match probe.events.lock() {
        Ok(queue) => queue,
        Err(_) => return -2,
    };

    let Some(event) = queue.pop_front() else {
        return 0;
    };

    out_event.write(event);
    1
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_dropped_events(handle: *mut c_void) -> u64 {
    let Some(probe) = probe_from_handle(handle) else {
        return 0;
    };
    probe.dropped_events.load(Ordering::Relaxed)
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_stop(handle: *mut c_void) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return -1;
    };
    stop_probe(probe)
}

#[no_mangle]
pub unsafe extern "C" fn hyremote_vnc_probe_destroy(handle: *mut c_void) {
    if handle.is_null() {
        return;
    }

    let probe = Box::from_raw(handle as *mut Probe);
    let _ = stop_probe(&probe);
    drop(probe);
}
