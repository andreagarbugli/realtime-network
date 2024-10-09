#!/bin/bash

HOST_INTERFACE=enp5s0
CONT_IFACE_NAME=enp5s0
container=passthru-test
ADDRESS=10.0.0.213

NSPID=$(docker inspect --format='{{ .State.Pid }}' $container)

echo "Moving $HOST_IFACE to container $container (PID: $NSPID)"
ip link set $HOST_INTERFACE netns $NSPID

echo "Setting up $HOST_INTERFACE in container $container"
docker exec $container ip link set dev $HOST_INTERFACE up
# ip netns exec $NSPID ip link set dev $HOST_INTERFACE up

echo "Setting up IP address $ADDRESS in container $container"
docker exec $container ip addr add $ADDRESS/24 dev $CONT_IFACE_NAME
# ip netns exec $NSPID ip addr add $ADDRESS/24 dev $HOST_INTERFACE
