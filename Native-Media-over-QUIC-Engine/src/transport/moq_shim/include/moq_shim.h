/*
 * Narrow C ABI over the selected MOQT implementation (moq-dev/moq).
 *
 * Only src/transport/ may include this header. Everything above the transport
 * adapter sees the ReceivedObject model instead.
 *
 * Threading: callbacks run on the shim's runtime threads, possibly
 * concurrently for different groups. They must not block and must not call
 * back into the shim. Payload pointers are valid only for the callback's
 * duration; the caller copies what it keeps.
 */
#ifndef MOQ_SHIM_H
#define MOQ_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct moq_shim_session moq_shim_session;

typedef enum
{
	MOQ_SHIM_EVENT_CONNECTING = 1,     /* detail: URL */
	MOQ_SHIM_EVENT_CONNECTED = 2,      /* detail: negotiated version */
	MOQ_SHIM_EVENT_BROADCAST_READY = 3,/* detail: namespace */
	MOQ_SHIM_EVENT_SUBSCRIBED = 4,     /* sub_id; detail: track name */
	MOQ_SHIM_EVENT_GROUP_BEGIN = 5,    /* sub_id, group */
	MOQ_SHIM_EVENT_GROUP_END = 6,      /* sub_id, group, object = objects read; detail: NULL or error */
	MOQ_SHIM_EVENT_SUBSCRIPTION_ENDED = 7, /* sub_id; detail: NULL (clean) or error */
	MOQ_SHIM_EVENT_ERROR = 8,          /* detail: error text */
	MOQ_SHIM_EVENT_CLOSED = 9          /* detail: NULL or error */
} moq_shim_event_kind;

typedef struct
{
	int32_t kind;          /* moq_shim_event_kind */
	uint64_t sub_id;
	uint64_t group;
	uint64_t object;
	const char *detail;    /* UTF-8, may be NULL, valid for the callback only */
} moq_shim_event;

typedef struct
{
	uint64_t sub_id;
	uint64_t group;
	uint64_t object;
	const uint8_t *data;   /* valid for the callback only */
	size_t size;
} moq_shim_object;

typedef void (*moq_shim_event_cb)(void *ctx, const moq_shim_event *event);
typedef void (*moq_shim_object_cb)(void *ctx, const moq_shim_object *object);

typedef struct
{
	const char *url;        /* e.g. https://relay:4443/ (WebTransport) */
	const char *version;    /* e.g. "moq-transport-22"; NULL offers every version */
	const char *ns;         /* broadcast namespace, e.g. "mediakind-test" */
	const char *log_filter; /* tracing filter, e.g. "warn"; NULL leaves logging off */
	moq_shim_event_cb on_event;
	moq_shim_object_cb on_object;
	void *ctx;
} moq_shim_config;

typedef struct
{
	const char *track;
	uint64_t sub_id;        /* caller-chosen, echoed in events and objects */
	int8_t priority;        /* higher first */
	uint32_t max_delay_ms;  /* superseded-group budget; 0 = skip a superseded group at once */
	int32_t live_join;      /* 1 = start at the latest group (join current group); 0 = oldest available */
} moq_shim_subscribe;

/* Starts the runtime and the connection; returns NULL on invalid config. */
moq_shim_session *moq_shim_session_open(const moq_shim_config *config);

/* Queues a subscription; results arrive as events. Returns 0 when queued. */
int32_t moq_shim_subscribe_track(moq_shim_session *session, const moq_shim_subscribe *request);

/* Withdraws a subscription; returns 0 when queued. */
int32_t moq_shim_unsubscribe_track(moq_shim_session *session, uint64_t sub_id);

/* Closes the session, waits for the runtime to stop and frees it. No callback runs after return. */
void moq_shim_session_close(moq_shim_session *session);

/* Static identity of the pinned implementation, e.g. "moq-net@b3de94d". */
const char *moq_shim_implementation(void);

#ifdef __cplusplus
}
#endif

#endif /* MOQ_SHIM_H */
