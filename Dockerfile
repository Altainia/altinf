# ── Stage 1: build Wt 4.13.1 from source ─────────────────────────────────────
# This layer is cached after the first build. Updates to altinf source skip it.
FROM ubuntu:22.04 AS wt-builder

ARG WT_VERSION=4.13.1
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git \
    libboost-all-dev libssl-dev libsqlite3-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch ${WT_VERSION} \
        https://github.com/emweb/wt.git /wt-src

RUN cmake -S /wt-src -B /wt-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_EXAMPLES=OFF \
        -DENABLE_LIBWTTEST=OFF \
        -DCONNECTOR_FCGI=OFF \
    && cmake --build /wt-build --parallel "$(nproc)" \
    && cmake --install /wt-build


# ── Stage 2: build altinf ─────────────────────────────────────────────────────
FROM wt-builder AS app-builder

ARG SASS_VERSION=1.99.0
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcmark-dev curl xz-utils \
    && rm -rf /var/lib/apt/lists/*

# dart-sass standalone binary (only needed at build time to compile SCSS)
RUN curl -fsSL "https://github.com/sass/dart-sass/releases/download/${SASS_VERSION}/dart-sass-${SASS_VERSION}-linux-x64.tar.gz" \
    | tar -xz -C /opt
ENV PATH="/opt/dart-sass:${PATH}"

COPY . /src
# posts/ is gitignored; create it so the copy_resources target doesn't fail
RUN mkdir -p /src/posts

RUN cmake -S /src -B /src/build \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /src/build --parallel "$(nproc)"


# ── Stage 3: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-date-time1.74.0 \
    libboost-program-options1.74.0 \
    libboost-regex1.74.0 \
    libssl3 \
    libsqlite3-0 \
    libcmark0.30.2 \
    zlib1g \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Wt runtime libraries (built from source, not in apt)
COPY --from=wt-builder /usr/local/lib/libwt*.so* /usr/local/lib/
RUN ldconfig

COPY --from=app-builder /src/build/altinf   /app/altinf
COPY --from=app-builder /src/build/resources /app/resources
COPY wt_config.xml /app/wt_config.xml

# /data  → volume for SQLite DB + posts/
# /certs → volume for cert.pem, key.pem, dh.pem
VOLUME ["/data", "/certs"]

EXPOSE 8080 8443

ENTRYPOINT ["/app/altinf", \
    "--config",        "/app/wt_config.xml", \
    "--docroot",       "/app/resources", \
    "--approot",       "/data", \
    "--http-address",  "0.0.0.0", "--http-port",  "8080", \
    "--https-address", "0.0.0.0", "--https-port", "8443", \
    "--ssl-certificate",  "/certs/cert.pem", \
    "--ssl-private-key",  "/certs/key.pem", \
    "--ssl-tmp-dh",       "/certs/dh.pem"]
