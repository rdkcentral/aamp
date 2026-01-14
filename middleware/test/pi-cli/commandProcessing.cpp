#include "commandProcessing.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <atomic>
#include <chrono>

// Forward declaration for use in PositionPollingThread
void getPositionMillisecondsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);

// --- PositionPollingThread Helper ---
class PositionPollingThread {
public:
    PositionPollingThread(InterfacePlayerRDK& player)
        : player(player), running(false) {}

    void start(int intervalMs) {
        stop();
        running = true;
        pollingThread = std::thread([this, intervalMs]() {
            while (running) {
                getPositionMillisecondsCommand(player, {});
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
        });
    }

    void stop() {
        running = false;
        if (pollingThread.joinable()) {
            pollingThread.join();
        }
    }

    ~PositionPollingThread() {
        stop();
    }

private:
    InterfacePlayerRDK& player;
    std::atomic<bool> running;
    std::thread pollingThread;
};

// --- CommandExecutor Implementation ---
void CommandExecutor::threadFunction() {
    while (true) {
        std::function<void(const std::vector<std::string>&)> currentTask;
        std::vector<std::string> currentParams;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&]() { return !commandQueue.empty() || stopThread; });

            if (stopThread && commandQueue.empty()) {
                break;
            }

            auto taskPair = commandQueue.front();
            commandQueue.pop();
            currentTask = taskPair.first;
            currentParams = taskPair.second;
        }

        if (currentTask) {
            currentTask(currentParams);
        }
    }
}

CommandExecutor::CommandExecutor() {
    workerThread = std::thread(&CommandExecutor::threadFunction, this);
}

CommandExecutor::~CommandExecutor() {
    stop();
}

void CommandExecutor::addCommand(const std::function<void(const std::vector<std::string>&)>& command, const std::vector<std::string>& params) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.emplace(command, params);
    }
    cv.notify_one();
}

void CommandExecutor::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stopThread = true;
    }
    cv.notify_one();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void displayHelp(const std::map<std::string, Command>& commands) {
    // Find the maximum width of "- command:" for alignment
    size_t maxCmdWidth = 0;
    for (const auto& [key, command] : commands) {
        size_t len = command.matchValue.length() + 1; // matchValue + ":"
        if (len > maxCmdWidth)
            maxCmdWidth = len;
    }
    // Set the helpText column (e.g., 2 spaces after the widest command)
    size_t helpCol = maxCmdWidth + 2;

    std::cout << "Available commands:\n";
    for (const auto& [key, command] : commands) {
        std::string cmdStr = command.matchValue + ":";
        size_t cmdWidth = cmdStr.length();

        // Split helpText
        const std::string& help = command.helpText;
        size_t usagePos = help.find("Usage:");
        std::string desc, usage;
        if (usagePos != std::string::npos) {
            desc = help.substr(0, usagePos);
            usage = help.substr(usagePos);
        } else {
            desc = help;
        }

        // Print command and aligned description
        std::cout << cmdStr;
        if (!desc.empty() && desc.back() == '\n')
            desc.pop_back();
        // Align to helpCol
        if (cmdWidth < helpCol)
            std::cout << std::string(helpCol - cmdWidth, ' ');
        std::cout << "- " << desc << std::endl;

        // Print aligned usage if present
        if (!usage.empty()) {
            std::cout << std::string(helpCol, ' ') << usage << std::endl;
        }
    }
}

// --- Command Wrappers ---

void createPipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() < 1 || params.size() > 2) {
        std::cout << "Usage: createpipeline <pipelineName> [pipelinePriority]\n";
        return;
    }
    const char* pipelineName = params[0].c_str();
    int priority = (params.size() > 1 ? std::stoi(params[1]) : 0);
    bool ok = player.CreatePipeline(pipelineName, priority);
    std::cout << "CreatePipeline executed. Success: " << (ok ? "true" : "false") << "\n";
}

void setPlayerNameCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setplayername <name>\n";
        return;
    }
    player.SetPlayerName(params[0]);
    std::cout << "SetPlayerName executed.\n";
}

void setPreferredDRMCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setpreferreddrm <drmID>\n";
        return;
    }
    player.SetPreferredDRM(params[0].c_str());
    std::cout << "SetPreferredDRM executed.\n";
}

void initializeSourceForPlayerCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() < 3) {
        std::cout << "Usage: initializesourceforplayer <playerInstance(int)> <source(int)> <mediaType>\n";
        return;
    }
    void* playerInstance = reinterpret_cast<void*>(std::stoul(params[0]));
    void* source = reinterpret_cast<void*>(std::stoul(params[1]));
    int mediaType = std::stoi(params[2]);
    player.InitializeSourceForPlayer(playerInstance, source, mediaType);
    std::cout << "InitializeSourceForPlayer executed.\n";
}

void configurePipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    // The signature from InterfacePlayerRDK.h:
    // void ConfigurePipeline(int, int, int, int, bool, bool, bool, bool, int32_t, gint, const char *, int, bool, std::string url);
    if (params.size() != 14) {
        std::cout << "Usage: configurepipeline <int> <int> <int> <int> <bool> <bool> <bool> <bool> <int32> <gint> <pipelineName> <int> <bool> <url>\n";
        return;
    }

    try {
        int format = std::stoi(params[0]);
        int audioFormat = std::stoi(params[1]);
        int auxFormat = std::stoi(params[2]);
        int subFormat = std::stoi(params[3]);
        bool bESChangeStatus = (params[4] == "1" || params[4] == "true");
        bool forwardAudioToAux = (params[5] == "1" || params[5] == "true");
        bool setReadyAfterPipelineCreation = (params[6] == "1" || params[6] == "true");
        bool isSubEnable = (params[7] == "1" || params[7] == "true");
        int32_t trackId = std::stoi(params[8]);
        gint rate = std::stoi(params[9]);
        std::string pipelineNameStr = params[10];
        const char* pipelineName = pipelineNameStr.c_str();
        int PipelinePriority = std::stoi(params[11]);
        bool subBool = (params[12] == "1" || params[12] == "true");
        std::string url = params[13];

        player.ConfigurePipeline(
            format, audioFormat, auxFormat, subFormat,
            bESChangeStatus, forwardAudioToAux, setReadyAfterPipelineCreation, isSubEnable,
            trackId, rate, pipelineName, PipelinePriority, subBool, url
        );
        std::cout << "ConfigurePipeline executed.\n";
    } catch (const std::exception& e) {
        std::cout << "Error parsing parameters: " << e.what() << "\n";
        std::cout << "Usage: configurepipeline <int> <int> <int> <int> <bool> <bool> <bool> <bool> <int32> <gint> <pipelineName> <int> <bool> <url>\n";
    }
}

void setPauseOnStartCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setpauseonstart [true|false]\n";
        return;
    }
    std::string val = params[0];
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if (val == "true" || val == "1") {
        player.SetPauseOnStartPlayback(true);
        std::cout << "Pause on start playback ENABLED\n";
    } else if (val == "false" || val == "0") {
        player.SetPauseOnStartPlayback(false);
        std::cout << "Pause on start playback DISABLED\n";
    } else {
        std::cout << "Invalid value. Usage: setpauseonstart [true|false]\n";
    }
}

void setPlayBackRateCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setplaybackrate <rate>\n";
        return;
    }
    double rate = std::stod(params[0]);
    bool ok = player.SetPlayBackRate(rate);
    std::cout << "SetPlayBackRate executed. Success: " << (ok ? "true" : "false") << "\n";
}

void setAudioVolumeCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setaudiovolume <volume>\n";
        return;
    }
    int volume = std::stoi(params[0]);
    player.SetAudioVolume(volume);
    std::cout << "SetAudioVolume executed.\n";
}

void setupStreamCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 2) {
        std::cout << "Usage: setupstream <streamId> <url>\n";
        return;
    }
    int streamId = std::stoi(params[0]);
    std::string url = params[1];
    int result = player.InterfacePlayer_SetupStream(streamId, url);
    std::cout << "SetupStream executed. Result: " << result << "\n";
}

void pauseCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    bool pauseVal = true;
    bool forceStop = false;
    if (params.size() == 2) {
        pauseVal = (params[0] == "1" || params[0] == "true");
        forceStop = (params[1] == "1" || params[1] == "true");
    }
    bool result = player.Pause(pauseVal, forceStop);
    std::cout << "Pause executed. Result: " << (result ? "true" : "false") << "\n";
}

void resumeInjectorCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.ResumeInjector();
    std::cout << "ResumeInjector executed.\n";
}

void stopCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    bool keepLastFrame = false;
    if (params.size() > 0)
        keepLastFrame = (params[0] == "1" || params[0] == "true");
    player.Stop(keepLastFrame);
    std::cout << "Stop executed.\n";
}

void flushCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 4) {
        std::cout << "Usage: flush <position(double)> <rate(int)> <shouldTearDown(true|false)> <isAppSeek(true|false)>\n";
        return;
    }
    double position = std::stod(params[0]);
    int rate = std::stoi(params[1]);
    bool shouldTearDown = (params[2] == "1" || params[2] == "true");
    bool isAppSeek = (params[3] == "1" || params[3] == "true");
    bool result = player.Flush(position, rate, shouldTearDown, isAppSeek);
    std::cout << "Flush executed. Result: " << (result ? "true" : "false") << "\n";
}

void flushTrackCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 4) {
        std::cout << "Usage: flushtrack <mediaType> <pos> <audioDelta> <subDelta>\n";
        return;
    }
    int mediaType = std::stoi(params[0]);
    double pos = std::stod(params[1]);
    double audioDelta = std::stod(params[2]);
    double subDelta = std::stod(params[3]);
    double result = player.FlushTrack(mediaType, pos, audioDelta, subDelta);
    std::cout << "FlushTrack executed. Result: " << result << "\n";
}

void isPipelinePausedCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    bool paused = player.IsPipelinePaused();
    std::cout << "IsPipelinePaused: " << (paused ? "true" : "false") << "\n";
}

void enablePendingPlayStateCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.EnablePendingPlayState();
    std::cout << "EnablePendingPlayState executed.\n";
}

void setPendingSeekCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: setpendingseek <true|false>\n";
        return;
    }
    bool state = (params[0] == "1" || params[0] == "true");
    player.SetPendingSeek(state);
    std::cout << "SetPendingSeek executed.\n";
}

void triggerEventCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 2) {
        std::cout << "Usage: triggerevent <eventId(int)> <data(int)>\n";
        return;
    }
    int event = std::stoi(params[0]);
    int data = std::stoi(params[1]);
    player.TriggerEvent(static_cast<InterfaceCB>(event), data);
    std::cout << "TriggerEvent executed.\n";
}

void notifyEOSCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.NotifyEOS();
    std::cout << "NotifyEOS executed.\n";
}

void notifyFirstFrameCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: notifyfirstframe <mediaType>\n";
        return;
    }
    int mt = std::stoi(params[0]);
    player.NotifyFirstFrame(mt);
    std::cout << "NotifyFirstFrame executed.\n";
}

void endOfStreamReachedCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 2) {
        std::cout << "Usage: endofstreamreached <mediaType> <shouldHaltBuffering>\n";
        return;
    }
    int mediaType = std::stoi(params[0]);
    bool shouldHaltBuffering = (params[1] == "1" || params[1] == "true");
    player.EndOfStreamReached(mediaType, shouldHaltBuffering);
    std::cout << "EndOfStreamReached executed.\n";
}

void sendHelperCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "SendHelper: Not implemented in CLI, needs binary data and output refs.\n";
}

void waitForSourceSetupCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: waitforsourcesetup <mediaType>\n";
        return;
    }
    int mediaType = std::stoi(params[0]);
    bool ok = player.WaitForSourceSetup(mediaType);
    std::cout << "WaitForSourceSetup executed. Success: " << (ok ? "true" : "false") << "\n";
}

void needDataCbCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "needDataCb: CLI cannot trigger callbacks, use in app code.\n";
}

void enoughDataCbCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "enoughDataCb: CLI cannot trigger callbacks, use in app code.\n";
}

void bufferingTimeoutCbCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "bufferingTimeoutCb: CLI cannot trigger callbacks, use in app code.\n";
}

void decodeErrorCbCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "decodeErrorCb: CLI cannot trigger callbacks, use in app code.\n";
}

void ptsErrorCbCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "ptsErrorCb: CLI cannot trigger callbacks, use in app code.\n";
}

void timerAddCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    std::cout << "timeradd: CLI cannot add timers, needs callback pointer.\n";
}

void timerRemoveCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 2) {
        std::cout << "Usage: timerremove <taskId> <timerName>\n";
        return;
    }
    guint taskId = static_cast<guint>(std::stoul(params[0]));
    const char* timerName = params[1].c_str();
    player.TimerRemove(taskId, timerName);
    std::cout << "TimerRemove executed.\n";
}

void timerIsRunningCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: timerisrunning <taskId>\n";
        return;
    }
    guint taskId = static_cast<guint>(std::stoul(params[0]));
    bool running = player.TimerIsRunning(taskId);
    std::cout << "TimerIsRunning: " << (running ? "true" : "false") << "\n";
}

//void dumpDiagnosticsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
//    player.DumpDiagnostics();
//    std::cout << "DumpDiagnostics executed.\n";
//}

void enableGstDebugLoggingCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: enablegstdebuglogging <debugLevel>\n";
        return;
    }
    player.EnableGstDebugLogging(params[0]);
    std::cout << "EnableGstDebugLogging executed.\n";
}

void initializePlayerGstreamerPluginsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    InterfacePlayerRDK::InitializePlayerGstreamerPlugins();
    std::cout << "InitializePlayerGstreamerPlugins executed.\n";
}

void tearDownStreamCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: teardownstream <mediaType>\n";
        return;
    }
    int mediaType = std::stoi(params[0]);
    player.TearDownStream(mediaType);
    std::cout << "TearDownStream executed.\n";
}

void destroyPipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.DestroyPipeline();
    std::cout << "DestroyPipeline executed.\n";
}

void removeProbesCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.RemoveProbes();
    std::cout << "RemoveProbes executed.\n";
}

void clearProtectionEventCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    player.ClearProtectionEvent();
    std::cout << "ClearProtectionEvent executed.\n";
}

void setVideoRectangle(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    if (params.size() != 4) {
        std::cout << "Usage: setvideorectangle <x> <y> <width> <height>\n";
        return;
    }
    try {
        int x = std::stoi(params[0]);
        int y = std::stoi(params[1]);
        int w = std::stoi(params[2]);
        int h = std::stoi(params[3]);
        player.SetVideoRectangle(x, y, w, h);
        std::cout << "SetVideoRectangle executed: (" << x << ", " << y << ", " << w << ", " << h << ")\n";
    } catch (const std::exception& e) {
        std::cout << "Invalid arguments, must be integers. Usage: setvideorectangle <x> <y> <width> <height>\n";
    }
}

void injectFragmentCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    // Usage: injectfragment <mediaType:int> <filePath> [pts] [dts] [duration] [fragmentPTSoffset]
    if (params.size() < 3) {
        std::cout << "Usage: injectfragment <mediaType:int> <filePath> <initFragment> [pts] [dts] [duration] [fragmentPTSoffset]\n";
        return;
    }
    int mediaType = std::stoi(params[0]);
    std::string filePath = params[1];

    bool initFragment = (params[2] == "1" || params[2] == "true");
    double pts = params.size() > 3 ? std::stod(params[3]) : 0.0;
    double dts = params.size() > 4 ? std::stod(params[4]) : 0.0;
    double duration = params.size() > 5 ? std::stod(params[5]) : 0.0;
    double fragmentPTSoffset = params.size() > 6 ? std::stod(params[6]) : 0.0;

    // Read file into buffer
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cout << "injectfragment: Failed to open file: " << filePath << "\n";
        return;
    }
    std::streamsize size = file.tellg();
    if (size <= 0) {
        std::cout << "injectfragment: File is empty: " << filePath << "\n";
        return;
    }
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cout << "injectfragment: Failed to read file: " << filePath << "\n";
        return;
    }

    // Default flags for SendHelper
    bool copy = true;
    bool discontinuity = false;
    bool notifyFirstBufferProcessed = false;
    bool sendNewSegmentEvent = false;
    bool resetTrickUTC = false;
    bool firstBufferPushed = false;

    bool ok = player.SendHelper(
        mediaType,
        buffer.data(),
        buffer.size(),
        pts,
        dts,
        duration,
        fragmentPTSoffset,
        copy,
        initFragment,
        discontinuity,
        notifyFirstBufferProcessed,
        sendNewSegmentEvent,
        resetTrickUTC,
        firstBufferPushed
    );
    std::cout << "injectfragment executed. Success: " << (ok ? "true" : "false") << "\n";
}

void getPositionMillisecondsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params) {
    // Usage: getpositionmilliseconds
    if (!params.empty()) {
        std::cout << "Usage: getpositionmilliseconds\n";
        return;
    }
    int64_t posMs = player.GetPositionMilliseconds();
    std::cout << "GetPositionMilliseconds: " << posMs << " ms\n";
}


static PositionPollingThread* g_positionPollingThread = nullptr;

void reportPositionsCommand(const std::vector<std::string>& params) {
    if (params.size() != 1) {
        std::cout << "Usage: reportpositions <intervalMs>\n";
        return;
    }
    int intervalMs = std::stoi(params[0]);
    if (intervalMs <= 0) {
        g_positionPollingThread->stop();
        std::cout << "Stopped position polling.\n";
        return;
    }
    g_positionPollingThread->start(intervalMs);
    std::cout << "Started position polling every " << intervalMs << " ms.\n";
}

