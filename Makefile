descend = \
	+mkdir -p $(OUTPUT)$(1) && \
	$(MAKE) $(COMMAND_O) subdir=$(if $(subdir),$(subdir)/$(1),$(1)) $(PRINT_DIR) -C $(1) $(2)

kernel_dir = /lib/modules/`uname -r`/build
module_dir = `pwd`/kernel-module/igb

all: igb daemons_all avb-user

clean: igb_clean daemons_all_clean avb-user_clean


help:
	@echo 'Possible targets:'
	@echo ''
	@echo '  igb               - igb kernel module'
	@echo ''
	@echo '  daemons_all       - build all daemons (mrpd gptp maap shaper)'
	@echo '  mrpd              - mrpd daemon'
	@echo '  gptp              - gptp daemon for linux'
	@echo '  maap              - maap daemon'
	@echo '  shaper            - shaper daemon for linux'
	@echo ''
	@echo '  avb-user          - avb userspace client'
	@echo ''
	@echo 'Cleaning targets:'
	@echo ''
	@echo '  all of the above with the "_clean" string appended cleans'
	@echo '    the respective build directory.'
	@echo '  clean: a summary clean target to clean _all_ folders'
	@echo ''

igb: FORCE
	cd $(module_dir)
	make -C $(kernel_dir) M=$(module_dir)

igb_clean:
	$(call descend,kernel-module/igb/,clean)

mrpd:
	$(call descend,daemons/$@)

mrpd_clean:
	$(call descend,daemons/mrpd/,clean)

gptp:
	ARCH=I210
	$(call descend,daemons/$@/linux/build/)

gptp_clean:
	$(call descend,daemons/gptp/linux/build/,clean)

maap:
	$(call descend,daemons/$@/linux/build/)

maap_clean:
	$(call descend,daemons/maap/linux/build/,clean)

shaper:
	$(call descend,daemons/$@)

shaper_clean:
	$(call descend,daemons/shaper/,clean)

daemons_all: mrpd maap gptp shaper

daemons_all_clean: mrpd_clean gptp_clean maap_clean shaper_clean

avb-user:
	$(call descend,tools/)

avb-user_clean:
	$(call descend,tools/,clean)

install: all
	mkdir -p $(HOME)/.avb
	cp daemons/mrpd/mrpd $(HOME)/.avb
	cp daemons/gptp/linux/build/obj/daemon_cl $(HOME)/.avb
	cp daemons/maap/linux/build/maap_daemon $(HOME)/.avb
	cp daemons/shaper/shaper_daemon $(HOME)/.avb
	cp tools/avb-user $(HOME)/.avb
	cp kernel-module/igb/igb_avb.ko $(HOME)/.avb
	cp avb_up.sh /usr/local/bin
	cp avb_down.sh /usr/local/bin

.PHONY: FORCE
