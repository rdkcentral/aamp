//! Narrow C ABI over moq-dev/moq for the native MoQ player.
//!
//! The shim owns a Tokio runtime on its own thread, one moq-tokio connection
//! (WebTransport or raw QUIC, reconnecting), and one broadcast consumer for the
//! configured namespace. Each subscription reads its groups concurrently and
//! hands every Object (moq-net frame) to the C callback with its Group ID and
//! Object index. See include/moq_shim.h for the contract.

#![allow(non_camel_case_types, reason = "C ABI types mirror include/moq_shim.h")]

use std::collections::HashMap;
use std::ffi::{CStr, CString, c_char, c_void};
use std::sync::Arc;
use std::time::Duration;

use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

const MOQ_REV: &str = "b3de94d";

const EVENT_CONNECTING: i32 = 1;
const EVENT_CONNECTED: i32 = 2;
const EVENT_BROADCAST_READY: i32 = 3;
const EVENT_SUBSCRIBED: i32 = 4;
const EVENT_GROUP_BEGIN: i32 = 5;
const EVENT_GROUP_END: i32 = 6;
const EVENT_SUBSCRIPTION_ENDED: i32 = 7;
const EVENT_ERROR: i32 = 8;
const EVENT_CLOSED: i32 = 9;

#[repr(C)]
pub struct moqshim_event {
	kind: i32,
	sub_id: u64,
	group: u64,
	object: u64,
	detail: *const c_char,
}

#[repr(C)]
pub struct moqshim_object {
	sub_id: u64,
	group: u64,
	object: u64,
	data: *const u8,
	size: usize,
}

type EventCb = Option<extern "C" fn(*mut c_void, *const moqshim_event)>;
type ObjectCb = Option<extern "C" fn(*mut c_void, *const moqshim_object)>;

#[repr(C)]
pub struct moqshim_config {
	url: *const c_char,
	version: *const c_char,
	ns: *const c_char,
	log_filter: *const c_char,
	on_event: EventCb,
	on_object: ObjectCb,
	ctx: *mut c_void,
}

#[repr(C)]
pub struct moqshim_subscribe {
	track: *const c_char,
	sub_id: u64,
	priority: i8,
	max_delay_ms: u32,
	live_join: i32,
}

/// The caller's callbacks. The caller guarantees `ctx` outlives the session and
/// that the callbacks are thread-safe, so sharing them across tasks is sound.
struct Sink {
	on_event: EventCb,
	on_object: ObjectCb,
	ctx: *mut c_void,
}

unsafe impl Send for Sink {}
unsafe impl Sync for Sink {}

impl Sink {
	fn event(&self, kind: i32, sub_id: u64, group: u64, object: u64, detail: Option<&str>) {
		let Some(cb) = self.on_event else { return };
		let detail = detail.and_then(|text| CString::new(text.replace('\0', " ")).ok());
		let event = moqshim_event {
			kind,
			sub_id,
			group,
			object,
			detail: detail.as_ref().map_or(std::ptr::null(), |text| text.as_ptr()),
		};
		cb(self.ctx, &event);
	}

	fn object(&self, sub_id: u64, group: u64, object: u64, data: &[u8]) {
		let Some(cb) = self.on_object else { return };
		let object = moqshim_object {
			sub_id,
			group,
			object,
			data: data.as_ptr(),
			size: data.len(),
		};
		cb(self.ctx, &object);
	}
}

struct SubscribeRequest {
	track: String,
	sub_id: u64,
	priority: i8,
	max_delay: Duration,
	live_join: bool,
}

enum Command {
	Subscribe(SubscribeRequest),
	Unsubscribe(u64),
	Close,
}

pub struct moqshim_session {
	commands: mpsc::UnboundedSender<Command>,
	thread: Option<std::thread::JoinHandle<()>>,
}

struct Settings {
	url: url::Url,
	version: Option<moq_net::Version>,
	namespace: String,
}

unsafe fn text(ptr: *const c_char) -> Option<String> {
	if ptr.is_null() {
		return None;
	}
	unsafe { CStr::from_ptr(ptr) }.to_str().ok().map(str::to_owned)
}

#[unsafe(no_mangle)]
pub extern "C" fn moqshim_implementation() -> *const c_char {
	static IMPLEMENTATION: std::sync::OnceLock<CString> = std::sync::OnceLock::new();
	IMPLEMENTATION
		.get_or_init(|| CString::new(format!("{}@{MOQ_REV}", moq_net::IMPLEMENTATION)).unwrap_or_default())
		.as_ptr()
}

