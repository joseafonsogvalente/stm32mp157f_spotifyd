FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture armhf && \
    apt-get update && \
    apt-get install -y \
        curl \
        git \
        build-essential \
        gcc-arm-linux-gnueabihf \
        g++-arm-linux-gnueabihf \
        pkg-config \
        libasound2-dev:armhf \
        libssl-dev:armhf \
        libclang-dev \
        clang \
        cmake \
        python3 \
        perl \
        ca-certificates \
        file \
        binutils-arm-linux-gnueabihf \
        && \
    rm -rf /var/lib/apt/lists/*

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --profile minimal

ENV PATH="/root/.cargo/bin:${PATH}"

RUN rustup target add armv7-unknown-linux-gnueabihf

ENV CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABIHF_LINKER=arm-linux-gnueabihf-gcc
ENV CC_armv7_unknown_linux_gnueabihf=arm-linux-gnueabihf-gcc
ENV CXX_armv7_unknown_linux_gnueabihf=arm-linux-gnueabihf-g++

ENV PKG_CONFIG_ALLOW_CROSS=1
ENV PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig

ENV OPENSSL_STATIC=1
ENV OPENSSL_LIB_DIR=/usr/lib/arm-linux-gnueabihf
ENV OPENSSL_INCLUDE_DIR=/usr/include

WORKDIR /src

CMD ["bash"]
