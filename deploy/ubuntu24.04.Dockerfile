FROM mcr.microsoft.com/mirror/docker/library/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CPP_CHECK=1

RUN echo "*** Installing packages"
RUN apt update --fix-missing
RUN apt install -y cmake build-essential libboost-dev git libboost-program-options-dev \
    wget gcovr doxygen libboost-filesystem-dev libasan6 python3

RUN apt install -y clang-format cppcheck
RUN apt install -y clang
RUN if [ "$TARGET_ARCHITECTURE" = "amd64" ]; then \
    apt-get install -y gcc-multilib; \
  elif [ "$TARGET_ARCHITECTURE" = "arm64" ]; then \
    apt-get install -y libc6-dev; \
  fi

# Build yaml-cpp from source
RUN git clone https://github.com/jbeder/yaml-cpp.git && \
    cd yaml-cpp && \
    mkdir build && cd build && \
    cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DYAML_CPP_INSTALL=ON && \
    make -j$(nproc) && \
    make install

RUN apt-get update && apt-get install -y pkg-config

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