/// # Safety
/// `config` must point to a valid `moqshim_config` whose strings are NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn moqshim_session_open(config: *const moqshim_config) -> *mut moqshim_session {
	let Some(config) = (unsafe { config.as_ref() }) else {
		return std::ptr::null_mut();
	};
	let (Some(url), Some(namespace)) = (unsafe { text(config.url) }, unsafe { text(config.ns) }) else {
		return std::ptr::null_mut();
	};
	let Ok(url) = url::Url::parse(&url) else {
		return std::ptr::null_mut();
	};
	let version = match unsafe { text(config.version) } {
		Some(version) => match version.parse::<moq_net::Version>() {
			Ok(version) => Some(version),
			Err(_) => return std::ptr::null_mut(),
		},
		None => None,
	};
	if let Some(filter) = unsafe { text(config.log_filter) } {
		let _ = tracing_subscriber::fmt()
			.with_env_filter(tracing_subscriber::EnvFilter::new(filter))
			.with_writer(std::io::stderr)
			.with_ansi(false)
			.try_init();
	}

	let sink = Arc::new(Sink {
		on_event: config.on_event,
		on_object: config.on_object,
		ctx: config.ctx,
	});
	let settings = Settings { url, version, namespace };
	let (commands, receiver) = mpsc::unbounded_channel();

	let thread = std::thread::Builder::new().name("moq-shim".into()).spawn(move || {
		let runtime = match tokio::runtime::Builder::new_multi_thread()
			.worker_threads(2)
			.thread_name("moq-shim-rt")
			.enable_all()
			.build()
		{
			Ok(runtime) => runtime,
			Err(err) => {
				sink.event(EVENT_CLOSED, 0, 0, 0, Some(&format!("runtime: {err}")));
				return;
			}
		};
		let result = runtime.block_on(run(settings, sink.clone(), receiver));
		runtime.shutdown_timeout(Duration::from_secs(2));
		let detail = result.err();
		if let Some(err) = &detail {
			sink.event(EVENT_ERROR, 0, 0, 0, Some(err));
		}
		sink.event(EVENT_CLOSED, 0, 0, 0, detail.as_deref());
	});

	match thread {
		Ok(thread) => Box::into_raw(Box::new(moqshim_session {
			commands,
			thread: Some(thread),
		})),
		Err(_) => std::ptr::null_mut(),
	}
}

/// # Safety
/// `session` must come from `moqshim_session_open`; `request` must be valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn moqshim_subscribe_track(session: *mut moqshim_session, request: *const moqshim_subscribe) -> i32 {
	let (Some(session), Some(request)) = (unsafe { session.as_ref() }, unsafe { request.as_ref() }) else {
		return -1;
	};
	let Some(track) = (unsafe { text(request.track) }) else {
		return -1;
	};
	let command = Command::Subscribe(SubscribeRequest {
		track,
		sub_id: request.sub_id,
		priority: request.priority,
		max_delay: Duration::from_millis(request.max_delay_ms.into()),
		live_join: request.live_join != 0,
	});
	if session.commands.send(command).is_ok() { 0 } else { -1 }
}

/// # Safety
/// `session` must come from `moqshim_session_open`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn moqshim_unsubscribe_track(session: *mut moqshim_session, sub_id: u64) -> i32 {
	let Some(session) = (unsafe { session.as_ref() }) else {
		return -1;
	};
	if session.commands.send(Command::Unsubscribe(sub_id)).is_ok() { 0 } else { -1 }
}

/// # Safety
/// `session` must come from `moqshim_session_open` and must not be used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn moqshim_session_close(session: *mut moqshim_session) {
	if session.is_null() {
		return;
	}
	let mut session = unsafe { Box::from_raw(session) };
	let _ = session.commands.send(Command::Close);
	if let Some(thread) = session.thread.take() {
		let _ = thread.join();
	}
}

async fn run(settings: Settings, sink: Arc<Sink>, mut commands: mpsc::UnboundedReceiver<Command>) -> Result<(), String> {
	let mut connect = moq_tokio::connect::Config::default();
	connect.url = Some(settings.url.clone());
	connect.version = settings.version.into_iter().collect();
	// The reference relay is QUIC-only; a WebSocket fallback only adds a refused dial.
	connect.websocket.enabled = Some(false);
	let client = connect.init(Default::default()).map_err(|err| format!("client: {err}"))?;

	let origin = moq_tokio::origin::spawn();
	sink.event(EVENT_CONNECTING, 0, 0, 0, Some(settings.url.as_str()));
	let connection = client.with_subscriber(origin.clone()).connect(settings.url.clone());

	let session_sink = sink.clone();
	let watcher = connection.clone();
	let connected = async move {
		let watcher = watcher.established().await.map_err(|err| format!("connect: {err}"))?;
		let version = watcher.version().map(|version| version.to_string()).unwrap_or_default();
		session_sink.event(EVENT_CONNECTED, 0, 0, 0, Some(&version));
		Ok::<_, String>(())
	};

	// Requests made before the broadcast resolves are held and replayed once it does.
	let mut pending = Vec::new();
	// Wait for a covering route rather than asking on the spot: the namespace announcement is
	// still in flight right after SETUP. The follower is held for the session so the path
	// stays watched.
	let consumer = origin.consume();
	let mut following = consumer
		.follow(settings.namespace.as_str(), None)
		.map_err(|err| format!("broadcast {}: {err}", settings.namespace))?;
	let broadcast = tokio::select! {
		res = async {
			connected.await?;
			following
				.available()
				.await
				.map_err(|err| format!("broadcast {}: {err}", settings.namespace))?
				.ok_or_else(|| format!("broadcast {} went offline before serving", settings.namespace))
		} => res?,
		res = connection.closed() => return res.map_err(|err| format!("connection: {err}")),
		() = hold_until_close(&mut commands, &mut pending) => return Ok(()),
	};
	sink.event(EVENT_BROADCAST_READY, 0, 0, 0, Some(&settings.namespace));

	let mut subscriptions = Subscriptions {
		broadcast,
		sink: sink.clone(),
		shutdown: CancellationToken::new(),
		active: HashMap::new(),
	};
	let mut closing = pending.into_iter().any(|command| !subscriptions.apply(command));
	let result = loop {
		if closing {
			break Ok(());
		}
		tokio::select! {
			command = commands.recv() => closing = !command.is_some_and(|command| subscriptions.apply(command)),
			res = connection.closed() => break res.map_err(|err| format!("connection: {err}")),
		}
	};
	subscriptions.shutdown.cancel();
	drop(connection);
	result
}

