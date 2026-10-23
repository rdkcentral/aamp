# AAMP JavaScript Bindings

Creates JavaScript binding for AAMP UVE APIs. This binding can be used with javascript files to call AAMP UVE APIs.

## Instructions

AAMP JS Binding library and jsbinding binary is created as part of install-aamp.sh.

Seperately, these can be built on XCode with target aampjsbindings (library) and uveExecuter (binary)

## Usage

Once the binary file for binding is created, run a javascript file using

```
./uveExecuter <path to js file>
```

Scripts that need a library loaded first (e.g. a UMD bundle such as PlayerPlatform JS) can pass it as a preload.
Preload files are evaluated in order at global scope before the last file, which is run as the test script:

```
./uveExecuter <preload.js> [<preload.js> ...] <path to js file>
```

Besides `AAMP`, `AAMPMediaPlayer` and `XREReceiver`, uveExecuter provides a minimal browser environment:
`window`/`self`, `console`, `setTimeout`/`setInterval`, `fetch`, `XMLHttpRequest`, `URL`/`URLSearchParams`,
`performance`, `localStorage`, `Event`/`EventTarget` and an inert `document`. DOM APIs never render anything.
