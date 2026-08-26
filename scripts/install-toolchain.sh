wget https://github.com/axgdev/frog-toolchain/releases/download/v1.3.2/toolchain-stable-static-x86_64-gcc16.2.0-binutils2.47-newlib4.6.0.20260123.tar.xz
tar -xf toolchain-stable-static-x86_64-gcc16.2.0-binutils2.47-newlib4.6.0.20260123.tar.xz
rm -rf toolchain-stable-static-x86_64-gcc16.2.0-binutils2.47-newlib4.6.0.20260123.tar.xz
sudo mkdir -p /opt/frog-toolchain/mipsel-mti-elf_stable
sudo mv mipsel-mti-elf/* /opt/frog-toolchain/mipsel-mti-elf_stable/
rm -rf mipsel-mti-elf
