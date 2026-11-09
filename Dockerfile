FROM alpine:3.24.4@sha256:f59abe656b692d901e184be3445e084786602108599d2a51d229c0f5a48c71bf AS builder

RUN apk add --no-cache \
    build-base \
    libx11-dev \
    bash \
    && rm -rf /var/cache/apk/*

WORKDIR /src

COPY xsetwall.c stb_image.h x ./

RUN ./x build

FROM alpine:3.24.4@sha256:f59abe656b692d901e184be3445e084786602108599d2a51d229c0f5a48c71bf AS runner

RUN apk add --no-cache \
    libx11 \
    && rm -rf /var/cache/apk/*

COPY --from=builder /src/xsetwall /usr/local/bin/xsetwall

CMD ["xsetwall"]