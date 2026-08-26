#!/bin/sh
./scripts/install-dependencies.sh
./scripts/install-toolchain.sh
./scripts/buildSfClean.sh
find . -name "*.asd" -print0 | xargs -0 zip nightly.zip
