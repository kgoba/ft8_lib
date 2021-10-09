SUBDIRS := libs
SUBDIRS += apps

all install:
	echo $(SUBDIRS)
	@for i in `echo $(SUBDIRS)`; do \
			$(MAKE) -C $$i $@ || exit 1; \
	done


clean mrproper:
	@for i in `echo $(SUBDIRS)`; do \
			$(MAKE) -C $$i $@ || exit 1; \
	done

