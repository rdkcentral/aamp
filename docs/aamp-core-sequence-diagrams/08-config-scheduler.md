# 08 - Configuration & Scheduler

## Module: AampConfig + AampScheduler
## Source Files Read
- `AampConfig.h` (lines 1-350, complete enum definitions) ✅
- `AampConfig.cpp` (lines 1-150, constructor, validation, owner lookup) ✅
- `AampScheduler.h` (lines 1-200, complete) ✅
- `AampScheduler.cpp` (lines 1-200, complete implementation) ✅

## Confidence: 95%
- Gap: `AampConfig.cpp` lines 150+ (ReadAampCfgTxtFile, ProcessConfigJson, individual Get/Set implementations)

---

## 1. Configuration Hierarchy & Override Flow

```mermaid
sequenceDiagram
    participant App as Application
    participant Tune as Tune Settings
    participant Oper as Operator (RFC)
    participant Stream as Stream Settings
    participant DevCfg as Dev Config File
    participant Config as AampConfig

    Note over Config: Priority Order (highest to lowest):<br/>TUNE > DEV_CFG > STREAM > APP > OPERATOR > DEFAULT

    App->>Config: SetConfigValue(AAMP_APPLICATION_SETTING, key, value)
    Config->>Config: Store value with owner=APP
    
    Oper->>Config: SetConfigValue(AAMP_OPERATOR_SETTING, key, value)
    Config->>Config: Store value with owner=OPERATOR

    Tune->>Config: SetConfigValue(AAMP_TUNE_SETTING, key, value)
    Config->>Config: Store value with owner=TUNE (highest priority)

    Stream->>Config: SetConfigValue(AAMP_STREAM_SETTING, key, value)
    Config->>Config: Store value with owner=STREAM

    DevCfg->>Config: ReadAampCfgTxtFile() / ProcessConfigJson()
    Config->>Config: Store values with owner=DEV_CFG

    App->>Config: IsConfigSet(eAAMPConfig_EnableABR)
    Config-->>App: Returns effective value (highest priority owner wins)

    App->>Config: GetConfigOwner(key)
    Config-->>App: Returns which owner set the active value
```

## 2. Configuration Data Types & Storage

```mermaid
sequenceDiagram
    participant Caller as Caller
    participant Config as AampConfig
    participant BoolArr as bAampCfgValue[BOOL_COUNT]
    participant IntArr as iAampCfgValue[INT_COUNT]
    participant FloatArr as dAampCfgValue[FLOAT_COUNT]
    participant StrArr as sAampCfgValue[STRING_COUNT]

    Note over Config: Config types: Bool, Int (long), Float (double), String

    Caller->>Config: SetConfigValue(owner, eAAMPConfig_EnableABR, true)
    Config->>Config: Validate: owner priority >= current owner?
    Config->>BoolArr: Store {value=true, owner=APP, lastowner=prev}

    Caller->>Config: SetConfigValue(owner, eAAMPConfig_DefaultBitrate, 4000000)
    Config->>Config: Validate range (mConfigValueValidRange lookup)
    alt Value in valid range
        Config->>IntArr: Store {value=4000000, owner=APP}
    else Value out of range
        Config-->>Caller: Log ERROR_TEXT_BAD_RANGE, reject
    end

    Caller->>Config: SetConfigValue(owner, eAAMPConfig_NetworkTimeout, 10.0)
    Config->>FloatArr: Store {value=10.0, owner=APP}

    Caller->>Config: SetConfigValue(owner, eAAMPConfig_LicenseServerUrl, "https://...")
    Config->>StrArr: Store {value="https://...", owner=APP}
```

## 3. Configuration Categories (from source enums)

```mermaid
sequenceDiagram
    participant User as User/App
    participant Config as AampConfig
    participant Player as PrivateInstanceAAMP

    Note over Config: BOOL configs (150+):<br/>ABR, Fog, DRM, Logging, Pipeline, Sink, Features

    User->>Config: Key ABR configs
    Note right of Config: EnableABR, ABRBufferCheckEnabled,<br/>PersistentBitRateOverSeek,<br/>DashParallelFragDownload

    User->>Config: Key DRM configs
    Note right of Config: EnablePROutputProtection,<br/>SetLicenseCaching, FragMp4PrefetchLicense,<br/>RuntimeDRMConfig, AnonymousLicenseRequest

    User->>Config: Key Pipeline configs
    Note right of Config: UseWesterosSink, useRialtoSink,<br/>UseSinglePipeline, GStreamerBufferingBeforePlay,<br/>EnableMediaProcessor, EnableChunkInjection

    User->>Config: Key Network configs
    Note right of Config: ForceHttp, SslVerifyPeer,<br/>EnableCurlStore, EnableCMCD,<br/>PropagateURIParam

    User->>Config: INT configs (100+)
    Note right of Config: Timeouts, buffer sizes, retry limits,<br/>bitrate defaults, TSB settings,<br/>ABR thresholds

    User->>Config: FLOAT configs
    Note right of Config: NetworkTimeout, ManifestTimeout,<br/>PlaylistTimeout, LiveOffset,<br/>ReportProgressInterval

    Player->>Config: ISCONFIGSET(eAAMPConfig_EnableABR)
    Config-->>Player: true/false (macro expands to mConfig->IsConfigSet())

    Player->>Config: GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate)
    Config-->>Player: 4000000 (long value)
```

