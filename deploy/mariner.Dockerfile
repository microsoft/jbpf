FROM mcr.microsoft.com/azurelinux/base/core:3.0

COPY deploy/retry-tdnf.sh /usr/local/bin/retry-tdnf
RUN chmod 755 /usr/local/bin/retry-tdnf

RUN echo "*** Installing packages"
RUN retry-tdnf upgrade tdnf --refresh -y
RUN retry-tdnf -y update
RUN retry-tdnf -y install build-essential cmake git
RUN retry-tdnf -y install yaml-cpp-devel yaml-cpp-static boost-devel gcovr clang python3
RUN retry-tdnf -y install doxygen
## clang-format
RUN retry-tdnf -y install clang-tools-extra

WORKDIR /jbpf
COPY . /jbpf
ENTRYPOINT ["./helper_build_files/build_jbpf_lib.sh"]
