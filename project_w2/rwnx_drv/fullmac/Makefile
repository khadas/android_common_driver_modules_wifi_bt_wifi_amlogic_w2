# Enable A-MSDU support (need FW support)
## Select this if FW is compiled with AMSDU support
CONFIG_RWNX_SPLIT_TX_BUF ?= y
## Select this TO send AMSDU
CONFIG_RWNX_AMSDUS_TX ?= y

# Enable BFMER support (need FW support)
CONFIG_RWNX_BFMER ?= y

# Support of MU-MIMO transmission (need FW support)
ifeq ($(CONFIG_RWNX_BFMER), y)
CONFIG_RWNX_MUMIMO_TX ?= n
else
CONFIG_RWNX_MUMIMO_TX = n
endif

# Enable suspend mode
CONFIG_RWNX_SUSPEND ?= n

# Enable handling of radar event
CONFIG_RWNX_RADAR ?= y

# Enable HW queue for Broadcast/Multicast traffic (need FW support)
CONFIG_RWNX_BCMC ?= y

# Enable Monitor+Data interface support (need FW support)
CONFIG_RWNX_MON_DATA ?= n

# extra DEBUG config
CONFIG_RWNX_SW_PROFILING ?= n
CONFIG_RWNX_DBG ?= n

#USB interface
CONFIG_RWNX_USB_MODE ?= n

obj-m := w2.o
w2-y := rwnx_msg_tx.o          \
               rwnx_msg_rx.o          \
               rwnx_utils.o           \
               rwnx_cmds.o            \
               rwnx_irqs.o            \
               rwnx_cfgfile.o         \
               rwnx_strs.o            \
               rwnx_rx.o              \
               rwnx_tx.o              \
               rwnx_txq.o             \
               rwnx_main.o            \
               rwnx_mod_params.o      \
               rwnx_mesh.o            \
               rwnx_platform.o        \
               rwnx_dini.o            \
               ipc_host.o             \
               rwnx_tdls.o            \
               hal_desc.o             \
               rwnx_iwpriv_cmds.o

obj-m += w2_usb.o
w2_usb-objs += usb_common.o

obj-m += w2_pci.o
w2_pci-objs := rwnx_pci.o \
                rwnx_v7.o

w2-$(CONFIG_RWNX_RADAR)       += rwnx_radar.o
w2-$(CONFIG_DEBUG_FS)         += rwnx_debugfs.o
w2-$(CONFIG_DEBUG_FS)         += rwnx_fw_dump.o
w2-$(CONFIG_DEBUG_FS)         += rwnx_fw_trace.o
w2-$(CONFIG_NL80211_TESTMODE) += rwnx_testmode.o
w2-$(CONFIG_RWNX_BFMER)       += rwnx_bfmer.o
w2-$(CONFIG_RWNX_MUMIMO_TX)   += rwnx_mu_group.o

ccflags-y := -DCONFIG_RWNX_FULLMAC
ifneq ($(CUR_DIR),)
src :=$(CUR_DIR)/fullmac
endif
ccflags-y += -I$(src)
ccflags-y += -I$(src)/..
ccflags-y += -I$(src)/../../common
ccflags-$(CONFIG_RWNX_RADAR) += -DCONFIG_RWNX_RADAR
ccflags-$(CONFIG_RWNX_MON_DATA) += -DCONFIG_RWNX_MON_DATA
ccflags-$(CONFIG_RWNX_BFMER) += -DCONFIG_RWNX_BFMER
ccflags-$(CONFIG_RWNX_SPLIT_TX_BUF) += -DCONFIG_RWNX_SPLIT_TX_BUF
ifeq ($(CONFIG_RWNX_SPLIT_TX_BUF), y)
ccflags-$(CONFIG_RWNX_AMSDUS_TX) += -DCONFIG_RWNX_AMSDUS_TX
endif
ccflags-$(CONFIG_RWNX_DBG) += -DCONFIG_RWNX_DBG
ccflags-$(CONFIG_RWNX_SW_PROFILING) += -DCONFIG_RWNX_SW_PROFILING
ccflags-$(CONFIG_RWNX_MUMIMO_TX) += -DCONFIG_RWNX_MUMIMO_TX
ccflags-$(CONFIG_RWNX_SUSPEND) += -DCONFIG_RWNX_SUSPEND

ifeq ($(CONFIG_RWNX_MUMIMO_TX), y)
ccflags-y += -DCONFIG_USER_MAX=2
else
ccflags-y += -DCONFIG_USER_MAX=1
endif

ifeq ($(CONFIG_RWNX_BCMC), y)
ccflags-y += -DNX_TXQ_CNT=5
else
ccflags-y += -DNX_TXQ_CNT=4
endif

ccflags-$(CONFIG_RWNX_USB_MODE) += -DCONFIG_RWNX_USB_MODE

# For old kernel (<=3.19)
ifeq ($(shell test $(VERSION) -lt 4 -a "$(CONFIG_VENDOR_RWNX)" = y ; echo $$?),0)
ccflags-y += -DCONFIG_VENDOR_RWNX_VHT_NO80
endif

quiet_cmd_genvers = GENVERSION $@
      cmd_genvers = ($(if $(KBUILD_EXTMOD),,$(srctree)/)$(src)/../mkvers.sh rwnx_version_gen.h $@)

$(obj)/rwnx_main.o: $(obj)/rwnx_version_gen.h

# To avoid always regenerating rwnx_version_gen.h (i.e. depend on FORCE), add
# dependency on all object (which already include dependency on headers files).
# Still Cannot add dependency on rwnx_main.o otherwise this will create circular
# dependency, so instead add dependency only on rwnx_main.c. This will fail to
# regenerate rwnx_version_gen.h if dependency of only rwnx_main.c is changed.
# This is OK as, currently, the only such dependency is rwnx_version.h
$(obj)/rwnx_version_gen.h: $(addprefix $(obj)/,$(filter-out rwnx_main.o, $(w2-y))) $(src)/rwnx_main.c
	$(call cmd,genvers)

clean-files := rwnx_version_gen.h
