#
# Copyright (c) 2003-2018 Apple Inc. All rights reserved.
#
# Top level makefile for Build & Integration (B&I).
# 
# This file is used to facilitate checking the mDNSResponder project directly from git and submitting to B&I at Apple.
#
# The various platform directories contain makefiles or projects specific to that platform.
#
#    B&I builds must respect the following target:
#         install:
#         installsrc:
#         installhdrs:
#         installapi:
#         clean:
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make

MVERS = "mDNSResponder-1344"

VER =
ifneq ($(strip $(GCC_VERSION)),)
	VER = -- GCC_VERSION=$(GCC_VERSION)
endif
echo "VER = $(VER)"

projectdir	:= $(SRCROOT)/mDNSMacOSX
buildsettings	:= OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) MVERS=$(MVERS) SDKROOT=$(SDKROOT)

.PHONY: install installSome installEmpty installExtras SystemLibraries installhdrs installapi installsrc java clean

# B&I install build targets
#
# For the mDNSResponder build alias, the make target used by B&I depends on the platform:
#
#	Platform	Make Target
#	--------	-----------
#	osx		install
#	ios		installSome
#	atv		installSome
#	watch		installSome
#
# For the mDNSResponderSystemLibraries and mDNSResponderSystemLibraries_sim build aliases, B&I uses the SystemLibraries
# target for all platforms.

install:
ifeq ($(RC_ProjectName), mDNSResponderServices)
ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Services-macOS' $(VER)
else
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Services' $(VER)
endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p $(DSTROOT)/AppleInternal
else
	cd '$(projectdir)'; xcodebuild install $(buildsettings) $(VER)
endif

installSome:
	cd '$(projectdir)'; xcodebuild install $(buildsettings) $(VER)

installEmpty:
	mkdir -p $(DSTROOT)/AppleInternal

installExtras:
ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-macOS' $(VER)
else ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), ios)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-iOS' $(VER)
else ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), atv)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-tvOS' $(VER)
else
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras' $(VER)
endif

SystemLibraries:
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target SystemLibraries $(VER)

# B&I installhdrs build targets

installhdrs::
ifeq ($(RC_ProjectName), mDNSResponderServices)
ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target 'Build Services-macOS' $(VER)
else
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target 'Build Services' $(VER)
endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p $(DSTROOT)/AppleInternal
else ifneq ($(findstring SystemLibraries,$(RC_ProjectName)),)
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target SystemLibraries $(VER)
endif

# B&I installapi build targets

installapi:
ifeq ($(RC_ProjectName), mDNSResponderServices)
ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target 'Build Services-macOS' $(VER)
else
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target 'Build Services' $(VER)
endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p $(DSTROOT)/AppleInternal
else ifneq ($(findstring SystemLibraries,$(RC_ProjectName)),)
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target SystemLibrariesDynamic $(VER)
endif

# Misc. targets

installsrc:
	ditto . '$(SRCROOT)'
	rm -rf '$(SRCROOT)/mDNSWindows' '$(SRCROOT)/Clients/FirefoxExtension'

java:
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target libjdns_sd.jnilib $(VER)

clean::
	echo clean
