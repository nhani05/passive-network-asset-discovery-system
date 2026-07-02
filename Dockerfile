FROM ubuntu:22.04 AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        pkg-config \
        libpcap-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON \
    && cmake --build build --parallel

FROM ubuntu:22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        iputils-ping \
        libpcap0.8 \
        postgresql-client \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /usr/sbin/nologin asset

RUN mkdir -p /work/logs && chown -R asset:asset /work

COPY --from=build /src/build/asset-discovery /usr/local/bin/asset-discovery
COPY --from=build --chown=asset:asset /src/configs /work/configs

USER asset
WORKDIR /work

ENTRYPOINT ["asset-discovery"]
