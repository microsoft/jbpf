FROM mcr.microsoft.com/mirror/docker/library/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
ENV CPP_CHECK=1

# --- Bootstrap system certificates before switching to HTTPS ---
RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Use HTTPS for all Ubuntu mirrors to avoid port 80 firewall issues
RUN sed -i 's|http://archive.ubuntu.com|https://archive.ubuntu.com|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|https://security.ubuntu.com|g' /etc/apt/sources.list

RUN echo "*** Installing packages" && \
    apt update && \
    apt install -y --no-install-recommends \
        cmake \
        build-essential \
        libboost-dev \
        git \
        libboost-program-options-dev \
        gcovr \
        doxygen \
        libboost-filesystem-dev \
        libasan6 \
        python3 \
        clang-format \
        cppcheck \
        clang \
        gcc-multilib \
        libyaml-cpp-dev && \
    apt clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /jbpf
COPY . /jbpf

ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
