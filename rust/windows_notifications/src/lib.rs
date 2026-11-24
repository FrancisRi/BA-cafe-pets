use std::{
    collections::HashSet,
    ffi::c_void,
    time::{SystemTime, UNIX_EPOCH},
};
use windows::{
    UI::Notifications::{
        KnownNotificationBindings,
        Management::{UserNotificationListener, UserNotificationListenerAccessStatus as Access},
        NotificationKinds, UserNotification,
    },
    Win32::System::WinRT::{RO_INIT_SINGLETHREADED, RoInitialize, RoUninitialize},
    core::Result,
};
use windows_collections::IVectorView;
use windows_future::{AsyncStatus, IAsyncOperation};

struct Apartment;

impl Apartment {
    fn init() -> Result<Self> {
        unsafe { RoInitialize(RO_INIT_SINGLETHREADED)? };
        Ok(Self)
    }
}

impl Drop for Apartment {
    fn drop(&mut self) {
        unsafe { RoUninitialize() }
    }
}

type Snapshot = IAsyncOperation<IVectorView<UserNotification>>;
type Receive = extern "C" fn(*mut c_void, *const u16, usize, *const u16, usize);

pub struct Listener {
    service: UserNotificationListener,
    request: Option<IAsyncOperation<Access>>,
    snapshot: Option<Snapshot>,
    seen: HashSet<(i64, u32)>,
    since: i64,
    mode: i32,
    _apartment: Apartment,
}

fn now() -> i64 {
    const WINDOWS_EPOCH: i64 = 116_444_736_000_000_000;
    WINDOWS_EPOCH
        + (SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos()
            / 100) as i64
}

impl Listener {
    fn new() -> Result<Box<Self>> {
        let apartment = Apartment::init()?;
        let service = UserNotificationListener::Current()?;
        service.GetAccessStatus()?;
        Ok(Box::new(Self {
            service,
            request: None,
            snapshot: None,
            seen: HashSet::new(),
            since: now(),
            mode: 0,
            _apartment: apartment,
        }))
    }

    fn poll(&mut self, mode: i32, receive: Receive, context: *mut c_void) -> Result<i32> {
        if mode != self.mode {
            self.mode = mode;
            self.snapshot = None;
            self.seen.clear();
            self.since = now();
        }
        if let Some(request) = &self.request {
            if request.Status()? == AsyncStatus::Started {
                return Ok(3);
            }
            self.request = None;
        }
        let access = self.service.GetAccessStatus()?;
        if access != Access::Allowed {
            self.snapshot = None;
            self.seen.clear();
            self.since = now();
            return Ok(if access == Access::Denied { 2 } else { 1 });
        }
        if mode == 0 {
            return Ok(4);
        }
        if let Some(snapshot) = &self.snapshot {
            if snapshot.Status()? == AsyncStatus::Started {
                return Ok(4);
            }
            let snapshot = self.snapshot.take().unwrap();
            let entries = snapshot.GetResults()?;
            let mut latest = HashSet::new();
            let mut fresh = Vec::new();
            for item in &entries {
                if let (Ok(time), Ok(id)) = (item.CreationTime(), item.Id()) {
                    let key = (time.UniversalTime, id);
                    if key.0 >= self.since && latest.insert(key) && !self.seen.contains(&key) {
                        fresh.push((key, item));
                    }
                }
            }
            self.seen = latest;
            fresh.sort_by_key(|(key, _)| *key);
            for (_, item) in fresh {
                let _ = forward(&item, mode, receive, context);
            }
        }
        self.snapshot = Some(
            self.service
                .GetNotificationsAsync(NotificationKinds::Toast)?,
        );
        Ok(4)
    }
}

fn forward(
    item: &UserNotification,
    mode: i32,
    receive: Receive,
    context: *mut c_void,
) -> Result<()> {
    let app = item.AppInfo()?;
    let name = app.DisplayInfo()?.DisplayName()?.to_string_lossy();
    let id = app.AppUserModelId()?.to_string_lossy().to_ascii_lowercase();
    let codex = id.starts_with("openai.codex_")
        || id.eq_ignore_ascii_case("openai.codex")
        || name.eq_ignore_ascii_case("codex");
    if mode == 1 && !codex {
        return Ok(());
    }
    let binding = item
        .Notification()?
        .Visual()?
        .GetBinding(&KnownNotificationBindings::ToastGeneric()?)?;
    let lines = binding.GetTextElements()?;
    let mut text = (&lines)
        .into_iter()
        .filter_map(|line| line.Text().ok())
        .map(|s| s.to_string_lossy())
        .filter(|s| !s.trim().is_empty());
    let Some(first) = text.next() else {
        return Ok(());
    };
    let mut body = first;
    for line in text.take(5) {
        body.push('\n');
        body.push_str(&line);
    }
    let title: Vec<u16> = (if codex { "Codex" } else { &name })
        .encode_utf16()
        .take(180)
        .collect();
    let body: Vec<u16> = body.encode_utf16().take(2048).collect();
    receive(
        context,
        title.as_ptr(),
        title.len(),
        body.as_ptr(),
        body.len(),
    );
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_notifications_open() -> *mut Listener {
    Listener::new().map_or(std::ptr::null_mut(), Box::into_raw)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ba_notifications_close(listener: *mut Listener) {
    drop(unsafe { Box::from_raw(listener) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_notifications_request(listener: &mut Listener) -> i32 {
    if listener.request.is_some() {
        return 3;
    }
    match listener.service.RequestAccessAsync() {
        Ok(request) => {
            listener.request = Some(request);
            3
        }
        Err(_) => 5,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ba_notifications_poll(
    listener: &mut Listener,
    mode: i32,
    receive: Receive,
    context: *mut c_void,
) -> i32 {
    listener.poll(mode, receive, context).unwrap_or(5)
}
