GADGET_NAME = postit

EDJE_CC = edje_cc
EDJE_FLAGS = -v -id images/ -fd fonts/

.PHONY: all install clean

all: $(GADGET_NAME).edj

$(GADGET_NAME).edj: $(GADGET_NAME).edc $(GADGET_NAME).embryo images/*
	$(EDJE_CC) $(EDJE_FLAGS) $< $@

install: all
	rm -f ~/.e/e/gadgets/$(GADGET_NAME).edj
	cp $(GADGET_NAME).edj ~/.e/e/gadgets/$(GADGET_NAME).edj

uninstall: all
	rm -f ~/.e/e/gadgets/$(GADGET_NAME).edj;


clean:
	rm -f $(GADGET_NAME).edj
