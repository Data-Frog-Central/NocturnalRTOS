make clean
rm -r bl
rm -fR output/
mkdir /mnt/x/output
ln -s /mnt/x/output output
./buildSf.sh
