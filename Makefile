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
KNOT_THING_TARGET = $(KNOT_THING_NAME).zip
KNOT_THING_FILES = ./src

KNOT_THING_DOWNLOAD_DIR = download
KNOT_THING_BUILD_DIR = build

#Dependencies
KNOT_PROTOCOL_LIB_VERSION = master
KNOT_PROTOCOL_LIB_REPO = knot-protocol-source
KNOT_PROTOCOL_LIB_SITE = https://github.com/CESARBR/$(KNOT_PROTOCOL_LIB_REPO).git
KNOT_PROTOCOL_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_PROTOCOL_LIB_REPO)/src

KNOT_HAL_LIB_VERSION = master
KNOT_HAL_LIB_REPO = knot-hal-source
KNOT_HAL_LIB_SITE = https://github.com/CESARBR/$(KNOT_HAL_LIB_REPO).git
KNOT_HAL_HDR_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/include
KNOT_HAL_SRC_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/src/hal

.PHONY: clean

default: all

all:  $(KNOT_THING_TARGET)

$(KNOT_THING_DOWNLOAD_DIR):
	$(MKDIR) -p ./$(KNOT_THING_DOWNLOAD_DIR)

$(KNOT_THING_BUILD_DIR):
	$(MKDIR) -p ./$(KNOT_THING_BUILD_DIR)

$(KNOT_PROTOCOL_LIB_DIR):  $(KNOT_THING_DOWNLOAD_DIR)
	$(GIT) clone -b $(KNOT_PROTOCOL_LIB_VERSION) $(KNOT_PROTOCOL_LIB_SITE) ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_PROTOCOL_LIB_REPO)
	$(GIT) clone -b $(KNOT_HAL_LIB_VERSION) $(KNOT_HAL_LIB_SITE) ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)

$(KNOT_THING_TARGET):  $(KNOT_PROTOCOL_LIB_DIR)
	#Creating subdirectories
	$(MKDIR) -p ./$(KNOT_THING_NAME)/src

	#Filling whith configuraiton files for Arduino IDE
	# TODO: Create keywords.txt file to KNoT Thing
	# TODO: Amend keywords of the HAL libs, protocol and thing
	$(CP) -r $(KNOT_THING_FILES)/library.properties ./$(KNOT_THING_NAME)
	# $(CP) -r $(KNOT_THING_FILES)/keywords.txt ./$(KNOT_THING_NAME)

	#Filling root and thing directory
	$(CP) -r $(KNOT_THING_FILES)/*.h ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_THING_FILES)/*.c ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_THING_FILES)/*.cpp ./$(KNOT_THING_NAME)/src

	#Filling protocol directory
	# TODO: Check whether to bring Makefile.am
	$(CP) -r $(KNOT_PROTOCOL_LIB_DIR)/* ./$(KNOT_THING_NAME)/src

	#Filling hal directory
	# TODO: Add to the KNoTThing root keywords.txt content of each keywords.txt HAL lib
	$(CP) -r $(KNOT_HAL_HDR_LIB_DIR)/*.h ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_HAL_SRC_LIB_DIR)/log/*.cpp ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_HAL_SRC_LIB_DIR)/storage/*.cpp ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_HAL_SRC_LIB_DIR)/time/*.cpp ./$(KNOT_THING_NAME)/src

	#Zip directory
	$(ZIP) -r $(KNOT_THING_TARGET) ./$(KNOT_THING_NAME)

clean:
	$(RM) $(KNOT_THING_TARGET)
	$(RM) -rf ./$(KNOT_THING_DOWNLOAD_DIR)
	$(RM) -rf ./$(KNOT_THING_BUILD_DIR)
	$(RM) -rf ./$(KNOT_THING_NAME)
