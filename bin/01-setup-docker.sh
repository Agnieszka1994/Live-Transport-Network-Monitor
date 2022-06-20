#!/bin/bash -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${DIR}/..

docker build . -t live_transport_network_monitor -f Dockerfile.base

docker build . -t live_transport_network_monitor -f Dockerfile.dev