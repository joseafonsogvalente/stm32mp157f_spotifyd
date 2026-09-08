# Spotifyd on STM32MP157F-DK2

Run [spotifyd](https://github.com/Spotifyd/spotifyd) as a Spotify Connect speaker on the **STM32MP157F-DK2**, using the board's onboard audio codec and 3.5 mm headphone output.

The target system is **ST OpenSTLinux**, running on the STM32MP157F-DK2's ARM Cortex-A7 processor.

The main purpose of this repository is to document the process of cross-compiling spotifyd for the STM32MP157F-DK2 and resolving the OpenSSL dependency issue present in the pre-built ARMv7 release.

---

# Target software

The build was tested with the following environment:

| Component           | Version / value       |
| ------------------- | --------------------- |
| Board               | STM32MP157F-DK2       |
| CPU                 | ARM Cortex-A7         |
| Target architecture | ARMv7                 |
| Target ABI          | ARM hard-float        |
| `uname -m`          | `armv7l`              |
| OS                  | ST OpenSTLinux Weston |
| OpenSTLinux         | 5.0.15                |
| Kernel              | Linux 6.6             |
| Yocto               | Scarthgap             |
| spotifyd            | v0.4.2                |
| Audio backend       | ALSA                  |
| Audio codec         | CS42L51               |

# Why build spotifyd from source?

spotifyd provides pre-built Linux ARM binaries. However, the ARMv7 release binary used during testing requires the following OpenSSL library:

```text
libssl.so.1.1
```

Running the pre-built binary on the OpenSTLinux target results in:

```text
./spotifyd: error while loading shared libraries:
libssl.so.1.1: cannot open shared object file:
No such file or directory
```

The OpenSTLinux target does not provide this library.

Instead of installing an obsolete OpenSSL 1.1 runtime on the target, spotifyd is built from source using the ARMv7 cross-compilation toolchain.

OpenSSL is statically linked into the resulting binary.

This means the resulting spotifyd executable does not require `libssl.so.1.1` or another OpenSSL shared library at runtime.

---

# Cross-compilation

The build is performed on an x86-64 development machine.

A Debian Bookworm Docker container is used to provide a reproducible ARMv7 cross-compilation environment.

The container provides:

* ARMv7 cross compiler
* ARMv7 ALSA development libraries
* ARM OpenSSL development libraries
* Rust
* Cargo
* Clang / libclang
* CMake
* native build tools
* ARM binutils

This avoids having to install the complete ARM cross-compilation environment directly on the development machine.

---

# Docker build environment

The Docker image is based on Debian Bookworm.

Build the image with:

```bash
docker build -t spotifyd-armv7 .
```

The Dockerfile configures:

```text
ARMv7 hard-float
armv7-unknown-linux-gnueabihf
```

as the target architecture.

The ARM cross compiler used is:

```text
arm-linux-gnueabihf-gcc
```

---

## Important Docker configuration

The following environment variables configure the ARM linker:

```dockerfile
ENV CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABIHF_LINKER=arm-linux-gnueabihf-gcc
ENV CC_armv7_unknown_linux_gnueabihf=arm-linux-gnueabihf-gcc
ENV CXX_armv7_unknown_linux_gnueabihf=arm-linux-gnueabihf-g++
```

The ARMv7 pkg-config environment is configured with:

```dockerfile
ENV PKG_CONFIG_ALLOW_CROSS=1
ENV PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig
```

---

# Static OpenSSL

The most important part of the Docker configuration is the OpenSSL setup:

```dockerfile
ENV OPENSSL_STATIC=1
ENV OPENSSL_LIB_DIR=/usr/lib/arm-linux-gnueabihf
ENV OPENSSL_INCLUDE_DIR=/usr/include
```

This instructs the `openssl-sys` Rust dependency to use the ARM OpenSSL libraries and statically link them.

The Docker image therefore installs the ARM OpenSSL development package:

```text
libssl-dev:armhf
```

The resulting static libraries are:

```text
/usr/lib/arm-linux-gnueabihf/libssl.a
/usr/lib/arm-linux-gnueabihf/libcrypto.a
```

This is what allows the resulting spotifyd binary to run without requiring an OpenSSL shared library on the STM32 target.

---

# Build spotifyd

The repository targets spotifyd **v0.4.2**.

The spotifyd source should be checked out at the `spotifyd-v0.4.2` tag:

```bash
git clone --branch spotifyd-v0.4.2 --depth 1 \
    https://github.com/spotifyd/spotifyd.git
```

Verify the version:

```bash
cd spotifyd
git describe --tags
```

Expected:

```text
v0.4.2
```

---

## Build command

From the root of the spotifyd source tree:

```bash
docker run --rm \
    -v "$PWD:/src:Z" \
    spotifyd-armv7 \
    cargo build \
        --release \
        --locked \
        --target armv7-unknown-linux-gnueabihf \
        --no-default-features \
        --features alsa_backend
```

### Fedora / SELinux note

The `:Z` suffix on the volume mount is important when using Docker on Fedora with SELinux enabled:

```bash
-v "$PWD:/src:Z"
```

It allows the Docker container to access the mounted source tree.

---

# Build output

After a successful build, the binary is located at:

```text
target/armv7-unknown-linux-gnueabihf/release/spotifyd
```

The build should finish with something similar to:

```text
Finished `release` profile [optimized] target(s) in ...
```

---

# Verify the binary

Before copying the binary to the STM32, verify its architecture:

```bash
file target/armv7-unknown-linux-gnueabihf/release/spotifyd
```

Expected output should identify the binary as an ARM 32-bit hard-float executable, for example:

```text
ELF 32-bit LSB pie executable, ARM, EABI5, hard-float
```

This confirms that the binary was built for the STM32MP157F's ARMv7 hard-float userspace.

---

# Verify OpenSSL dependencies

The binary can be inspected with:

```bash
readelf -d target/armv7-unknown-linux-gnueabihf/release/spotifyd | grep NEEDED
```

The important point is that the output should **not** contain:

```text
libssl.so.1.1
libssl.so.3
libcrypto.so.3
```

In particular, the original problem:

```text
libssl.so.1.1
```

should no longer be a runtime dependency.

This confirms that OpenSSL was statically linked.

---

# Deploy to the STM32MP157F-DK2

Copy the resulting binary to the board.

For example:

```bash
scp target/armv7-unknown-linux-gnueabihf/release/spotifyd \
    root@<STM32-IP>:/tmp/spotifyd
```

Then connect to the board:

```bash
ssh root@<STM32-IP>
```

Make sure the binary is executable:

```bash
chmod +x /tmp/spotifyd
```

Test that it runs:

```bash
/tmp/spotifyd --help
```

If the help output is displayed, the ARMv7 binary is executing correctly on the target.

---

# ALSA audio

Before testing spotifyd, verify that the STM32 audio hardware is available.

## List ALSA playback devices

Run:

```bash
aplay -l
```

On the STM32MP157F-DK2, the relevant device appears as the CS42L51 codec:

```text
card 0: STM32MP15DK [STM32MP15-DK]
    device 0: ... cs42l51-hifi
```

The board can also be inspected with:

```bash
cat /proc/asound/cards
```

Expected:

```text
0 [STM32MP15DK]: STM32MP15-DK - STM32MP15-DK
```

---

## List ALSA device names

Run:

```bash
aplay -L
```

This shows the ALSA devices and plugins available on the target.

The hardware device used by this project is:

```text
hw:0,0
```

which corresponds to the CS42L51 audio codec.

---

# Test the headphone output

Before involving Spotifyd, verify that the ALSA audio path works independently.

Run:

```bash
speaker-test -D hw:0,0 -c 2 -t sine
```

You should hear a test tone through the STM32MP157F-DK2's 3.5 mm headphone output.

If this works, the following components are confirmed:

```text
Linux
  ↓
ALSA
  ↓
STM32 audio driver
  ↓
CS42L51 codec
  ↓
3.5 mm headphone output
```

Stop the test with:

```text
Ctrl+C
```

---

# Run spotifyd

Once ALSA playback has been confirmed, spotifyd can be started manually.

The working command is:

```bash
./spotifyd \
    --no-daemon \
    --backend alsa \
    --device hw:0,0 \
    --device-name "STM32 Spotify" \
    --bitrate 320 \
    -v
```

### Parameters

| Option                          | Purpose                                |
| ------------------------------- | -------------------------------------- |
| `--no-daemon`                   | Keep spotifyd attached to the terminal |
| `--backend alsa`                | Use ALSA for audio output              |
| `--device hw:0,0`               | Use the CS42L51 ALSA device            |
| `--device-name "STM32 Spotify"` | Name shown in Spotify Connect          |
| `--bitrate 320`                 | Request 320 kbps audio                 |
| `-v`                            | Enable verbose logging                 |

---

# Spotify Connect

With spotifyd running, open Spotify on another device using the same Spotify account/network.

Open the Spotify device selector:

**Connect to a device**

The STM32 should appear as:

```text
STM32 Spotify
```

Select it and start playback.

Audio should be output through the STM32MP157F-DK2's 3.5 mm headphone connector.

At this point the complete audio path is:

```text
Spotify
   │
   │ Wi-Fi / Spotify Connect
   ▼
STM32MP157F-DK2
   │
   ▼
spotifyd / librespot
   │
   ▼
ALSA
   │
   ▼
CS42L51
   │
   ▼
3.5 mm output
```

---

# `hw` vs `plughw`

The initial working configuration uses:

```text
hw:0,0
```

ALSA also provides a `plughw` device:

```text
plughw:0,0
```

`plughw` allows ALSA to perform format and sample-rate conversion when necessary.

For a more flexible configuration, `plughw:0,0` can therefore be tested:

```bash
./spotifyd \
    --no-daemon \
    --backend alsa \
    --device plughw:0,0 \
    --device-name "STM32 Spotify" \
    --bitrate 320 \
    -v
```

The currently confirmed working configuration is:

```text
hw:0,0
```

---

# Configuration file

The next step is to move the command-line configuration into a spotifyd configuration file.

For example:

```text
/etc/spotifyd/spotifyd.conf
```

A configuration can be structured as:

```toml
[global]
backend = "alsa"
device = "hw:0,0"
device_name = "STM32 Spotify"
device_type = "speaker"
bitrate = 320
volume_controller = "alsa"
no_daemon = true
```

The configuration file should be tested before creating a system service.

> Note: command-line execution is currently the known-good configuration. The configuration-file behaviour should be verified independently before relying on it for automatic startup.

# References

* [spotifyd](https://github.com/Spotifyd/spotifyd)
* [spotifyd releases](https://github.com/Spotifyd/spotifyd/releases)
* [spotifyd cross-compilation documentation](https://docs.spotifyd.rs/installation/cross-compilation.html)
* [spotifyd source installation](https://docs.spotifyd.rs/installation/source.html)
* [ST OpenSTLinux](https://www.st.com/en/embedded-software/stm32mp1dev.html)
* [STM32MP157F-DK2](https://www.st.com/en/evaluation-tools/32f746gdiscovery.html)
