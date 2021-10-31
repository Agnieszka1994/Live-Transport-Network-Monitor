#!/bin/bash -ex
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd ${DIR}/../build/
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
