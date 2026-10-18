# Top-level build/run for yolo-soc-codesign.
# Builds Darknet for x86 and for the Zedboard's ARM Cortex-A9, and runs the
# same detections on three platforms: x86, QEMU user mode, QEMU system mode (qemu-esl)

SHELL		:= /bin/bash
.SHELLFLAGS	:= -o pipefail -c

# ---- Configuration (override on the command line, e.g. make run-x86 IMAGE=data/horses.jpg)

DARKNET		:= darknet
SDK_ENV		?= $(HOME)/eslsdk/environment-setup-cortexa9t2hf-neon-esl-linux-gnueabi
JOBS        ?= $(shell nproc)
LOGDIR    	?= logs
BOARD     	?= qemu
BOARD_DIR 	?= darknet

CFG       	?= cfg/yolov3-tiny.cfg
DATA      	?= cfg/coco.data
WEIGHTS   	?= yolov3-tiny.weights
IMAGE     	?= data/dog.jpg

WEIGHTS_URL := https://pjreddie.com/media/files/yolov3-tiny.weights
ARM_EXE     := darknet-arm
ARM_OBJ     := ./obj-arm/
LOG         := $(abspath $(LOGDIR))
TAG         := $(notdir $(basename $(CFG)))_$(notdir $(basename $(IMAGE)))
DETECT      := detector test $(DATA) $(CFG) $(WEIGHTS) $(IMAGE) -dont_show -ext_output
DETECTIONS  := '^[a-z ]+: [0-9]+%'

.PHONY: help all x86 arm run-x86 run-user deploy run-qemu boot compare clean

help:
	@echo "make x86       build Darknet for this PC            -> darknet/darknet"
	@echo "make arm       build Darknet for Cortex-A9 (ESL SDK) -> darknet/darknet-arm"
	@echo "make run-x86   detect on x86                         -> $(LOGDIR)/x86_$(TAG).txt"
	@echo "make run-user  detect under QEMU user mode           -> $(LOGDIR)/arm-a9-user_$(TAG).txt"
	@echo "make boot      boot the emulated ZedBoard (qemu-esl); quit with Ctrl-a x"
	@echo "make run-qemu  copy to the booted board and detect   -> $(LOGDIR)/qemu-esl_$(TAG).txt"
	@echo "make compare   check every log's detections against x86"
	@echo "make clean     remove x86 and ARM build outputs"

all: x86 arm

$(DARKNET)/$(WEIGHTS):
	wget -q --show-progress -O $@ $(WEIGHTS_URL)

$(LOG):
	mkdir -p $@

x86:
	$(MAKE) -C $(DARKNET) -j$(JOBS)

arm:
	source $(SDK_ENV) && \
	$(MAKE) -C $(DARKNET) -j$(JOBS) EXEC=$(ARM_EXE) OBJDIR=$(ARM_OBJ) CC="$$CC" CPP="$$CXX"

run-x86: x86 $(DARKNET)/$(WEIGHTS) | $(LOG)
	cd $(DARKNET) && ./darknet $(DETECT) 2>&1 | tee $(LOG)/x86_$(TAG).txt

run-user: arm $(DARKNET)/$(WEIGHTS) | $(LOG)
	source $(SDK_ENV) && cd $(DARKNET) && \
	qemu-arm -cpu cortex-a9 -L $$OECORE_TARGET_SYSROOT ./$(ARM_EXE) $(DETECT) 2>&1 | tee $(LOG)/arm-a9-user_$(TAG).txt

boot:
	source $(SDK_ENV) && qemu-esl

deploy: arm $(DARKNET)/$(WEIGHTS)
	ssh $(BOARD) mkdir -p $(BOARD_DIR)
	cd $(DARKNET) && scp -q -r $(ARM_EXE) $(WEIGHTS) cfg data $(BOARD):$(BOARD_DIR)/

run-qemu: deploy | $(LOG)
	ssh $(BOARD) 'cd $(BOARD_DIR) && ./$(ARM_EXE) $(DETECT) 2>&1' | tee $(LOG)/qemu-esl_$(TAG).txt

compare:
	@ref=$(LOG)/x86_$(TAG).txt; \
	test -f $$ref || { echo "no x86 reference: run 'make run-x86' first"; exit 1; }; \
	for f in $(LOG)/*_$(TAG).txt; do \
	  n=$$(grep -cE $(DETECTIONS) $$f); \
	  if [ $$n -gt 0 ] && diff -q <(grep -E $(DETECTIONS) $$ref) <(grep -E $(DETECTIONS) $$f) >/dev/null; \
	  then echo "MATCH     ($$n detections)  $$(basename $$f)"; \
	  else echo "MISMATCH  ($$n detections)  $$(basename $$f)"; fi; \
	done

clean:
	$(MAKE) -C $(DARKNET) clean
	rm -rf $(DARKNET)/$(ARM_OBJ) $(DARKNET)/$(ARM_EXE)
