# Home Assistant Shortcuts for Pebble

A Pebble watch app that lets you trigger Home Assistant scripts directly from your wrist. Configure up to 8 shortcuts, each with a custom name, script, and icon.

## Features

- Up to 8 configurable shortcuts to Home Assistant scripts
- 8 icon choices per shortcut (Star, Unlock, Lock, Light, Garage, Scene, Power, Home)
- Animated success/error feedback with vibration
- Settings page on your phone via the Pebble app (powered by Clay)
- Token and URL configured once, persisted across app restarts
- Supports Aplite, Basalt, and Chalk platforms

## Setup

### Prerequisites

- A Pebble watch paired with the Pebble app on your phone
- A Home Assistant instance accessible from your phone's network
- A [Long-Lived Access Token](https://www.home-assistant.io/docs/authentication/) from Home Assistant

### Install

1. Build the app:
   ```bash
   pebble build
   ```
2. Install to your watch:
   ```bash
   pebble install --phone <your-phone-ip>
   ```

### Configure

1. Open the Pebble app on your phone
2. Find "Home Assistant Shortcuts" and tap the gear icon
3. Enter your Home Assistant base URL (e.g. `https://your-hostname/api/services/script/`)
4. Enter your Long-Lived Access Token
5. Add shortcuts: each needs a **Name** (shown on watch), **Script** (the Home Assistant script name), and an **Icon**
6. Tap "Save Settings"

### Example

If you have a Home Assistant script called `unlock_front_door`, configure a shortcut with:
- **Name**: Unlock Front Door
- **Script**: unlock_front_door
- **Icon**: Unlock

Selecting it on the watch will POST to `https://your-hostname/api/services/script/unlock_front_door`.

## How it works

```
Watch (C)                    Phone (JS)                  Home Assistant
    |                            |                            |
    |-- select shortcut -------->|                            |
    |                            |-- POST /api/services/ ---->|
    |                            |<---- 200 OK ---------------|
    |<-- result (Done!) ---------|                            |
    |   [vibrate + green popup]  |                            |
```

The watch app sends the shortcut index to the phone via AppMessage. The phone's JS runtime makes the HTTP POST to Home Assistant with Bearer token authentication, then sends the result back to the watch.

## Building from source

Requires the [Pebble SDK](https://developer.rebble.io/developer.pebble.com/sdk/index.html).

```bash
pebble build
```

## License

MIT
