FROM ubuntu:22.04 AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        pkg-config \
        libpcap-dev \
        libsqlite3-dev \
        qtbase5-dev \
        qtdeclarative5-dev \
        qtquickcontrols2-5-dev \
        qml-module-qtquick-controls2 \
        qml-module-qtquick-dialogs \
        qml-module-qtquick-layouts \
        qml-module-qtquick-window2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
    && cmake --build build --parallel

FROM ubuntu:22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        iputils-ping \
        libpcap0.8 \
        libsqlite3-0 \
        libqt5widgets5 \
        libqt5qml5 \
        libqt5quick5 \
        libqt5quickcontrols2-5 \
        qml-module-qtquick-controls2 \
        qml-module-qtquick-dialogs \
        qml-module-qtquick-layouts \
        qml-module-qtquick-window2 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /usr/sbin/nologin asset

RUN mkdir -p /work/data /work/logs /data && chown -R asset:asset /work /data

COPY --from=build /src/build/asset-discovery /usr/local/bin/asset-discovery
COPY --from=build /src/build/asset-discovery-gui /usr/local/bin/asset-discovery-gui
COPY --from=build /src/build/asset-capture /usr/local/bin/asset-capture
COPY --from=build --chown=asset:asset /src/configs /work/configs

USER asset
WORKDIR /work

ENTRYPOINT ["asset-discovery"]
