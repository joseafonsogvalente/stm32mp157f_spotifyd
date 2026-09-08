# Network configuration

The STM32MP157F-DK2 has onboard Wi-Fi. The OpenSTLinux image used by this project does not include NetworkManager, so `nmcli` is not used.

Wi-Fi is configured using:

* `wpa_supplicant` — handles Wi-Fi authentication and association
* `systemd-networkd` — handles DHCP and network configuration
* `systemd` — starts the Wi-Fi connection automatically at boot

The required configuration files are stored in this repository under:

```text
network/
├── 51-wireless.network
└── wpa_supplicant-wlan0.conf
```

---

## Check the Wi-Fi interface

On the STM32, verify that the wireless interface is available:

```bash
ip link
```

The expected interface is:

```text
wlan0
```

It can also be checked with:

```bash
iw dev
```

Expected output contains something similar to:

```text
Interface wlan0
    type managed
```

---

## Check that wpa_supplicant is available

Verify that `wpa_supplicant` is installed:

```bash
which wpa_supplicant
```

Expected:

```text
/usr/sbin/wpa_supplicant
```

---

# Deploy the network configuration

The configuration files in this repository can be copied directly to the appropriate locations on the STM32.

From the development machine:

```bash
scp network/wpa_supplicant-wlan0.conf \
    root@<STM32-IP>:/etc/wpa_supplicant/
```

and:

```bash
scp network/51-wireless.network \
    root@<STM32-IP>:/etc/systemd/network/
```

Alternatively, copy both files while logged into the STM32 using `scp`, USB storage, or another deployment mechanism.

The final locations on the target should be:

```text
/etc/wpa_supplicant/wpa_supplicant-wlan0.conf
/etc/systemd/network/51-wireless.network
```

---

## Protect the Wi-Fi configuration

The `wpa_supplicant` configuration contains the Wi-Fi credentials.

On the STM32:

```bash
chmod 600 /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

The configuration file should not be made publicly readable.

> **Security note:** Do not commit a configuration containing your real Wi-Fi password to a public repository. The repository should contain either a template configuration or a configuration using a generated PSK without exposing the plaintext password.

---

# `wpa_supplicant` configuration

The file:

```text
network/wpa_supplicant-wlan0.conf
```

is used by:

```text
wpa_supplicant@wlan0.service
```

The filename is important because `wlan0` identifies the wireless interface used by this systemd service.

A typical configuration looks like:

```text
ctrl_interface=/var/run/wpa_supplicant

network={
        ssid="YOUR_SSID"
        psk=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
}
```

The `psk` value should preferably be generated using:

```bash
wpa_passphrase "YOUR_SSID" "YOUR_WIFI_PASSWORD"
```

rather than storing the plaintext password.

---

# `systemd-networkd` configuration

The file:

```text
network/51-wireless.network
```

configures `systemd-networkd` to obtain an IPv4 address using DHCP.

Its contents are:

```ini
[Match]
Name=wlan0

[Network]
DHCP=ipv4
```

This means that once `wlan0` has successfully associated with the Wi-Fi network, `systemd-networkd` will request an IP address from the DHCP server, normally your home router.

---

# Enable Wi-Fi at boot

Enable the `wpa_supplicant` instance for `wlan0`:

```bash
systemctl enable wpa_supplicant@wlan0.service
```

Enable `systemd-networkd`:

```bash
systemctl enable systemd-networkd.service
```

Start both services immediately:

```bash
systemctl restart wpa_supplicant@wlan0.service
systemctl restart systemd-networkd.service
```

---

# Verify the Wi-Fi connection

Check the `wpa_supplicant` service:

```bash
systemctl status wpa_supplicant@wlan0.service
```

It should show:

```text
Active: active (running)
```

Check the Wi-Fi association:

```bash
iw wlan0 link
```

A successful connection should show something similar to:

```text
Connected to XX:XX:XX:XX:XX:XX
SSID: MyHomeWiFi
```

# Check `systemd-networkd`

The network state can be inspected with:

```bash
networkctl status wlan0
```

Also check:

```bash
systemctl status systemd-networkd.service
```

For troubleshooting, inspect the logs:

```bash
journalctl -u wpa_supplicant@wlan0.service
```

and:

```bash
journalctl -u systemd-networkd.service
```

For live logs:

```bash
journalctl -fu wpa_supplicant@wlan0.service
```
