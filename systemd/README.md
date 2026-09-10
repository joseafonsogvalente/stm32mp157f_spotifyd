# Spotifyd + Spotify UI systemd Services

Systemd services for automatically starting `spotifyd` and the custom GTK3 Spotify UI on the STM32MP157F-DK2.

The setup consists of:

* **spotifyd** — Spotify Connect daemon, outputting audio through ALSA `hw:0,0`
* **spotify-ui** — GTK3 graphical interface running under the Weston Wayland session

---

# Spotifyd Service

The Spotifyd service starts `spotifyd` at boot and outputs audio through the board's ALSA device `hw:0,0`.

## Installation

Make sure the spotifyd binary is located at:

```text
/home/root/spotifyd/spotifyd
```

The service file is:

```text
/etc/systemd/system/spotifyd.service
```

Reload systemd:

```bash
systemctl daemon-reload
```

Enable the service at boot:

```bash
systemctl enable spotifyd.service
```

Start it:

```bash
systemctl start spotifyd.service
```

## Check status

```bash
systemctl status spotifyd.service
```

View logs:

```bash
journalctl -u spotifyd.service -f
```

## Stop / Disable

Stop the service:

```bash
systemctl stop spotifyd.service
```

Disable automatic startup:

```bash
systemctl disable spotifyd.service
```

## Service configuration

The service runs:

```text
/home/root/spotifyd/spotifyd
```

with:

```text
--backend alsa
--device hw:0,0
--device-name STM32 Spotify
--device-type speaker
--bitrate 320
--volume-controller none
--use-mpris
--dbus-type system
```

The service automatically restarts if `spotifyd` exits unexpectedly.

---

# Spotify UI Service

The `spotify-ui` service starts the custom GTK3 Spotify interface automatically after the Weston graphical session is available.

The application connects to Spotifyd through MPRIS over the **system D-Bus** and displays the current Spotify playback information, including artwork, artist, song, playback status, progress and controls.

## Weston Configuration

The STM32MP157F-DK2 uses:

```text
weston-graphical-session.service
```

The Weston graphical session runs as:

```text
User=weston
Group=weston
```

with the Wayland socket:

```text
/run/user/1000/wayland-0
```

Therefore the GTK application runs as the `weston` user.

---

## D-Bus Configuration

Spotifyd exposes its MPRIS interface on the **system D-Bus** because it is started with:

```text
--use-mpris
--dbus-type system
```

Spotifyd's D-Bus configuration is located at:

```text
/etc/dbus-1/system.d/spotifyd.conf
```

The Spotifyd policy allows the `root` user to own and communicate with the Spotifyd MPRIS service.

However, the GTK UI runs as the `weston` user, not as `root`. Therefore an additional D-Bus policy is required to allow the UI to send MPRIS commands to Spotifyd.

The additional policy is:

```text
/etc/dbus-1/system.d/spotify-ui.conf
```

with:

```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="weston">
    <allow send_destination_prefix="org.mpris.MediaPlayer2.spotifyd"/>
  </policy>
</busconfig>
```

This gives the `weston` user permission to send messages to Spotifyd's MPRIS services without running the entire GTK application as root.

This is required for the UI controls such as:

```text
Previous
Play / Pause
Next
```

to work through MPRIS.

After changing the D-Bus configuration, restart the relevant services:

```bash
systemctl restart dbus
```

```bash
systemctl restart spotifyd.service
```

```bash
systemctl restart spotify-ui.service
```

The UI dynamically discovers the currently running Spotifyd MPRIS instance, so it continues to work even when Spotifyd receives a new dynamic D-Bus service name after restarting.

---

## Installation

The compiled UI binary is installed at:

```text
/usr/bin/spotify-ui
```

The systemd service file is:

```text
/etc/systemd/system/spotify-ui.service
```

The service uses:

```text
User=weston
Group=weston
WAYLAND_DISPLAY=wayland-0
XDG_RUNTIME_DIR=/run/user/1000
```

and starts:

```text
/usr/bin/spotify-ui
```

## Service configuration

The service starts after the Weston graphical session and Spotifyd:

```ini
[Unit]
Description=Spotify GTK UI
After=weston-graphical-session.service spotifyd.service
Requires=weston-graphical-session.service
Wants=spotifyd.service

[Service]
Type=simple
User=weston
Group=weston
Environment=WAYLAND_DISPLAY=wayland-0
Environment=XDG_RUNTIME_DIR=/run/user/1000
ExecStart=/usr/bin/spotify-ui
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

`User=` and `Group=` are used so the GTK application runs inside the Weston user's graphical environment. Systemd supports specifying the user/group identity for system services this way.

---

## Reload systemd

```bash
systemctl daemon-reload
```

Enable the UI at boot:

```bash
systemctl enable spotify-ui.service
```

Start it:

```bash
systemctl start spotify-ui.service
```

## Check status

```bash
systemctl status spotify-ui.service
```

View logs:

```bash
journalctl -u spotify-ui.service -f
```

## Stop / Disable

Stop the UI:

```bash
systemctl stop spotify-ui.service
```

Disable automatic startup:

```bash
systemctl disable spotify-ui.service
```

---

## Testing the UI manually

The UI can be tested as the Weston user with:

```bash
su -s /bin/sh weston -c 'WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 /usr/bin/spotify-ui'
```

This verifies that the application can access the Weston Wayland display before running it through systemd.

---

# Startup sequence

After boot, the intended sequence is:

```text
System boot
    │
    ├── spotifyd.service
    │       │
    │       ├── Spotify Connect
    │       ├── ALSA hw:0,0
    │       └── MPRIS / system D-Bus
    │
    └── weston-graphical-session.service
            │
            └── spotify-ui.service
                    │
                    ├── GTK3 / Wayland
                    ├── MPRIS controls
                    └── Spotify playback information
```

The UI is designed to tolerate Spotifyd becoming available after the UI starts because it periodically reconnects to the MPRIS service.

The systemd service explicitly provides the graphical environment variables because system services do not automatically inherit the complete graphical-session environment.