struct Subscriptions {
	broadcast: moq_net::broadcast::Consumer,
	sink: Arc<Sink>,
	shutdown: CancellationToken,
	active: HashMap<u64, CancellationToken>,
}

impl Subscriptions {
	/// Applies a command, returning false once the session should close.
	fn apply(&mut self, command: Command) -> bool {
		match command {
			Command::Subscribe(request) => {
				let cancel = self.shutdown.child_token();
				if let Some(previous) = self.active.insert(request.sub_id, cancel.clone()) {
					previous.cancel();
				}
				tokio::spawn(subscribe(self.broadcast.clone(), request, self.sink.clone(), cancel));
				true
			}
			Command::Unsubscribe(sub_id) => {
				if let Some(cancel) = self.active.remove(&sub_id) {
					cancel.cancel();
				}
				true
			}
			Command::Close => false,
		}
	}
}

async fn hold_until_close(commands: &mut mpsc::UnboundedReceiver<Command>, pending: &mut Vec<Command>) {
	while let Some(command) = commands.recv().await {
		if matches!(command, Command::Close) {
			return;
		}
		pending.push(command);
	}
}

async fn subscribe(broadcast: moq_net::broadcast::Consumer, request: SubscribeRequest, sink: Arc<Sink>, cancel: CancellationToken) {
	let sub_id = request.sub_id;
	let result = tokio::select! {
		res = read_track(&broadcast, &request, &sink, &cancel) => res,
		_ = cancel.cancelled() => Ok(()),
	};
	sink.event(EVENT_SUBSCRIPTION_ENDED, sub_id, 0, 0, result.err().as_deref());
}

async fn read_track(
	broadcast: &moq_net::broadcast::Consumer,
	request: &SubscribeRequest,
	sink: &Arc<Sink>,
	cancel: &CancellationToken,
) -> Result<(), String> {
	let track = broadcast
		.track(&request.track)
		.map_err(|err| format!("track {}: {err}", request.track))?;
	let range = if request.live_join {
		moq_net::track::Range::live()
	} else {
		moq_net::track::Range::oldest()
	};
	let subscription = moq_net::track::Subscription::default()
		.with_priority(request.priority)
		.with_delay(
			moq_net::track::Delay::default()
				.with_min(None)
				.with_max(request.max_delay),
		)
		.with_ranges([range]);
	let mut subscriber = track
		.subscribe(subscription)
		.await
		.map_err(|err| format!("subscribe {}: {err}", request.track))?;
	sink.event(EVENT_SUBSCRIBED, request.sub_id, 0, 0, Some(&request.track));

	while let Some(group) = subscriber.recv_group().await.map_err(|err| err.to_string())? {
		let sink = sink.clone();
		let cancel = cancel.clone();
		let sub_id = request.sub_id;
		tokio::spawn(async move {
			let group_id = group.start;
			sink.event(EVENT_GROUP_BEGIN, sub_id, group_id, 0, None);
			let (count, result) = tokio::select! {
				res = read_group(group, sub_id, &sink) => res,
				_ = cancel.cancelled() => (0, Err("cancelled".to_owned())),
			};
			sink.event(EVENT_GROUP_END, sub_id, group_id, count, result.err().as_deref());
		});
	}
	Ok(())
}

async fn read_group(mut group: moq_net::group::Consumer, sub_id: u64, sink: &Sink) -> (u64, Result<(), String>) {
	let group_id = group.start;
	let mut count = 0;
	loop {
		let index = group.index();
		match group.read_frame().await {
			Ok(Some(frame)) => {
				sink.object(sub_id, group_id, index, &frame.payload);
				count += 1;
			}
			Ok(None) => return (count, Ok(())),
			Err(err) => return (count, Err(err.to_string())),
		}
	}
}
