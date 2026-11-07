FROM alpine:3.24.4@sha256:93b41ef40b1677fce8fcc64ff4af09daddfacf5040afde618bc46c1e2d1cc7d4 AS builder

RUN apk add --no-cache \
    build-base \
    libx11-dev \
    bash \
    && rm -rf /var/cache/apk/*

WORKDIR /src

COPY xsetwall.c stb_image.h x ./

RUN ./x build

FROM alpine:3.24.4@sha256:93b41ef40b1677fce8fcc64ff4af09daddfacf5040afde618bc46c1e2d1cc7d4 AS runner

RUN apk add --no-cache \
    libx11 \
    && rm -rf /var/cache/apk/*

COPY --from=builder /src/xsetwall /usr/local/bin/xsetwall

CMD ["xsetwall"]