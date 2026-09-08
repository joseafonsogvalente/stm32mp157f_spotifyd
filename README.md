# stm32mp157f_spotifyd
Spotifyd running on STM32MP157F-DK2 board


Check the releases page and download the armv7 version
https://github.com/spotifyd/spotifyd/releases

Build the Docker image:
```
docker build -t spotifyd-armv7 .
```
And run the same build:
```
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

After it finishes, check the binary
Run:
```
file target/armv7-unknown-linux-gnueabihf/release/spotifyd
```
Ideally you'll get something like:
```
ELF 32-bit LSB pie executable, ARM, EABI5, hard-float ...
```
Then check whether OpenSSL is still a runtime dependency:
```
readelf -d target/armv7-unknown-linux-gnueabihf/release/spotifyd | grep NEEDED
```
You do not want to see:
```
libssl.so.1.1
libssl.so.3
libcrypto.so.3
```

After the cross-compilation the binary should be at:
```
target/armv7-unknown-linux-gnueabihf/release/spotifyd
```

Copy the binary to the STM32 board

### Identify the ALSA devices on the board

```
aplay -L
cat /proc/asound/cards
```
