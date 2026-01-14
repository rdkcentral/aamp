#pragma once

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include "InterfacePlayerRDK.h"

class Command {
public:
    std::string matchValue;
    std::string helpText;
    std::function<void(const std::vector<std::string>&)> action;

    Command(const std::string& value, const std::string& help, std::function<void(const std::vector<std::string>&)>&& func)
        : matchValue(value), helpText(help), action(std::move(func)) {}
};

class CommandExecutor {
private:
    std::queue<std::pair<std::function<void(const std::vector<std::string>&)>, std::vector<std::string>>> commandQueue;
    std::condition_variable cv;
    std::mutex queueMutex;
    bool stopThread = false;
    std::thread workerThread;

    void threadFunction();

public:
    CommandExecutor();
    ~CommandExecutor();
    void addCommand(const std::function<void(const std::vector<std::string>&)>& command, const std::vector<std::string>& params);
    void stop();
};

void displayHelp(const std::map<std::string, Command>& commands);

// Command wrappers for every InterfacePlayerRDK command we want to expose
void createPipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setPlayerNameCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setPreferredDRMCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void initializeSourceForPlayerCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void configurePipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setPauseOnStartCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setPlayBackRateCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setAudioVolumeCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setupStreamCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void pauseCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void resumeInjectorCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void stopCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void flushCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void flushTrackCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void isPipelinePausedCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void enablePendingPlayStateCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void setPendingSeekCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void triggerEventCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void notifyEOSCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void notifyFirstFrameCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void endOfStreamReachedCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void sendHelperCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void waitForSourceSetupCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void timerAddCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void timerRemoveCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void timerIsRunningCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void dumpDiagnosticsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void enableGstDebugLoggingCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void initializePlayerGstreamerPluginsCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void tearDownStreamCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void destroyPipelineCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void removeProbesCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);
void clearProtectionEventCommand(InterfacePlayerRDK& player, const std::vector<std::string>& params);

// Register all commands
std::map<std::string, Command> initializeCommands(CommandExecutor& executor, InterfacePlayerRDK& player);
