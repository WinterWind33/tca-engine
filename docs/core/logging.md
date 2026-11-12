# Logging

The engine writes one log per run, `engine.log`, in a per-user folder named after the game, never
next to the executable or in the working directory. A Steam install folder is often read-only for
the player, and the working directory is whatever the launcher picked.

```cpp
application.set_app_name("My Game")         // names the log folder
           .set_company_name("My Studio")   // optional, adds a folder above it
           .set_logic(std::make_unique<my_game_logic>());
application.run();                          // the log starts here
```

## Where the logs live

For the game's support page ("please send us your log"):

| Platform | Folder                                                                  |
|----------|-------------------------------------------------------------------------|
| Windows  | `%LOCALAPPDATA%\<company>\<game>\logs`                                  |
| Linux    | `$XDG_STATE_HOME/<company>/<game>/logs` (default `~/.local/state/...`)  |
| macOS    | `~/Library/Logs/<company>/<game>`                                       |

Without a company name, the `<company>` folder is left out. `core::logging::get_log_folder()`
returns the folder in use, e.g. to open it from an in-game "report a bug" button.

Each run starts a fresh `engine.log`. The previous runs are kept as `engine.1.log` (the most recent)
up to `engine.5.log` (the oldest), so a crash log survives the next few launches:

```
launch:   engine.log ──► engine.1.log ──► engine.2.log ──► … ──► engine.5.log ──► deleted
```

The log starts with a header naming the game, the operating system and the UTC start time.

## Rules

- `run()` names the log folder, and the log file opens on the first line logged. A game can take
  its loggers whenever it likes, but a line it logs **before** `run()` opens the log with no folder
  named, and the whole run then goes to the debugger output and stderr. A game that needs to log
  earlier calls `core::logging::set_log_folder()` itself, with
  `platform::get_preferred_log_folder()`, at the very start of `main()`.
- A Win32 application without an app name logs under the default name `TCA Application`; set one
  so players find the log under the game's name. A console application without one, or any
  application whose folder can't be created, logs to the debugger output (on Windows) and stderr
  only. Losing the log never stops the game from starting.
- `core::logging::get_logger(name)` gives a game a logger of its own that writes to the same file.
  `set_log_level()` sets the level of every engine logger (default `info`; `parse_log_level()` reads
  one from a command line or a settings file). Warnings and worse are flushed straight away.
- `core::logging::add_sink()` sends every engine logger's output somewhere else too, such as an
  in-game console; `remove_sink()` stops it.
