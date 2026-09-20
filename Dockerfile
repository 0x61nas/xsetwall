FROM alpine:3.24.2@sha256:294b683cb724975bec92580e1e685676bd4b50bda910ddb8c51d4cabeaec77e6 AS builder

RUN apk add --no-cache \
    build-base \
    libx11-dev \
    bash \
    && rm -rf /var/cache/apk/*

WORKDIR /src

COPY xsetwall.c stb_image.h x ./

RUN ./x build

FROM alpine:3.24.2@sha256:294b683cb724975bec92580e1e685676bd4b50bda910ddb8c51d4cabeaec77e6 AS runner

RUN apk add --no-cache \
    libx11 \
    && rm -rf /var/cache/apk/*

COPY --from=builder /src/xsetwall /usr/local/bin/xsetwall

CMD ["xsetwall"]