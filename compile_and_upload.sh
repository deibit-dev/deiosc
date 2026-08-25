#!/usr/bin/env bash
# 1. Compilar (desde el directorio de este repositorio).
#    Fuera del árbol del SDK hay que indicar dónde está la plataforma:
#      SDK_PLATFORM=/ruta/logue-sdk/platform/minilogue-xd ./compile_and_upload.sh
set -e
cd "$(dirname "$0")"
SDK_PLATFORM="${SDK_PLATFORM:-}"
if [ -z "$SDK_PLATFORM" ]; then
  echo "Define SDK_PLATFORM con la ruta del SDK (ej: logue-sdk/platform/minilogue-xd)"
  exit 1
fi
python3 gen_tables.py
make PLATFORMDIR="$SDK_PLATFORM" clean && make PLATFORMDIR="$SDK_PLATFORM" && make PLATFORMDIR="$SDK_PLATFORM" install
# 2. Subir al minilogue xd (puertos SOUND detectados: in 2 / out 2)
#    Opcional: limpiar el slot antes de cargar
/home/david/dev/minilogue/logue-sdk/tools/logue-cli/logue-cli-linux64-0.07-2b/logue-cli clear -m osc -s 4 -i 2 -o 2
/home/david/dev/minilogue/logue-sdk/tools/logue-cli/logue-cli-linux64-0.07-2b/logue-cli load -u deiosc.mnlgxdunit -i 2 -o 2 -s 4