## 4. Scheduler Task Lifecycle

```mermaid
sequenceDiagram
    participant Player as PrivateInstanceAAMP
    participant Sched as AampScheduler
    participant Queue as mTaskQueue (deque)
    participant Thread as Scheduler Thread

    Player->>Sched: StartScheduler(playerId)
    Sched->>Thread: Create std::thread(ExecuteAsyncTask)
    Sched->>Sched: mSchedulerRunning = true

    Note over Thread: Thread blocks on mQCond.wait()

    Player->>Sched: ScheduleTask(AsyncTaskObj{task, data, "SetRate"})
    Sched->>Sched: Check state != ERROR/RELEASED
    Sched->>Sched: Check !mLockOut
    Sched->>Sched: Assign mNextTaskId++ (wraps at INT_MAX)
    alt Task is "SetRate"
        Sched->>Queue: Remove existing "SetRate" if queued
    end
    Sched->>Queue: push_back(obj)
    Sched->>Thread: mQCond.notify_one()

    Thread->>Thread: Wake from wait
    Thread->>Thread: Unlock queueLock
    Thread->>Thread: Acquire mExMutex (execution lock)
    Thread->>Thread: Lock queueLock again
    Thread->>Queue: front() + pop_front()
    Thread->>Thread: Check state != ERROR/RELEASED
    Thread->>Thread: Unlock queueLock
    Thread->>Thread: Execute obj.mTask(obj.mData)
    Thread->>Thread: Lock queueLock, loop back to wait
```

## 5. Scheduler Suspend/Resume/Stop

```mermaid
sequenceDiagram
    participant Caller as Caller (Tune/Teardown)
    participant Sched as AampScheduler
    participant Thread as Scheduler Thread
    participant Queue as mTaskQueue

    Note over Caller,Sched: SuspendScheduler - blocks thread from executing tasks

    Caller->>Sched: SuspendScheduler()
    Sched->>Sched: mExLock.lock() (acquires mExMutex)
    Sched->>Sched: mLockOut = true
    Note over Thread: Thread blocked at lock_guard(mExMutex) in ExecuteAsyncTask

    Caller->>Sched: RemoveAllTasks()
    Sched->>Queue: clear() all pending tasks

    Note over Caller,Sched: ResumeScheduler - allows thread to continue

    Caller->>Sched: ResumeScheduler()
    Sched->>Sched: mExLock.unlock()
    Sched->>Sched: mLockOut = false
    Note over Thread: Thread unblocked, resumes processing

    Note over Caller,Sched: StopScheduler - full shutdown

    Caller->>Sched: StopScheduler()
    Sched->>Sched: mSchedulerRunning = false
    Sched->>Sched: SuspendScheduler() if not already
    Sched->>Sched: RemoveAllTasks()
    Sched->>Sched: ResumeScheduler()
    Sched->>Sched: mQCond.notify_one() (wake thread)
    Thread->>Thread: Loop exits (mSchedulerRunning=false)
    Sched->>Thread: join()
    Note over Sched: Scheduler fully stopped
```

## 6. Config + Scheduler Integration in Tune

```mermaid
sequenceDiagram
    participant App as Application
    participant AAMP as PrivateInstanceAAMP
    participant Config as AampConfig
    participant Sched as AampScheduler

    App->>AAMP: Tune(url, ...)
    AAMP->>Config: Load tune-time overrides (AAMP_TUNE_SETTING)
    AAMP->>Config: ProcessConfigJson(tuneParams)
    
    AAMP->>Sched: SetState(eSTATE_PREPARING)
    
    AAMP->>Config: ISCONFIGSET(eAAMPConfig_Fog)
    Config-->>AAMP: true/false
    
    AAMP->>Config: GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout)
    Config-->>AAMP: timeout value (double)

    AAMP->>Sched: ScheduleTask({TuneHelper, data, "TuneHelper"})
    Sched->>Sched: Queue task, notify thread

    Note over Sched: Thread executes TuneHelper asynchronously
    
    Sched->>AAMP: TuneHelper() executes
    AAMP->>Config: ISCONFIGSET(eAAMPConfig_DashParallelFragDownload)
    AAMP->>Config: GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate)
    
    AAMP->>Sched: SetState(eSTATE_PLAYING)
```

---

## Key Design Patterns (from source)

1. **Owner-priority config system**: 6 priority levels (DEFAULT < OPERATOR < STREAM < APP < TUNE < DEV_CFG) — higher priority owner overrides lower
2. **Range validation**: Each int config has a valid range enforced at set time
3. **Macro access**: `ISCONFIGSET()`, `GETCONFIGVALUE()`, `SETCONFIGVALUE()` macros for ergonomic usage
4. **SetRate dedup**: Scheduler removes duplicate `SetRate` tasks from queue before adding new one
5. **State gating**: Scheduler rejects tasks when player is in `eSTATE_ERROR` or `eSTATE_RELEASED`
6. **Suspend/Lock pattern**: `SuspendScheduler()` holds execution mutex, preventing task execution while queue is manipulated
