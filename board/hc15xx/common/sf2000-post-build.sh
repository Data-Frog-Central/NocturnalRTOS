#!/bin/bash
script_dir=$(dirname "$(realpath "$0")")
source $script_dir/post-build.sh
gcc -o $script_dir/crc $script_dir/crc.c
chmod +x $script_dir/crc
exec $script_dir/crc ${IMAGES_DIR}/${app}.bin ${IMAGES_DIR}/${app}.asd 
rm $script_dir/crc
