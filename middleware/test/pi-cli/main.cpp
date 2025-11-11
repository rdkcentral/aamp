#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <vector>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <gst/gst.h>
#include "InterfacePlayerRDK.h"
#include "commandProcessing.h"

// Helper function to convert a string to lowercase
std::string toLowerCase(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

// Helper function to parse user input into command and parameters
std::pair<std::string, std::vector<std::string>> parseInput(const std::string& input) {
    std::istringstream stream(input);
    std::string command;
    std::vector<std::string> params;

    stream >> command; // Extract the command
    command = toLowerCase(command); // Convert the command to lowercase

    std::string param;
    while (stream >> param) {
        params.push_back(param); // Extract parameters
    }

    return { command, params };
}

// Globals for readline tab completion
std::map<std::string, Command>* globalCommands = nullptr;

// Function for readline tab completion
char* commandCompletion(const char* text, int state) {
    static std::vector<std::string> matches;
    static size_t matchIndex = 0;

    if (state == 0) {
        matches.clear();
        matchIndex = 0;

        std::string input(text);
        for (const auto& [key, command] : *globalCommands) {
            if (key.find(input) == 0) {
                matches.push_back(key);
            }
        }
    }

    if (matchIndex < matches.size()) {
        return strdup(matches[matchIndex++].c_str());
    }

    return nullptr;
}

// Function to initialize readline completion
void initializeReadline() {
    rl_attempted_completion_function = [](const char* text, int start, int end) -> char** {
        if (start == 0) {
            return rl_completion_matches(text, commandCompletion);
        }
        return nullptr;
    };
}

int picli_main(int argc, char *argv[]) {
    // Create the command executor
    CommandExecutor executor;
    InterfacePlayerRDK player;
    
    // Install required CB functions so we don't crash
    player.TearDownCallback([](bool, int){ /* no-op */ });
    player.RegisterNeedDataCb([](int){ /* no-op */ });

    // Initialize commands
    std::map<std::string, Command> commands = initializeCommands(executor, player);
    globalCommands = &commands; // Set global pointer for tab completion

    // Initialize readline
    initializeReadline();

    // Main loop for user input
    while (true) {
        // Print a prompt to the user
        char* input = readline("Please enter a command (type 'help' for available commands): ");

        // If user input is error/EOF, exit
        if (!input) {
            break;
        }

        // If empty string, don't process
        if (strlen(input) == 0) {
             continue;
        }

        // Add input to history
        add_history(input);

        // Parse the input into command and parameters
        auto [commandInput, params] = parseInput(input);

        // Free readline input buffer
        free(input);

        // Check if the command exists
        auto it = commands.find(commandInput);
        if (it != commands.end()) {
            // Add the command's action to the executor with parameters
            executor.addCommand(it->second.action, params);
        } else {
            // Command not found
            std::cout << "Unknown command '" << commandInput << "'. Type 'help' to see available commands.\n";
        }
    }
    // Cleanly stop the command processing thread.
    executor.stop();

    return 0;
}

int
main (int argc, char *argv[])
{
#if defined(__APPLE__)
  return gst_macos_main ((GstMainFunc) picli_main, argc, argv, NULL);
#else
  return picli_main (argc, argv);
#endif
}
