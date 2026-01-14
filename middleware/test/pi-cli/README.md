# pi-cli

`pi-cli` provides a command line interface to allow interactive execution of the InterfacePlayerRDK API.

---

## Supported Commands

Here are some of the commands supported by `pi-cli` (for a complete list, see the source code):

- **`exit`**: Exit the program. Ensures that the worker thread is stopped cleanly before exiting.
- **`help`**: Display a list of available commands and their descriptions.
- **`echo [parameters...]`**: Echoes the provided parameters back to the console.
- **`hello`**: Prints a hello world message.
- **`createpipeline <pipelineName> [pipelinePriority]`**: Create a pipeline.
- **`setplayername <name>`**: Set player name.
- **`configurepipeline <int> <int> <int> <int> <bool> <bool> <bool> <bool> <int32> <gint> <pipelineName> <int> <bool> <url>`**: Configure pipeline.
- **`setpauseonstart [true|false]`**: Enable/disable pause on start playback.
- **`pause [pause(bool)] [forceStop(bool)]`**: Pause the pipeline.
- **`resumeinjector`**: Resume injector.
- **`stop`**: Stop playback.
- **`flush`**: Flush pipeline.
- **`flushtrack <mediaType> <pos> <audioDelta> <subDelta>`**: Flush track.
- **`ispipelinepaused`**: Check if pipeline is paused.
- **`enablependingplaystate <true|false>`**: Enable pending play state.
- **`notifyfirstframe <mediaType>`**: Notify first frame.
- **`endofstreamreached <mediaType> <shouldHaltBuffering>`**: End of stream reached.
- **`sendhelper`**: Send helper event (not available in CLI).
- **`waitforsourcesetup <mediaType>`**: Wait for source setup.
- **`timeradd`**: Add timer (not available in CLI).
- **`timerremove <taskId>`**: Remove timer.
- **`initializeplayergstreamerplugins`**: Initialize Player GStreamer plugins.
- **`teardownstream <mediaType>`**: Teardown stream.
- **`destroypipeline`**: Destroy pipeline.
- **`removeprobes`**: Remove probes.
- **`clearprotectionevent`**: Clear protection event.
- **`setvideorectangle <x> <y> <width> <height>`**: Set video rectangle.
- **`injectfragment <mediaType:int> <filePath> [pts] [dts] [duration] [fragmentPTSoffset]`**: Inject fragment into player.

---

## Building the Application

Follow these steps to build `pi-cli` on macOS or Linux:

### Prerequisites

Make sure you have the following installed:
- `CMake` (version 3.10 or higher)
- A C++ compiler with support for C++17 (e.g., `clang` or `g++`)
- `readline` development libraries

### Steps to Build

1. Open a terminal.

2. Navigate to the root directory of the `middleware-player-interface` project:
   ```bash
   cd /path/to/middleware-player-interface
   ```

3. Create a build directory and navigate into it:
   ```bash
   mkdir build
   cd build
   ```

4. Enable the `pi-cli` build and generate build files:
   ```bash
   cmake .. -DBUILD_PICLI=ON
   ```

5. Build the project:
   ```bash
   make
   ```

6. The compiled executable will be located in `test/pi-cli/bin` inside the `build` folder. You can run it as follows:
   ```bash
   ./test/pi-cli/bin/pi-cli
   ```

---

## Running the Application

Once built, run the `pi-cli` executable. You will see a prompt where you can enter supported commands.

### Example Usage

```bash
$ ./test/pi-cli/bin/pi-cli
Please enter a command (type 'help' for available commands): help
Available commands:
- exit: Exit the program.
- help: Display this help message.
- echo: Echoes the provided parameters.
...
```

To exit the program, type:
```bash
exit
```

---

## Notes

- For Linux builds, the `-pthread` flag is automatically applied to ensure multithreading compatibility.
- If you encounter any issues, ensure that your development environment meets the prerequisites.

```bash
# To install CMake on macOS using Homebrew:
brew install cmake
```

---

Enjoy using `pi-cli`!

_For more details and updates, refer to the [pi-cli source and command implementation](https://github.com/rdkcentral/middleware-player-interface/tree/main/test/pi-cli/)._
