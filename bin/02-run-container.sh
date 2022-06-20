#!/bin/bash -ex

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd ${DIR}/../

CMD=${1}
OPTS=""
DOCKER_USER=admin
DOCKER_PROJECT_MOUNT_PATH=/home/${DOCKER_USER}/live_transport_network_monitor

if [[ -z "${CMD}" ]]; then
    CMD='bash'
fi

docker run ${OPTS} -it \
    --rm \
    --name live_transport_network_monitor \
    -v ${DIR}/../:${DOCKER_PROJECT_MOUNT_PATH} \
    live_transport_network_monitor \
    ${CMD}
