Samsung Galaxy Ace: Unpack and repack boot.img, editing boot logo
Ketut P. Kumajaya ketut.kumajaya @ xda-developers.com

Unpack:
$ mkdir -p unpack
$ tools/unpackbootimg -i source_img/boot.img -o unpack

Extracting boot.img-ramdisk.gz
$ mkdir -p boot
$ cd boot
$ gzip -dc ../unpack/boot.img-ramdisk.gz | cpio -i
$ cd ../

Packing a new ramdisk:
$ tools/mkbootfs boot | gzip > unpack/boot.img-ramdisk-new.gz

Create a new boot.img:
$ mkdir -p target_img
$ tools/mkbootimg --kernel unpack/boot.img-zImage --ramdisk unpack/boot.img-ramdisk-new.gz -o target_img/boot.img --base `cat unpack/boot.img-base`

Convert rle to png:
$ tools/from565 -rle < logo/COOPER.rle > logo/COOPER.raw
$ convert -size 320x480 -depth 8 rgb:logo/COOPER.raw logo/COOPER.png

Convert png to rle:
$ convert -depth 8 logo/COOPER-new.png rgb:logo/COOPER-new.raw
$ tools/to565 -rle < logo/COOPER-new.raw > logo/COOPER-new.rle

