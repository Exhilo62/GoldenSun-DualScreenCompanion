# GoldenSun3DS: builds the 3DS app against the mGBA core library (build/mgba-3ds).
# Run from a path without spaces (D:/gs3ds is a junction to this folder).
DEVKITPRO ?= /c/devkitPro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITARM)/bin:$(PATH)

CC      := arm-none-eabi-gcc
MGBA    := third_party/mgba
MGBALIB := build/mgba-3ds
OUT     := build/app

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
DEFS    := -DBUILD_STATIC -DCOLOR_16_BIT -DCOLOR_5_6_5 -DENABLE_DIRECTORIES -DENABLE_VFS -DENABLE_VFS_FD -DFIXED_ROM_BUFFER -DHAVE_STRDUP -DHAVE_STRLCPY -DIOAPI_NO_64 -DM_CORE_GBA -DUSE_ZLIB -D_GNU_SOURCE -D__3DS__
CFLAGS  := -MMD -MP -O2 -g -Wall -Wno-format -ffunction-sections -mword-relocations $(ARCH) $(DEFS) \
           -I$(MGBA)/include -I$(MGBALIB)/include -I$(DEVKITPRO)/libctru/include -Isrc
LDFLAGS := -specs=3dsx.specs -Wl,--gc-sections $(ARCH) -L$(DEVKITPRO)/libctru/lib
LIBS    := $(MGBALIB)/libmgba.a $(MGBALIB)/zlib/libzlibstatic.a -lctru -lm

SRCS := src/app.c src/platform_3ds.c src/ui.c src/gfx.c src/font_data.c src/art.c src/art_data.c src/logo_data.c src/icon_data.c src/mapicon_data.c src/mapview.c src/mapdecode.c src/menunav.c src/gs_state.c src/gs_game.c src/gs_areas.c src/gs_items.c src/gs_abilities.c src/gs_djinn.c src/bestiary.c src/launcher.c src/gs_classes.c src/gs_itemdesc.c src/gs_field.c src/dialoglog.c src/bgwrite.c $(MGBA)/src/platform/3ds/ctru-heap.c
OBJS := $(patsubst %.c,$(OUT)/%.o,$(SRCS))

all: $(OUT)/GoldenSun3DS.3dsx

# The pick screen shows the compile time (__DATE__ __TIME__): rebuild launcher.o whenever any other object changed so the stamp is the time of the latest build.
$(OUT)/src/launcher.o: $(filter-out $(OUT)/src/launcher.o,$(OBJS))

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/GoldenSun3DS.elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(OUT)/GoldenSun3DS.smdh: assets/icon.png
	smdhtool --create "Golden Sun - Dual Screen Companion" "Golden Sun with a dual-screen touch UI" "GoldenSun3DS" $< $@

$(OUT)/GoldenSun3DS.3dsx: $(OUT)/GoldenSun3DS.elf $(OUT)/GoldenSun3DS.smdh
	3dsxtool $< $@ --smdh=$(OUT)/GoldenSun3DS.smdh

-include $(OBJS:.o=.d)

clean:
	rm -rf $(OUT)
