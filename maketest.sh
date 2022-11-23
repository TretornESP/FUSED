rm -rf ./test
mkdir ./test
dd if=/dev/zero of=./test/raw.img bs=512 count=204800
cp ./test/raw.img ./test/ext2.img
mkfs.ext2 ./test/ext2.img