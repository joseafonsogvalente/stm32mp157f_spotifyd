# Spotifyd systemd Service

Systemd service for automatically starting `spotifyd` on the STM32MP157F-DK2.

The service starts spotifyd at boot and outputs audio through the board's ALSA device `hw:0,0`.

## Installation

Copy the service file to the board:

```bash
/etc/systemd/system/spotifyd.service
```

Make sure the spotifyd binary is located at:

```text
/home/root/spotifyd/spotifyd
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
--device-name STM32
--device-type speaker
--bitrate 320
```

The service automatically restarts if spotifyd exits unexpectedly.
