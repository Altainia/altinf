# ── Stage 1: build Wt 4.13.1 from source ─────────────────────────────────────
# This layer is cached after the first build. Updates to altinf source skip it.
FROM ubuntu:24.04 AS wt-builder

ARG WT_VERSION=4.13.1
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    gcc-14 g++-14 \
    libboost-all-dev libssl-dev libsqlite3-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/* \
    && ln -sf /usr/lib/x86_64-linux-gnu/libssl.so    /usr/lib/libssl.so \
    && ln -sf /usr/lib/x86_64-linux-gnu/libcrypto.so /usr/lib/libcrypto.so

# Ubuntu 24.04's build-essential pins GCC 13, which lacks std::ranges::to
# (libstdc++ implements P1206R7 only in GCC 14+). Default to GCC 14 so the
# whole image — Wt, alt, and altinf — builds with C++23 ranges::to support.
ENV CC=gcc-14 CXX=g++-14

RUN git clone --depth 1 --branch ${WT_VERSION} \
        https://github.com/emweb/wt.git /wt-src

RUN cmake -S /wt-src -B /wt-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_EXAMPLES=OFF \
        -DENABLE_LIBWTTEST=OFF \
        -DCONNECTOR_FCGI=OFF \
        -DENABLE_SSL=ON \
    && cmake --build /wt-build --parallel "$(nproc)" \
    && cmake --install /wt-build


# ── Stage 2: build altinf ─────────────────────────────────────────────────────
FROM wt-builder AS app-builder

ARG SASS_VERSION=1.99.0
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcmark-dev curl xz-utils sudo \
    && rm -rf /var/lib/apt/lists/*

# dart-sass standalone binary (only needed at build time to compile SCSS)
RUN curl -fsSL "https://github.com/sass/dart-sass/releases/download/${SASS_VERSION}/dart-sass-${SASS_VERSION}-linux-x64.tar.gz" \
    | tar -xz -C /opt
ENV PATH="/opt/dart-sass:${PATH}"

# alt library — installed before source copy so this layer is cached
RUN git clone --depth 1 https://github.com/Altainia/alt.git /alt-src \
    && cmake -S /alt-src -B /alt-src/build/local \
        -DCMAKE_BUILD_TYPE=Release \
        -DALT_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build /alt-src/build/local --parallel "$(nproc)" \
    && cmake --install /alt-src/build/local \
    && rm -rf /alt-src

COPY . /src
# posts/ is gitignored; create it so the copy_resources target doesn't fail
RUN mkdir -p /src/posts

RUN cmake -S /src -B /src/build \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /src/build --parallel "$(nproc)"


# ── Stage 3: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-filesystem1.83.0 \
    libboost-thread1.83.0 \
    libboost-date-time1.83.0 \
    libboost-program-options1.83.0 \
    libboost-regex1.83.0 \
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
COPY docker-entrypoint.sh /app/docker-entrypoint.sh
RUN chmod +x /app/docker-entrypoint.sh

# /data  → volume for SQLite DB + posts/
VOLUME ["/data"]

EXPOSE 8080

# The entrypoint builds the runtime config (injecting Google OAuth properties
# from ALTINF_GOOGLE_* env vars when present) before launching altinf.
ENTRYPOINT ["/app/docker-entrypoint.sh"]
