#
# Copyright (c) 2003-2020 Apple Inc. All rights reserved.
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

MVERS = "mDNSResponder-1380"

projectdir		:= $(SRCROOT)/mDNSMacOSX
buildsettings	:= OBJROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' DSTROOT='$(DSTROOT)' MVERS=$(MVERS) SDKROOT='$(SDKROOT)'
ifdef GCC_VERSION
buildsettings	+= GCC_VERSION=$(GCC_VERSION)
endif

.PHONY: install installSome installEmpty installExtras SystemLibraries installhdrs installapi installsrc java clean

# Sanitizer support
# Disable Sanitizer instrumentation in LibSystem contributors. See rdar://problem/29952210.
UNSUPPORTED_SANITIZER_PROJECTS := mDNSResponderSystemLibraries mDNSResponderSystemLibraries_Sim
PROJECT_SUPPORTS_SANITIZERS := 1
ifneq ($(words $(filter $(UNSUPPORTED_SANITIZER_PROJECTS), $(RC_ProjectName))), 0)
  PROJECT_SUPPORTS_SANITIZERS := 0
endif
ifeq ($(RC_ENABLE_ADDRESS_SANITIZATION),1)
  ifeq ($(PROJECT_SUPPORTS_SANITIZERS),1)
    $(info Enabling Address Sanitizer)
    buildsettings += -enableAddressSanitizer YES
  else
    $(warning WARNING: Address Sanitizer not supported for project $(RC_ProjectName))
  endif
endif
ifeq ($(RC_ENABLE_THREAD_SANITIZATION),1)
  ifeq ($(PROJECT_SUPPORTS_SANITIZERS),1)
    $(info Enabling Thread Sanitizer)
    buildsettings += -enableThreadSanitizer YES
  else
    $(warning WARNING: Thread Sanitizer not supported for project $(RC_ProjectName))
  endif
endif
ifeq ($(RC_ENABLE_UNDEFINED_BEHAVIOR_SANITIZATION),1)
  ifeq ($(PROJECT_SUPPORTS_SANITIZERS),1)
    $(info Enabling Undefined Behavior Sanitizer)
    buildsettings += -enableUndefinedBehaviorSanitizer YES
  else
    $(warning WARNING: Undefined Behavior Sanitizer not supported for project $(RC_ProjectName))
  endif
endif

# B&I install build targets
#
# For the mDNSResponder build alias, the make target used by B&I depends on the platform:
#
#	Platform	Make Target
#	--------	-----------
#	osx			install
#	ios			installSome
#	atv			installSome
#	watch		installSome
#
# For the mDNSResponderSystemLibraries and mDNSResponderSystemLibraries_sim build aliases, B&I uses the SystemLibraries
# target for all platforms.

install:
ifeq ($(RC_ProjectName), mDNSResponderServices)
  ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Services-macOS'
  else
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Services'
  endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p '$(DSTROOT)/AppleInternal'
else ifeq ($(RC_ProjectName), libmdns)
	mkdir -p '$(DSTROOT)/AppleInternal'
else
	cd '$(projectdir)'; xcodebuild install $(buildsettings)
endif

installSome:
	cd '$(projectdir)'; xcodebuild install $(buildsettings)

installEmpty:
	mkdir -p '$(DSTROOT)/AppleInternal'

installExtras:
ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-macOS'
else ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), ios)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-iOS'
else ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), atv)
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras-tvOS'
else
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target 'Build Extras'
endif

SystemLibraries:
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target SystemLibraries

# B&I installhdrs build targets

installhdrs::
ifeq ($(RC_ProjectName), mDNSResponderServices)
  ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target 'Build Services-macOS'
  else
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target 'Build Services'
  endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p '$(DSTROOT)/AppleInternal'
else ifeq ($(RC_ProjectName), libmdns)
	mkdir -p '$(DSTROOT)/AppleInternal'
else ifneq ($(findstring SystemLibraries,$(RC_ProjectName)),)
	cd '$(projectdir)'; xcodebuild installhdrs $(buildsettings) -target SystemLibraries
endif

# B&I installapi build targets

installapi:
ifeq ($(RC_ProjectName), mDNSResponderServices)
  ifeq ($(RC_PROJECT_COMPILATION_PLATFORM), osx)
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target 'Build Services-macOS'
  else
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target 'Build Services'
  endif
else ifeq ($(RC_ProjectName), mDNSResponderServices_Sim)
	mkdir -p '$(DSTROOT)/AppleInternal'
else ifeq ($(RC_ProjectName), libmdns)
	mkdir -p '$(DSTROOT)/AppleInternal'
else ifneq ($(findstring SystemLibraries,$(RC_ProjectName)),)
	cd '$(projectdir)'; xcodebuild installapi $(buildsettings) -target SystemLibrariesDynamic
endif

# Misc. targets

installsrc:
	ditto . '$(SRCROOT)'
	rm -rf '$(SRCROOT)/mDNSWindows' '$(SRCROOT)/Clients/FirefoxExtension'

java:
	cd '$(projectdir)'; xcodebuild install $(buildsettings) -target libjdns_sd.jnilib

clean::
	echo clean
