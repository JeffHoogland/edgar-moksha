# Simple Makefile for Enlightenment (edgar) gadgets

# gadget specific config
GADGET_NAME = audio
EXTRA_FILES = 


# nothing  should be changed below this point
GADGET_FILES = __init__.pyc $(GADGET_NAME).edj
prefix = $(shell pkg-config --variable=libdir enlightenment)
gadget_folder = ${prefix}/enlightenment/gadgets/$(GADGET_NAME)

.PHONY: all install clean

all: $(GADGET_FILES) $(EXTRA_FILES)

install: all
	@mkdir -p -v ${gadget_folder}
	@cp --preserve=mode -v $(GADGET_FILES) $(EXTRA_FILES) $(gadget_folder)

uninstall: all
	@rm -rfv ${gadget_folder}

clean:
	@rm -fv *.edj *.pyc


EDJE_CC = edje_cc
EDJE_FLAGS = -v -id images/ -fd fonts/

%.edj: %.edc images/*
	$(EDJE_CC) $(EDJE_FLAGS) $<
	@chmod -v og+r $@

%.pyc: %.py
	python3 -c "from py_compile import compile; compile('$<', '$@')"
