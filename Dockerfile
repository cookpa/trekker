FROM ubuntu:focal as builder

RUN apt-get update \
    && apt-get install -y \
                    build-essential \
                    ca-certificates \
                    curl \
                    zlib1g-dev

RUN curl -OL https://github.com/Kitware/CMake/releases/download/v3.23.2/cmake-3.23.2-linux-x86_64.sh \
    && chmod +x cmake-3.23.2-linux-x86_64.sh \
    && mkdir -p /opt/cmake/bin \
    && ./cmake-3.23.2-linux-x86_64.sh --skip-license --prefix="/opt/cmake" \
    && rm -f cmake-3.23.2-linux-x86_64.sh 

ENV PATH="/opt/cmake/bin:$PATH"

COPY . /opt/trekker/src

RUN mkdir -p /opt/trekker/build \
    && cd /opt/trekker/build \
    && cmake \
         -DBuild_Python3_WRAPPER=OFF \
         -DCMAKE_BUILD_TYPE=Release \
         /opt/trekker/src \
    && cmake --build . --target install --parallel

FROM ubuntu:focal

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
                    zlib1g-dev

COPY --from=builder /opt/trekker/build/install /opt/trekker
COPY --from=builder /opt/trekker/src /opt/trekker/src

LABEL maintainer="Philip A Cook" \
      description="This is a Docker container for Trekker https://dmritrekker.github.io/index.html"
    
ENV PATH="/opt/trekker/bin:$PATH"

ENTRYPOINT ["trekker"]
