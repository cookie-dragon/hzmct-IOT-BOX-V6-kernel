#make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- tisdk_am335x-evm_defconfig
make -j8  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage 
cp -f  arch/arm/boot/uImage ./
cp uImage /home/stdve/tftpboot

