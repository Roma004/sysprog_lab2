QEMU_BIN_DIR = ./qemu/build
QEMU = $(QEMU_BIN_DIR)/qemu-system-x86_64

QEMU_DRIVE_IF = /usr/share/OVMF/x64/OVMF_CODE.4m.fd
QEMU_ISO = ./archlinux-2025.10.01-x86_64.iso

QEMU_LOG_FILE = qemu_log.txt

PCIE_BAR0_FILE = ./pcie_bar0.bin
PCIE_BAR2_FILE = ./pcie_bar2.bin

QEMU_BASE_FLAGS = \
		-D $(QEMU_LOG_FILE) \
		-drive if=pflash,format=raw,readonly=on,file=$(QEMU_DRIVE_IF) \
		-smp 2,sockets=1,cores=2,threads=1 \
		-m 4G \
		-hda disk.qcow2 \
		-device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::2222-:22 \
		-monitor telnet:localhost:1234,server,nowait \
		-enable-kvm \
		-vga std

QEMU_BAR0_FLAGS=bar0-size=4K,bar0-obj=membar0
QEMU_BAR2_FLAGS=bar2-size=64K,bar2-obj=membar2
QEMU_BARS=$(QEMU_BAR0_FLAGS),$(QEMU_BAR2_FLAGS)

QEMU_TESTDEV_FLAGS=\
		-object memory-backend-file,size=4K,share=on,mem-path=$(PCIE_BAR0_FILE),id=membar0 \
		-object memory-backend-file,size=64K,share=on,mem-path=$(PCIE_BAR2_FILE),id=membar2 \
		-chardev socket,id=testdev_chr,host=127.0.0.1,port=17887,server=on,wait=off \
		-device lab2-testdev,$(QEMU_BARS),chardev-id=testdev_chr

QEMU_FLAGS = $(QEMU_BASE_FLAGS) $(QEMU_TESTDEV_FLAGS)

qemu-install:
	$(QEMU) $(QEMU_FLAGS) \
		-cdrom $(QEMU_ISO)

qemu-run:
	$(QEMU) $(QEMU_FLAGS) \
		-nographic

qemu-run-gdb:
	gdb --args $(QEMU) $(QEMU_FLAGS) -nographic
