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

FROM build AS test

RUN ctest --test-dir build --output-on-failure

FROM ubuntu:22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ARG PNAD_UID=1000
ARG PNAD_GID=1000

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        iproute2 \
        iputils-ping \
        libcap2-bin \
        libpcap0.8 \
        libsqlite3-0 \
        libgl1 \
        libegl1 \
        libxkbcommon-x11-0 \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-render-util0 \
        libxcb-xinerama0 \
        libqt5widgets5 \
        libqt5qml5 \
        libqt5quick5 \
        libqt5quickcontrols2-5 \
        qml-module-qtquick-controls2 \
        qml-module-qtquick-dialogs \
        qml-module-qtquick-layouts \
        qml-module-qtquick-window2 \
        qtwayland5 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid "${PNAD_GID}" asset \
    && useradd --uid "${PNAD_UID}" --gid "${PNAD_GID}" --create-home --shell /usr/sbin/nologin asset

RUN mkdir -p /work/configs /work/logs /data /out /samples \
    && chown -R asset:asset /work /data /out /samples

COPY --from=build /src/build/asset-discovery /usr/local/bin/asset-discovery
COPY --from=build /src/build/asset-discovery-gui /usr/local/bin/asset-discovery-gui
COPY --from=build /src/build/asset-capture /usr/local/bin/asset-capture
COPY --from=build --chown=asset:asset /src/configs /work/configs
COPY docker/pnad-gui-entrypoint.sh /usr/local/bin/pnad-gui-entrypoint

RUN chmod 0755 /usr/local/bin/pnad-gui-entrypoint

WORKDIR /work

ENV PNAD_GUI_SQLITE_PATH=/data/pnad.db \
    PNAD_GUI_PCAP_PATH=/samples/multi-asset.pcap \
    PNAD_GUI_EXPORT_DIR=/out \
    PNAD_DOCKER_RUNTIME=1 \
    QT_X11_NO_MITSHM=1 \
    QT_QUICK_BACKEND=software \
    LIBGL_ALWAYS_SOFTWARE=1

ENTRYPOINT ["pnad-gui-entrypoint"]
