# Weston Configuration

This directory contains the customized Weston configuration used on the **STM32MP157F-DK2** running OpenSTLinux.

## Files

* `weston.ini` — Modified Weston configuration used on the target.
* `weston.ini.bk` — Backup of the original Weston configuration.

## What was changed

The default OpenSTLinux Weston configuration displays an ST background image and a desktop panel.

The modified configuration:

* Removes the Weston top panel.
* Removes the clock.
* Disables the default background image.
* Sets the Weston background to black.
* Keeps the screen idle timeout disabled.
* Disables Weston animations.

The result is a clean **black screen** after Weston starts, which can later be used as the background for the Qt/QML application.

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

To restore the original configuration:

```bash
cp weston.ini.bk /etc/xdg/weston/weston.ini
systemctl restart weston-graphical-session.service
```
