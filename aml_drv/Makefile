AML_VERS_NUM=v1.3.0_20230330-w2 driver:3a03a10b4b7fd446894bcbabdb5da4c6e401b59d
#KERNELDIR=/usr/lib/modules/5.8.0-63-generic/build
PWD=$(shell pwd)
ifeq ($(ARCH),)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
else ifeq ($(ARCH),arm)
SYSTEM_TYPE = SYSTEM32
CROSS_COMPILE ?= arm-linux-gnueabihf-
EXTRA_CFLAGS += -DSYSTEM32
else ifeq ($(ARCH),arm64)
SYSTEM_TYPE = SYSTEM64
CROSS_COMPILE ?= aarch64-linux-gnu-
EXTRA_CFLAGS += -DSYSTEM64
endif


ifeq ($(PROJ_NAME),p-amlogic)
SRCTOP ?= /proj/vlsi.wifi/p-amlogic
KERNELDIR ?= $(SRCTOP)/out/target/product/newton/obj/KERNEL_OBJ
EXTRA_CFLAGS += -DSYSTEM64
else ifeq ($(PROJ_NAME),yeke)
SRCTOP ?= /proj/vlsi.wifi/yeke_64bit
KERNELDIR ?= $(SRCTOP)/out/target/product/p281/obj/KERNEL_OBJ
EXTRA_CFLAGS += -DSYSTEM64
else ifeq ($(PROJ_NAME),9-xiaomi)
SRCTOP ?= /proj/vlsi.wifi/9-xiaomi
KERNELDIR ?= $(SRCTOP)/out/target/product/aquaman/obj/KERNEL_OBJ
EXTRA_CFLAGS += -DSYSTEM64
else ifeq ($(PROJ_NAME),gva)
SRCTOP ?= /proj/vlsi.wifi/gva
KERNELDIR ?= $(SRCTOP)/kernel-4.9/common
EXTRA_CFLAGS += -DSYSTEM64
else
ifeq ($(SYSTEM_TYPE),)
ARCH = arm
CROSS_COMPILE = arm-linux-gnueabihf-
EXTRA_CFLAGS += -DSYSTEM32
endif
SRCTOP ?= /proj/vlsi.wifi/p212_32bit
KERNELDIR ?= $(SRCTOP)/out/target/product/ampere/obj/KERNEL_OBJ
endif

CC        ?= $(CROSS_COMPILE)gcc
STRIP     ?= $(CROSS_COMPILE)strip

M ?= $(shell pwd)
ifneq ($(KERNEL_SRC),)
CUR_DIR  := $(KERNEL_SRC)/$(M)
else
CUR_DIR  := $(M)
endif
EXT_INCS := $(CUR_DIR)/../common
INCS     := -I$(CUR_DIR) -I$(EXT_INCS)
KERNEL_SRC ?= $(KERNELDIR)
TARGET = vlsicomm

#
# WAITING FOR KCONFIG {
#
CONFIG_AML_SOFTMAC ?= n
CONFIG_AML_FULLMAC ?= m
CONFIG_AML_FHOST ?= n

#
# DEBUG OPTIONS
CONFIG_AML_UM_HELPER_DFLT ?= "/usr/bin/aml_umh.sh"

#
# FW ARCH:
CONFIG_AML_SDM ?= n
CONFIG_AML_TL4 ?= n

# IPC version
CONFIG_AML_OLD_IPC ?= n

# Support of P2P DebugFS for enabling/disabling NoA and OppPS
CONFIG_AML_P2P_DEBUGFS ?= y

# Need open it when use FPGA pcie version
CONFIG_AML_FPGA_PCIE ?= n

# Need close it when the vendor/device id is written to efuse
CONFIG_AML_PLATFORM_AH212 ?= y

# Support to use preallocing static buffer for wlan driver reloadable
CONFIG_AML_PREALLOC_BUF_STATIC ?= y

# Support to use preallocing buffer for debug SKB alloc failed
CONFIG_AML_PREALLOC_BUF_SKB ?= n

# Config to enable WFA certification mode
CONFIG_AML_WFA_CERT_MODE ?= n

# Config to enable Power_Save mode
CONFIG_AML_POWER_SAVE_MODE ?= n

# Support to use tasks to process IRQ Handlers
CONFIG_AML_USE_TASK ?= y

# Support recovery function
CONFIG_AML_RECOVERY ?= y
# } // WAITING FOR KCONFIG
#

subdir-ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_AML_DEBUGFS
subdir-ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_AML_UM_HELPER_DFLT=\"$(CONFIG_AML_UM_HELPER_DFLT)\"
subdir-ccflags-$(CONFIG_AML_P2P_DEBUGFS) += -DCONFIG_AML_P2P_DEBUGFS
subdir-ccflags-$(CONFIG_AML_FPGA_PCIE) += -DCONFIG_AML_FPGA_PCIE
subdir-ccflags-$(CONFIG_AML_PLATFORM_AH212) += -DCONFIG_AML_PLATFORM_AH212
subdir-ccflags-$(CONFIG_AML_PREALLOC_BUF_STATIC) += -DCONFIG_AML_PREALLOC_BUF_STATIC
subdir-ccflags-$(CONFIG_AML_PREALLOC_BUF_SKB) += -DCONFIG_AML_PREALLOC_BUF_SKB
subdir-ccflags-$(CONFIG_AML_WFA_CERT_MODE) += -DCONFIG_AML_WFA_CERT_MODE
subdir-ccflags-$(CONFIG_AML_POWER_SAVE_MODE) += -DCONFIG_AML_POWER_SAVE_MODE
subdir-ccflags-$(CONFIG_AML_USE_TASK) += -DCONFIG_AML_USE_TASK
subdir-ccflags-$(CONFIG_AML_RECOVERY) += -DCONFIG_AML_RECOVERY

# FW VARS
subdir-ccflags-y += -DNX_VIRT_DEV_MAX=4
subdir-ccflags-y += -DNX_REMOTE_STA_MAX=10
subdir-ccflags-y += -DNX_MU_GROUP_MAX=62
subdir-ccflags-y += -DNX_TX_MAX_RATES=4
subdir-ccflags-y += -DNX_CHAN_CTXT_CNT=3

# FW ARCH:
subdir-ccflags-$(CONFIG_AML_SDM) += -DCONFIG_AML_SDM
subdir-ccflags-$(CONFIG_AML_TL4) += -DCONFIG_AML_TL4
subdir-ccflags-$(CONFIG_AML_OLD_IPC) += -DCONFIG_AML_OLD_IPC

ifeq ($(CONFIG_ANDROID_GKI),y)
subdir-ccflags-y += -DCONFIG_ANDROID_GKI
endif

obj-$(CONFIG_AML_FULLMAC) += fullmac/

all: modules

modules clean:
	@$(PWD)/mklink.sh
	$(MAKE)  CUR_DIR=$(CUR_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(M) $@
	@$(PWD)/mklink.sh clean
#ifneq (${OUT_DIR},)
#	$(CROSS_COMPILE)strip --strip-unneeded ${OUT_DIR}/$(M)/$(TARGET).ko
#else
#	$(STRIP) -S $(TARGET).ko
#endif

modules_install:
	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;

