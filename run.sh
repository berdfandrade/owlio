#!/usr/bin/env bash
# Executa o Owlio usando o Qt baixado em ~/Qt (sem precisar de sudo).
# Se você instalar "libxcb-cursor0" pelo apt, a linha do LD_LIBRARY_PATH deixa de ser necessária.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
QT_DIR="${QT_DIR:-$HOME/Qt/6.8.3/gcc_64}"
export LD_LIBRARY_PATH="$QT_DIR/lib:$HOME/Qt/extra/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/build/owlio" "$@"
