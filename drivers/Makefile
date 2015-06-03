DRIVERS = netmap/ e1000/
MOD_LIST:= CONFIG_NETMAP=m CONFIG_E1000=m

M:=$(PWD)
# Additional compile flags (e.g. header location)
EXTRA_CFLAGS := -I$(root_dir)/drivers/netmap -I$(root_dir)/include -DCONFIG_NETMAP 

EXTRA_CFLAGS += -Wno-unused-but-set-variable

obj-m += $(O_DRIVERS)
all: build

build:
	$(MAKE) -C $(KSRC) M=$(PWD) $(MOD_LIST) \
			EXTRA_CFLAGS='$(EXTRA_CFLAGS)'	\
			O_DRIVERS="$(DRIVERS:%.c=%.o)" modules
	@ls -l `find . -name \*.ko`

clean:
	-@ $(MAKE) -C $(KSRC) M=$(PWD) clean 2> /dev/null
	-@ (rm -rf *.orig *.rej *.ko *.o .*.d \
			.tmp_versions *.mod.c modules.order \
			Module.symvers .*.cmd get-drivers )

define remote_template
$$(obj)/$(1): $$(M)/netmap/$(1:.o=.c)
	$$(call cmd,cc_o_c)
	$$(call cmd,modversions)
endef

$(foreach o,$(remoteobjs),$(eval $(call remote_template,$(o))))
