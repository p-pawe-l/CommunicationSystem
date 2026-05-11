FROM alpine:3.20

WORKDIR /workspace

RUN apk add --no-cache \
    build-base \
    clang \
    clang-extra-tools \
    cmake \
    git \
    make \
    ninja \
    py3-pybind11 \
    python3 \
    python3-dev

COPY . .

CMD ["sh"]
