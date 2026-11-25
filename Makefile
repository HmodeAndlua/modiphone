TARGET := iphone:clang:latest:15.0
ARCHS := arm64
INSTALL_TARGET_PROCESSES := OneState

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = OneStateFull

OneStateFull_FILES = \
	Tweak.mm \
	imgui/imgui.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/imgui_impl_metal.mm

OneStateFull_CFLAGS = -fobjc-arc -Wall -O2 -Wno-unknown-warning-option -Wno-uninitialized-const-pointer -Wno-error
OneStateFull_CCFLAGS = -std=c++17 -fobjc-arc -Wall -O2 -I. -Iimgui -fno-exceptions -Wno-unknown-warning-option -Wno-uninitialized-const-pointer -Wno-error
OneStateFull_FRAMEWORKS = Foundation UIKit Metal MetalKit QuartzCore CoreGraphics CoreText Security
OneStateFull_LIBRARIES = c++

include $(THEOS_MAKE_PATH)/tweak.mk
