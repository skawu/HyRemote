use rustvncserver::server::ServerEvent;
use rustvncserver::VncServer;
use std::collections::VecDeque;
use std::ffi::c_void;
use std::slice;
use std::sync::{Arc, Mutex};
use tokio::runtime::{Builder, Runtime};
use tokio::task::JoinHandle;

const ABI_VERSION: u32 = 1;

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
    listener: Mutex<Option<JoinHandle<()>>>,
    events: Arc<Mutex<VecDeque<HyRemoteVncProbeEvent>>>,
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
            pressed: u32::from(down),
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
        "HyRemote rustvncserver FFI probe".to_string(),
        None,
    );
    let server = Arc::new(server);
    let events = Arc::new(Mutex::new(VecDeque::new()));
    let event_queue = Arc::clone(&events);

    runtime.spawn(async move {
        while let Some(event) = event_rx.recv().await {
            if let Some(mapped) = map_event(event) {
                if let Ok(mut queue) = event_queue.lock() {
                    queue.push_back(mapped);
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
pub unsafe extern "C" fn hyremote_vnc_probe_start(handle: *mut c_void, port: u16) -> i32 {
    let Some(probe) = probe_from_handle(handle) else {
        return -1;
    };

    let mut listener = match probe.listener.lock() {
        Ok(listener) => listener,
        Err(_) => return -2,
    };

    if listener.is_some() {
        return 1;
    }

    let server = Arc::clone(&probe.server);
    *listener = Some(probe.runtime.spawn(async move {
        let _ = server.listen(port).await;
    }));
    0
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
