#!/usr/bin/env sh
set -eu
projectDir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
buildDir="$projectDir/build/release"
cmake -S "$projectDir" -B "$buildDir" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$buildDir" --config Release --parallel
