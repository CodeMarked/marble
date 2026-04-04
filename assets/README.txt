Marble runtime assets (asset conditioning pipeline output)

Source art and DCC exports live outside this tree; the build copies this folder
next to the game executable so the engine can resolve `<executable>/assets`.

Override at runtime with MARBLE_ASSETS_ROOT or the marbles --assets <path> flag.
