# Embedded

A collection of my embedded projects. Each project lives in its own folder under `projects/` and is a self-contained PlatformIO project with its own `platformio.ini`, sources and README.

## Projects

| Project | Board | Description |
| --- | --- | --- |
| [blink](projects/blink) | Freenove ESP32-S3 WROOM | Two LEDs with button-switched blinking modes |

## Layout

```
embedded/
├── README.md
└── projects/
    └── <project>/
        ├── platformio.ini
        ├── include/
        ├── lib/
        ├── src/
        ├── test/
        └── README.md
```

## Working with a project

Open the project folder (for example `projects/blink`) in VS Code with the PlatformIO extension, or use the CLI from inside it:

```sh
cd projects/blink
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial monitor
```

## Adding a new project

Create it under `projects/`:

```sh
mkdir projects/<name> && cd projects/<name>
pio project init --board <board_id>
```

Then add a row to the table above.
