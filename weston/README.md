# Weston Configuration

This directory contains the customized Weston configuration used on the **STM32MP157F-DK2** running OpenSTLinux.

## Files

* `weston.ini` — Modified Weston configuration used on the target.
* `weston.ini.bk` — Backup of the original Weston configuration.

## What was changed

The default OpenSTLinux Weston configuration displays an ST background image, a desktop panel, and automatically starts the ST demo launcher.

The modified configuration:

* Removes the Weston top panel.
* Removes the clock.
* Disables the default background image.
* Sets the Weston background to black.
* Keeps the screen idle timeout disabled.
* Disables Weston animations.
* Prevents the ST demo launcher from starting automatically.
* Keeps the Weston startup audio initialization script enabled.

The result is a clean **black screen** after Weston starts, with the custom Spotify GTK UI running without the ST demo menu appearing over it.

This provides a clean graphical environment that can later be used as the background for the Qt/QML application.

## ST Demo Launcher

OpenSTLinux uses `/usr/bin/weston-start` to execute startup scripts from:

```text
/usr/local/weston-start-at-startup/
```

The script executes **every file present in this directory** after Weston starts. Because of this, simply removing the executable permission or renaming the demo launcher to something such as `.disabled` is not sufficient.

The unwanted ST demo launcher was removed from the directory:

```text
/usr/local/weston-start-at-startup/start_up_demo_launcher.sh
```

The directory should contain only the startup scripts that are intentionally required, for example:

```text
/usr/local/weston-start-at-startup/
└── audio.sh
```

Do **not** place disabled or backup copies of startup scripts in this directory, as `weston-start` will attempt to execute them.

## Installation

Copy the modified configuration to the target:

```bash
scp weston.ini root@<TARGET_IP>:/etc/xdg/weston/weston.ini
```

Alternatively, copy it directly on the target:

```bash
cp weston.ini /etc/xdg/weston/weston.ini
```

Then restart Weston:

```bash
systemctl restart weston-graphical-session.service
```

### Removing the ST demo launcher

If the ST demo launcher is present on the target, remove it from the Weston startup directory:

```bash
rm /usr/local/weston-start-at-startup/start_up_demo_launcher.sh
```

Check the directory afterwards:

```bash
ls -l /usr/local/weston-start-at-startup/
```

Only the required startup scripts should remain.

## Configuration location

On the target, Weston reads:

```text
/etc/xdg/weston/weston.ini
```

## Backup

The original configuration is kept in:

```text
weston.ini.bk
```

To restore the original Weston configuration:

```bash
cp weston.ini.bk /etc/xdg/weston/weston.ini
systemctl restart weston-graphical-session.service
```

Note that restoring `weston.ini` alone does **not** restore the ST demo launcher. The launcher is controlled separately through:

```text
/usr/local/weston-start-at-startup/
```
