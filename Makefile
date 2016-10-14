#
# Copyright (c) 2016, CESAR.
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.
#
# KNoTThing Arduino Library Makefile
#

# TODO: change this variables according to operating system
MKDIR=mkdir
CP=cp
GIT=git
ZIP=zip

KNOT_THING_NAME = KNoTThing
KNOT_THING_VERSION = KNOT_THING_00.01
KNOT_THING_TARGET = $(KNOT_THING_NAME).zip

KNOT_THING_HDR = knot_thing_config.h knot_thing_main.h KNoTThing.h
KNOT_THING_SRC = knot_thing_main.c KNoTThing.cpp
KNOT_THING_FILES = ./src/*

KNOT_THING_DOWNLOAD_DIR = download
KNOT_THING_BUILD_DIR = build

#Dependencies
# TODO: Change branch to release tag
KNOT_PROTOCOL_LIB_VERSION = devel_tgfb_KNOT-205
KNOT_PROTOCOL_LIB_REPO = knot-protocol-source
KNOT_PROTOCOL_LIB_SITE = https://github.com/CESARBR/$(KNOT_PROTOCOL_LIB_REPO).git
KNOT_PROTOCOL_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_PROTOCOL_LIB_REPO)/src

# TODO: add knot hal lib to the makefile
#KNOT_HAL_LIB_VERSION = KNOT_HAL_00.01
#KNOT_HAL_LIB_REPO = knot-hal-source
#KNOT_HAL_LIB_SITE = git@github.com:CESARBR/$(KNOT_HAL_LIB_REPO).git
#KNOT_HAL_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/src

.PHONY: clean 

default: all

all:  $(KNOT_THING_TARGET)

$(KNOT_THING_DOWNLOAD_DIR):
	$(MKDIR) -p ./$(KNOT_THING_DOWNLOAD_DIR)

$(KNOT_THING_BUILD_DIR):
	$(MKDIR) -p ./$(KNOT_THING_BUILD_DIR)

$(KNOT_PROTOCOL_LIB_DIR):  $(KNOT_THING_DOWNLOAD_DIR)
	$(GIT) clone -b $(KNOT_PROTOCOL_LIB_VERSION) $(KNOT_PROTOCOL_LIB_SITE) ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_PROTOCOL_LIB_REPO)

$(KNOT_THING_TARGET):  $(KNOT_PROTOCOL_LIB_DIR)
	$(MKDIR) -p ./$(KNOT_THING_NAME)
	$(CP) -r $(KNOT_THING_FILES) ./$(KNOT_THING_NAME)/.
	$(CP) -r $(KNOT_PROTOCOL_LIB_DIR)/* ./$(KNOT_THING_NAME)/.
	$(ZIP) -r $(KNOT_THING_TARGET) ./$(KNOT_THING_NAME)

clean:
	$(RM) $(KNOT_THING_TARGET)
	$(RM) -rf ./$(KNOT_THING_DOWNLOAD_DIR)
	$(RM) -rf ./$(KNOT_THING_BUILD_DIR)
	$(RM) -rf ./$(KNOT_THING_NAME)

