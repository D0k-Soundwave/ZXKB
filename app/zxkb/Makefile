# ZXKB app. Build in the QMK MSYS shell (MINGW64 g++):  make -C /h/ZXKB/app/zxkb
# Output: ../../build/ZXKB.exe  - one file, no runtime or DLLs needed.
OUT      = ../../build/ZXKB.exe
OBJDIR   = ../../build/obj
SRC      = src/main.cpp src/hidlink.cpp src/vu.cpp src/vumath.cpp src/settings.cpp src/effects.cpp
OBJ      = $(SRC:src/%.cpp=$(OBJDIR)/%.o) $(OBJDIR)/zxkb.res.o
CXX      = g++
WINDRES  = windres
CXXFLAGS = -Os -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables \
           -ffunction-sections -fdata-sections -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00 -municode
LDFLAGS  = -static -s -mwindows -municode -Wl,--gc-sections
LIBS     = -lsetupapi -lhid -lole32 -lshell32 -luser32 -ladvapi32 -lcomctl32 -luuid

$(OUT): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	@ls -l $@

$(OBJDIR)/%.o: src/%.cpp src/*.h res/resource.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/zxkb.res.o: res/zxkb.rc res/resource.h res/zxkb.manifest res/zxkb.ico res/zxkb_off.ico | $(OBJDIR)
	$(WINDRES) -I res -O coff -o $@ $<

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(OUT)

.PHONY: clean