// --- Register All Commands ---
std::map<std::string, Command> initializeCommands(CommandExecutor& executor, InterfacePlayerRDK& player) {
    std::map<std::string, Command> commands;
    commands.emplace("exit", Command("exit", "Exit the program.", [&executor](const std::vector<std::string>&) { std::cout << "Exiting the program. Goodbye!" << std::endl; exit(0); }));
    commands.emplace("help", Command("help", "Display this help message.", [&commands](const std::vector<std::string>&) { displayHelp(commands); }));
    commands.emplace("echo", Command("echo", "Echoes the provided parameters.", [](const std::vector<std::string>& params) { std::cout << "Echo: "; for (const auto& param : params) std::cout << param << " "; std::cout << std::endl; }));
    commands.emplace("hello", Command("hello", "Prints a hello world message.", [&commands](const std::vector<std::string>&) { std::cout << "Hello World!" << std::endl; }));

    commands.emplace("createpipeline", Command("createpipeline", "Create a pipeline. Usage: createpipeline <pipelineName> [pipelinePriority]", [&player](const std::vector<std::string>& params) { createPipelineCommand(player, params); }));
    commands.emplace("setplayername", Command("setplayername", "Set player name. Usage: setplayername <name>", [&player](const std::vector<std::string>& params) { setPlayerNameCommand(player, params); }));
    commands.emplace("setpreferreddrm", Command("setpreferreddrm", "Set preferred DRM. Usage: setpreferreddrm <drmID>", [&player](const std::vector<std::string>& params) { setPreferredDRMCommand(player, params); }));
    commands.emplace("initializesourceforplayer", Command("initializesourceforplayer", "Initialize source. Usage: initializesourceforplayer <playerInstance(int)> <source(int)> <mediaType>", [&player](const std::vector<std::string>& params) { initializeSourceForPlayerCommand(player, params); }));
    commands.emplace("configurepipeline", Command("configurepipeline", "Configure pipeline. Usage: configurepipeline <int> <int> <int> <int> <bool> <bool> <bool> <bool> <int32> <gint> <pipelineName> <int> <bool> <url>", [&player](const std::vector<std::string>& params) { configurePipelineCommand(player, params); }));
    commands.emplace("setpauseonstart", Command("setpauseonstart", "Enable/disable pause on start playback. Usage: setpauseonstart [true|false]", [&player](const std::vector<std::string>& params) { setPauseOnStartCommand(player, params); }));
    commands.emplace("setplaybackrate", Command("setplaybackrate", "Set playback rate. Usage: setplaybackrate <rate>", [&player](const std::vector<std::string>& params) { setPlayBackRateCommand(player, params); }));
    commands.emplace("setaudiovolume", Command("setaudiovolume", "Set audio volume. Usage: setaudiovolume <volume>", [&player](const std::vector<std::string>& params) { setAudioVolumeCommand(player, params); }));
    commands.emplace("setupstream", Command("setupstream", "Setup stream. Usage: setupstream <streamId> <playerInstance(int)> <url>", [&player](const std::vector<std::string>& params) { setupStreamCommand(player, params); }));
    commands.emplace("pause", Command("pause", "Pause the pipeline. Usage: pause [pause(bool)] [forceStop(bool)]", [&player](const std::vector<std::string>& params) { pauseCommand(player, params); }));
    commands.emplace("resumeinjector", Command("resumeinjector", "Resume injector.", [&player](const std::vector<std::string>& params) { resumeInjectorCommand(player, params); }));
    commands.emplace("stop", Command("stop", "Stop playback.", [&player](const std::vector<std::string>& params) { stopCommand(player, params); }));
    commands.emplace("flush", Command("flush", "Flush pipeline.", [&player](const std::vector<std::string>& params) { flushCommand(player, params); }));
    commands.emplace("flushtrack", Command("flushtrack", "Flush track. Usage: flushtrack <mediaType> <pos> <audioDelta> <subDelta>", [&player](const std::vector<std::string>& params) { flushTrackCommand(player, params); }));
    commands.emplace("ispipelinepaused", Command("ispipelinepaused", "Check if pipeline is paused.", [&player](const std::vector<std::string>& params) { isPipelinePausedCommand(player, params); }));
    commands.emplace("enablependingplaystate", Command("enablependingplaystate", "Enable pending play state. Usage: enablependingplaystate <true|false>", [&player](const std::vector<std::string>& params) { enablePendingPlayStateCommand(player, params); }));
    commands.emplace("setpendingseek", Command("setpendingseek", "Set pending seek. Usage: setpendingseek <true|false>", [&player](const std::vector<std::string>& params) { setPendingSeekCommand(player, params); }));
    commands.emplace("triggerevent", Command("triggerevent", "Trigger event. Usage: triggerevent <eventId(int)> <data(int)>", [&player](const std::vector<std::string>& params) { triggerEventCommand(player, params); }));
    commands.emplace("notifyeos", Command("notifyeos", "Notify EOS.", [&player](const std::vector<std::string>& params) { notifyEOSCommand(player, params); }));
    commands.emplace("notifyfirstframe", Command("notifyfirstframe", "Notify first frame. Usage: notifyfirstframe <mediaType>", [&player](const std::vector<std::string>& params) { notifyFirstFrameCommand(player, params); }));
    commands.emplace("endofstreamreached", Command("endofstreamreached", "End of stream reached. Usage: endofstreamreached <mediaType> <shouldHaltBuffering>", [&player](const std::vector<std::string>& params) { endOfStreamReachedCommand(player, params); }));
    commands.emplace("sendhelper", Command("sendhelper", "Send helper event (not available in CLI).", [&player](const std::vector<std::string>& params) { sendHelperCommand(player, params); }));
    commands.emplace("waitforsourcesetup", Command("waitforsourcesetup", "Wait for source setup. Usage: waitforsourcesetup <mediaType>", [&player](const std::vector<std::string>& params) { waitForSourceSetupCommand(player, params); }));
    commands.emplace("timeradd", Command("timeradd", "Add timer (not available in CLI).", [&player](const std::vector<std::string>& params) { timerAddCommand(player, params); }));
    commands.emplace("timerremove", Command("timerremove", "Remove timer. Usage: timerremove <taskId>", [&player](const std::vector<std::string>& params) { timerRemoveCommand(player, params); }));
    commands.emplace("timerisrunning", Command("timerisrunning", "Check if timer is running. Usage: timerisrunning <taskId>", [&player](const std::vector<std::string>& params) { timerIsRunningCommand(player, params); }));
//    commands.emplace("dumpdiagnostics", Command("dumpdiagnostics", "Dump diagnostics.", [&player](const std::vector<std::string>& params) { dumpDiagnosticsCommand(player, params); }));
    commands.emplace("enablegstdebuglogging", Command("enablegstdebuglogging", "Enable GStreamer debug logging. Usage: enablegstdebuglogging <debugLevel>", [&player](const std::vector<std::string>& params) { enableGstDebugLoggingCommand(player, params); }));
    commands.emplace("initializeplayergstreamerplugins", Command("initializeplayergstreamerplugins", "Initialize Player GStreamer plugins.", [&player](const std::vector<std::string>& params) { initializePlayerGstreamerPluginsCommand(player, params); }));
    commands.emplace("teardownstream", Command("teardownstream", "Teardown stream. Usage: teardownstream <mediaType>", [&player](const std::vector<std::string>& params) { tearDownStreamCommand(player, params); }));
    commands.emplace("destroypipeline", Command("destroypipeline", "Destroy pipeline.", [&player](const std::vector<std::string>& params) { destroyPipelineCommand(player, params); }));
    commands.emplace("removeprobes", Command("removeprobes", "Remove probes.", [&player](const std::vector<std::string>& params) { removeProbesCommand(player, params); }));
    commands.emplace("clearprotectionevent", Command("clearprotectionevent", "Clear protection event.", [&player](const std::vector<std::string>& params) { clearProtectionEventCommand(player, params); }));
    commands.emplace("setvideorectangle", Command("setvideorectangle", "Usage: setvideorectangle <x> <y> <width> <height>", [&](const std::vector<std::string>& params) { setVideoRectangle(player, params); }));
    commands.emplace("injectfragment", Command("injectfragment", "Inject a fragment into the player. Usage: injectfragment <mediaType:int> <filePath> [pts] [dts] [duration] [fragmentPTSoffset]", [&player](const std::vector<std::string>& params) { injectFragmentCommand(player, params); } ) );
    commands.emplace("getpositionmilliseconds", Command("getpositionmilliseconds", "Get current position in milliseconds.", [&player](const std::vector<std::string>& params) { getPositionMillisecondsCommand(player, params); }));

    // Initialize the polling thread pointer if not already done
    if (!g_positionPollingThread) {
        g_positionPollingThread = new PositionPollingThread(player);
    }
    commands.emplace("reportpositions", Command("reportpositions","Start polling position every <intervalMs> ms. Usage: reportpositions <intervalMs>",[](const std::vector<std::string>& params) {reportPositionsCommand(params);}));

    return commands;
}
