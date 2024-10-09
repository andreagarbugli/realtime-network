######################################################
# Build Image
FROM ubuntu:jammy AS build-env

# Install build dependencies
RUN apt-get update -y
RUN apt-get install -y iproute2 build-essential vim

# Set the working directory
WORKDIR /vplc-tests

# Copy the source int "code" dir  to the container
COPY . .

# Run using bash
RUN ./build.sh

######################################################
# Final Image
FROM ubuntu:jammy

RUN apt-get update -y
RUN apt-get install iproute2 iputils-ping vim ethtool -y 

# Set the working directory
WORKDIR /vplc-tests

# Copy the compiled binary from the first step
COPY --from=build-env /vplc-tests/build/main .
COPY --from=build-env /vplc-tests/run.sh .

# Set the command to run when the container starts
CMD ["/bin/bash"]