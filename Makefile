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
FIND=find

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

KNOT_HAL_SRC_DRIVERS_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/src/drivers
KNOT_HAL_SRC_NRF_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/src/nrf24l01
KNOT_HAL_SRC_SPI_LIB_DIR = ./$(KNOT_THING_DOWNLOAD_DIR)/$(KNOT_HAL_LIB_REPO)/src/spi

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
	$(MKDIR) -p ./$(KNOT_THING_NAME)/src/include

	#Filling whith configuraiton files for Arduino IDE
	# TODO: Create keywords.txt file to KNoT Thing
	# TODO: Amend keywords of the HAL libs, protocol and thing
	$(CP) -r $(KNOT_THING_FILES)/library.properties ./$(KNOT_THING_NAME)

	#Filling root and thing directory
	$(CP) -r $(KNOT_THING_FILES)/*.h ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_THING_FILES)/*.c ./$(KNOT_THING_NAME)/src
	$(CP) -r $(KNOT_THING_FILES)/*.cpp ./$(KNOT_THING_NAME)/src

	#Filling protocol directory
	$(CP) -r $(KNOT_PROTOCOL_LIB_DIR)/*[^*.am] ./$(KNOT_THING_NAME)/src

	#Filling hal headers directory
	$(CP) -r $(KNOT_HAL_HDR_LIB_DIR)/*.h ./$(KNOT_THING_NAME)/src/include

	#include folder
	$(FIND) ./$(KNOT_HAL_SRC_LIB_DIR)/log/ \( ! -name '*linux*' -and -name '*.cpp' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;
	$(FIND) ./$(KNOT_HAL_SRC_LIB_DIR)/storage/ \( ! -name '*linux*' -and -name '*.cpp' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;
	$(FIND) ./$(KNOT_HAL_SRC_LIB_DIR)/time/ \( ! -name '*linux*' -and -name '*.cpp' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;

	# Include comm headers and source files
	$(FIND) ./$(KNOT_HAL_SRC_LIB_DIR)/comm/ \( \( -name '*.c' -or -name '*.h' \) -and ! -name '*serial*' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;

	# Include nrf24l01 headers and source files
	$(FIND) ./$(KNOT_HAL_SRC_NRF_LIB_DIR)/ \( \( -name '*.c' -or -name '*.h' \) -and ! -name '*linux*' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;

 	# Include gpio headers and source files
	$(CP) -r ./$(KNOT_HAL_SRC_DRIVERS_LIB_DIR)/gpio/*.c ./$(KNOT_THING_NAME)/src

	# Include drivers headers and source files
	$(CP) -r ./$(KNOT_HAL_SRC_DRIVERS_LIB_DIR)/*.c ./$(KNOT_THING_NAME)/src
	$(CP) -r ./$(KNOT_HAL_SRC_DRIVERS_LIB_DIR)/*.h ./$(KNOT_THING_NAME)/src

	# Include SPI headers and source files
	$(FIND) ./$(KNOT_HAL_SRC_SPI_LIB_DIR)/ \( \( -name '*.c' -or -name '*.h' \) -and ! -name '*linux*' \) -exec $(CP) {} ./$(KNOT_THING_NAME)/src \;

	#Zip directory
	$(ZIP) -r $(KNOT_THING_TARGET) ./$(KNOT_THING_NAME)

clean:
	$(RM) $(KNOT_THING_TARGET)
	$(RM) -rf ./$(KNOT_THING_DOWNLOAD_DIR)
	$(RM) -rf ./$(KNOT_THING_BUILD_DIR)
	$(RM) -rf ./$(KNOT_THING_NAME)
