#!/bin/sh

# USAGE:
# build.sh <platform> <dist-suffix>

# Provide one of the following as argument to adjust platform
# pico (default)
# pico2

FLAVOR=pico
DIST_PREFIX=
CONFIGURATION=Release

if [ $# -gt 0 ]; then
FLAVOR=$1
shift
fi

if [ $# -gt 0 ]; then
if [ -z "${DIST_PREFIX}" ]; then
DIST_PREFIX="${1}_"
fi
shift
fi

if [ $# -gt 0 ]; then
CONFIGURATION=$1
shift
fi

BUILD_DIR="build-${FLAVOR}-${CONFIGURATION}"
DIST_DIR="./dist"
GCC="/usr/bin/arm-none-eabi-gcc"
GPP="/usr/bin/arm-none-eabi-g++"

# Force the elf and uf2 binary files to always be regenerated on build
# (this is so old uf2 files don't pile up in dist directory)
rm ${BUILD_DIR}/src/*/*/*.elf
rm ${BUILD_DIR}/src/*/*/*.uf2

mkdir -p ./${BUILD_DIR}
BUILD_OUTPUT_FILE="./${BUILD_DIR}/build.out"

echo "\$cmake configure" > ${BUILD_OUTPUT_FILE}

cmake \
    --no-warn-unused-cli \
    -DPICO_BOARD=${FLAVOR} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
    -DCMAKE_BUILD_TYPE:STRING=${CONFIGURATION} \
    -DCMAKE_C_COMPILER:FILEPATH=${GCC} \
    -DCMAKE_CXX_COMPILER:FILEPATH=${GPP} \
    -DDREAMCAST_CONTROLLER_USB_PICO_TEST:BOOL=FALSE \
    -S. \
    -B./${BUILD_DIR} \
    -G "Unix Makefiles" | tee -a ${BUILD_OUTPUT_FILE}

STATUS=$?
if [ $STATUS -ne 0 ]; then
    echo "CMake returned error exit code: ${STATUS}"
    echo "Exiting"
    exit $STATUS
fi

echo "\$cmake build" >> ${BUILD_OUTPUT_FILE}

cmake \
    --build ${BUILD_DIR} \
    --config ${CONFIGURATION} \
    --target all \
    -j 10 | tee -a ${BUILD_OUTPUT_FILE}

STATUS=$?
if [ $STATUS -ne 0 ]; then
    echo "CMake returned error exit code: ${STATUS}"
    echo "Exiting"
    exit $STATUS
fi

mkdir -p ${DIST_DIR}
for file in ${BUILD_DIR}/src/*/*/*.uf2
do
    filename=$(basename $file)
    cp -v -- "$file" "${DIST_DIR}/${DIST_PREFIX}${filename}"
done
