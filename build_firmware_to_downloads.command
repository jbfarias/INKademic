#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PIO_BIN="${HOME}/.venvs/platformio-crossink/bin/pio"
BUILD_ENV="${1:-default}"
BUILD_DIR="${PROJECT_DIR}/.pio/build/${BUILD_ENV}"
DEST_DIR="${HOME}/Downloads/CrossInk-Academic"

if [[ ! -f "${PROJECT_DIR}/platformio.ini" ]]; then
  echo "Erro: execute este script dentro da raiz do projeto CrossInk."
  exit 1
fi

if [[ ! -x "${PIO_BIN}" ]]; then
  echo "Erro: PlatformIO não encontrado em: ${PIO_BIN}"
  exit 1
fi

cd "${PROJECT_DIR}"

echo "Compilando ambiente: ${BUILD_ENV}"
CI=1 "${PIO_BIN}" run -e "${BUILD_ENV}"

FIRMWARE_BIN="$(find "${BUILD_DIR}" -maxdepth 1 -type f \( -name 'firmware-*.bin' -o -name 'firmware.bin' \) -print -quit)"

if [[ -z "${FIRMWARE_BIN}" ]]; then
  echo "Erro: a compilação terminou, mas nenhum firmware*.bin foi encontrado em:"
  echo "  ${BUILD_DIR}"
  exit 1
fi

mkdir -p "${DEST_DIR}"

STAMP="$(date +%Y%m%d-%H%M%S)"
SOURCE_NAME="$(basename "${FIRMWARE_BIN}" .bin)"
OUTPUT_PATH="${DEST_DIR}/${SOURCE_NAME}-${BUILD_ENV}-${STAMP}.bin"

cp "${FIRMWARE_BIN}" "${OUTPUT_PATH}"

echo
echo "Firmware copiado para:"
echo "  ${OUTPUT_PATH}"
echo
echo "Tamanho:"
ls -lh "${OUTPUT_PATH}"
echo
echo "SHA-256:"
shasum -a 256 "${OUTPUT_PATH}"

open -R "${OUTPUT_PATH}" 2>/dev/null || open "${DEST_DIR}" 2>/dev/null || true
