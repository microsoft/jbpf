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
# RUN if [ "$TARGET_ARCHITECTURE" = "amd64" ]; then \
#   apt-get install -y libyaml-cpp-dev; \
# elif [ "$TARGET_ARCHITECTURE" = "arm64" ]; then \
#   apt-get install -y --no-install-recommends libyaml-cpp-dev libyaml-cpp-dev-common || true; \
# fi
RUN git clone --branch yaml-cpp-0.7.0 https://github.com/jbeder/yaml-cpp.git /tmp/yaml-cpp && \
    cd /tmp/yaml-cpp && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DYAML_CPP_BUILD_TESTS=OFF && \
    make -j$(nproc) && make install && \
    rm -rf /tmp/yaml-cpp

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
