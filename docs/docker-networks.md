# Docker networking test 

To build the image, run the following command:

```bash
docker image build -t realtime-network/vplc-netperf .
```

## Example with host network

To run the container with the host network, run the following command:

```bash
docker run -it --rm --cpuset-cpus 2 --privileged --network=host --name=host-test realtime-network/vplc-netperf
```

If you also want to map volume to the container, e.g. 'data' folder in the current directory, you can run the following command:

```bash
docker run -it --rm --cpuset-cpus 2 --privileged --network=host --name=host-test -v $(pwd)/data:/data realtime-network/vplc-netperf
```

## Example with veth network

## Example with macvlan network

Macvlan networks allow you to assign a MAC address to a container, making it appear as a physical device on your network. The Docker daemon routes traffic to containers by their MAC addresses. Using the macvlan driver is sometimes the best choice when dealing with legacy applications that expect to be directly connected to the physical network, rather than routed through the Docker host's network stack.

To configure a macvlan network, you need to create a macvlan network first. You can do this with the following command:

```bash
docker network create            \
        -d macvlan               \
        --subnet=10.0.0.0/24     \
        --gateway=10.0.0.1       \
        --ip-range=10.0.0.128/26 \
        -o parent=enp5s0         \
        macvlan-net
        
    #  --aux-address="my-router=192.168.32.129" \
```

You can run the container with the macvlan network with the following command:

```bash
docker run -it --rm --cpuset-cpus 2 --privileged --network=macvlan-net --name=macvlan-test -v $(pwd)/data:/data realtime-network/vplc-netperf
```

## Example with passthrough network

Passthrough networks allow you to connect a container directly to a physical network interface. This is useful when you want to have a container that is directly connected to the physical network, rather than routed through the Docker host's network stack.

Launch a container without a network:

```bash
docker run -it --rm --cpuset-cpus 2 --privileged --network=none --name=passthru-test -v $(pwd)/data:/data realtime-network/vplc-netperf
```