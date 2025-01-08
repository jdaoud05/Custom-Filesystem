#!/bin/bash

IMAGE="systems-base.qcow2"

if [ ! -f "$IMAGE" ]; then
  if [ ! -f "virtualbox.box" ]; then
    echo Downloading the Vagrant box...
    wget https://app.vagrantup.com/khoury-cs3650/boxes/base-environment/versions/2021.09.12/providers/virtualbox.box
    if [ ! -f "virtualbox.box" ]; then
      echo "Download failed?"
      exit 1
    fi
  fi
  echo Preparing the QEMU image...
  tar xzf virtualbox.box
  qemu-img convert -f vmdk -O qcow2 box-disk001.vmdk "$IMAGE"
fi

qemu-system-x86_64 \
  -M q35 \
  -cpu qemu64 \
  -smp 4 \
  -m 4096 \
  -rtc clock=vm,base=localtime \
  -device virtio-gpu-pci \
  -display default,show-cursor=on \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -device intel-hda \
  -device hda-duplex \
  -vga virtio \
  -hda "$IMAGE" \
  -nic user,model=virtio,hostfwd=tcp::2200-:22 \
  -nographic
