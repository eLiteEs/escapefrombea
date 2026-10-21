# ============================================================================
#  Escape From Marisa 3: Escape From Bea
#  Makefile de compilacion y redistribucion
#
#  Uso:
#    make deps-mingw         -> (una vez) compila raylib mingw (estática + DLL)
#    make linux              -> binario Linux dinámico
#    make linux-static       -> binario Linux estático
#    make windows            -> .exe Windows estático
#    make windows-dynamic    -> .exe Windows dinámico (+ DLL)
#    make dist               -> zips (linux, linux-dynamic, windows, windows-dynamic, web)
#    make dist-all           -> dist + MSI
#    make dist-windows-msi   -> solo instalador .msi (requiere msitools)
#    make run                -> compila y ejecuta (Linux dinámico)
#    make clean              -> limpia build/ y dist/
#    make check-deps         -> verifica dependencias
# ============================================================================

APP_NAME     := escape_from_bea
GAME_TITLE   := Escape From Marisa 3 - Escape From Bea
VERSION      ?= 1.5.4b
VERSION_MSI  ?= $(shell echo $(VERSION) | sed 's/[^0-9.].*//')

SRC_DIR      := src
BUILD_DIR    := build
DIST_DIR     := dist
WIX_DIR      := wix

SOURCES      := $(wildcard $(SRC_DIR)/*.cpp)
HEADERS      := $(wildcard $(SRC_DIR)/*.h)
OBJ_LINUX    := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj-linux/%.o,$(SOURCES))
OBJ_WIN      := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj-win/%.o,$(SOURCES)) win/resources.o

# ---------------------------------------------------------------------------
# Web (Emscripten)
# ---------------------------------------------------------------------------
EMCC         ?= em++
WEB_VENDOR   ?= vendor/web
WEB_INC      := $(WEB_VENDOR)/include
WEB_LIB      := $(WEB_VENDOR)/lib
WEB_BUILD    := build/web
WEB_OUT      := $(WEB_BUILD)/index.html

WEB_CFLAGS   := -std=c++17 -O3 -I$(SRC_DIR) -I$(WEB_INC) \
                -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 \
                -sUSE_GLFW=3 -sUSE_WEBGL2=1 \
                -sALLOW_MEMORY_GROWTH=1 \
                -sSTACK_SIZE=1048576 \
                -sINITIAL_MEMORY=67108864

WEB_LDFLAGS  := $(WEB_LIB)/libraylib.a \
                -sUSE_GLFW=3 -sUSE_WEBGL2=1 \
                -sALLOW_MEMORY_GROWTH=1 \
                -sSTACK_SIZE=1048576 \
                -sINITIAL_MEMORY=67108864 \
                -sFORCE_FILESYSTEM=1 \
                --shell-file web/shell.html \
                --preload-file assets@assets \
                -o $(WEB_OUT)

# ---------------------------------------------------------------------------
# Toolchain Linux
# ---------------------------------------------------------------------------
CXX          ?= g++
CXXFLAGS     := -std=c++17 -Os -ffunction-sections -fdata-sections -g \
                -Wall -Wextra -Wno-unused-parameter -I$(SRC_DIR)
LDFLAGS      := -Wl,--gc-sections -s

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
    RAYLIB_LIBS := -lraylib -lm -lpthread -ldl -lrt -lX11
endif

# ---------------------------------------------------------------------------
# Toolchain Windows (mingw-w64) con raylib vendorizada
# ---------------------------------------------------------------------------
MINGW_PREFIX   := x86_64-w64-mingw32-
MINGW_CXX      := $(MINGW_PREFIX)g++
MINGW_CC       := $(MINGW_PREFIX)gcc
MINGW_AR       := $(MINGW_PREFIX)ar

MINGW_VENDOR        ?= vendor/mingw64
MINGW_RAYLIB_INC    := $(MINGW_VENDOR)/include
MINGW_RAYLIB_LIB    := $(MINGW_VENDOR)/lib
MINGW_RAYLIB_A      := $(MINGW_RAYLIB_LIB)/libraylib.a
MINGW_RAYLIB_DLL    := $(MINGW_RAYLIB_LIB)/libraylib.dll
MINGW_RAYLIB_IMPLIB := $(MINGW_RAYLIB_LIB)/libraylib.dll.a

# Link estático (para la build "windows" clásica)
MINGW_LDFLAGS_STATIC := -L$(MINGW_RAYLIB_LIB) \
	-Wl,-Bstatic -lraylib -Wl,-Bdynamic \
	-Wl,--gc-sections -s \
	-static-libgcc -static-libstdc++ \
	-lwinmm -lgdi32 -lopengl32 -luser32 -lkernel32 \
	-lshell32 -lole32 -lcomdlg32 -lcomctl32 -mwindows

# ---------------------------------------------------------------------------
# Distribución
# ---------------------------------------------------------------------------
DIST_LINUX_DIR         := $(DIST_DIR)/$(APP_NAME)-linux
DIST_LINUX_DYNAMIC_DIR := $(DIST_DIR)/$(APP_NAME)-linux-dynamic
DIST_WIN_DIR           := $(DIST_DIR)/$(APP_NAME)-windows
DIST_WIN_DYNAMIC_DIR   := $(DIST_DIR)/$(APP_NAME)-windows-dynamic
WEB_DIST_DIR           := $(DIST_DIR)/escape_from_bea-web
WEB_ZIP                := $(DIST_DIR)/escape_from_bea-web.zip
MSI_OUT                := $(DIST_DIR)/$(APP_NAME)-windows.msi
ASSET_DIRS             := assets

.PHONY: all help check-deps deps-mingw \
        linux linux-static linux-dynamic \
        windows windows-static windows-dynamic \
        dist dist-all dist-linux dist-linux-dynamic \
        dist-windows dist-windows-dynamic dist-windows-msi \
        run clean clean-vendor dirs \
        package-linux package-linux-dynamic package-windows package-windows-dynamic \
        web serve dist-web editor run-editor

all: linux

# ---------------------------------------------------------------------------
help:
	@echo ""
	@echo "  $(GAME_TITLE) v$(VERSION)"
	@echo ""
	@echo "  make deps-mingw         Compila raylib mingw (estática + DLL)"
	@echo "  make linux              Compila binario Linux dinámico"
	@echo "  make linux-static       Compila binario Linux estático"
	@echo "  make windows            Compila .exe Windows estático"
	@echo "  make windows-dynamic    Compila .exe Windows dinámico (+ DLL)"
	@echo "  make dist               Zips: linux, linux-dynamic, windows, windows-dynamic, web"
	@echo "  make dist-all           dist + MSI (requiere msitools)"
	@echo "  make dist-windows-msi   Solo MSI (requiere msitools: sudo pacman -S msitools)"
	@echo "  make run                Compila y ejecuta (Linux dinámico)"
	@echo "  make clean              Limpia build/ y dist/"
	@echo "  make check-deps         Comprueba dependencias"
	@echo ""

# ---------------------------------------------------------------------------
check-deps:
	@echo ">> Comprobando dependencias..."
	@command -v $(CXX) >/dev/null 2>&1 && echo "  g++ (Linux):            OK" \
		|| { echo "  g++ (Linux):            FALTA"; exit 1; }
	@pkg-config --exists raylib 2>/dev/null \
		&& echo "  raylib (pkg-config):    OK ($$(pkg-config --modversion raylib))" \
		|| echo "  raylib (pkg-config):    no detectada"
	@command -v $(MINGW_CXX) >/dev/null 2>&1 \
		&& echo "  mingw-w64 g++:          OK" \
		|| echo "  mingw-w64 g++:          FALTA (make windows fallara)"
	@command -v zip >/dev/null 2>&1 \
		&& echo "  zip:                    OK" \
		|| echo "  zip:                    FALTA (necesario para make dist)"
	@command -v upx >/dev/null 2>&1 \
		&& echo "  upx:                    OK (opcional)" \
		|| echo "  upx:                    FALTA (opcional)"
	@command -v wixl >/dev/null 2>&1 \
		&& echo "  wixl (msitools):        OK" \
		|| echo "  wixl (msitools):        FALTA (necesario para .msi)"
	@[ -f "$(MINGW_RAYLIB_A)" ] \
		&& echo "  raylib mingw estatica:  OK" \
		|| echo "  raylib mingw estatica:  FALTA -> make deps-mingw"
	@[ -f "$(MINGW_RAYLIB_DLL)" ] \
		&& echo "  raylib mingw DLL:       OK" \
		|| echo "  raylib mingw DLL:       FALTA -> make deps-mingw"
	@echo ""

# ---------------------------------------------------------------------------
deps-mingw:
	@echo ">> Comprobando mingw-w64..."
	@command -v $(MINGW_CC) >/dev/null 2>&1 \
		|| { echo "ERROR: $(MINGW_CC) no encontrado. Instala mingw-w64."; exit 1; }
	@echo ">> Descargando raylib..."
	@rm -rf /tmp/raylib-mingw
	@git clone --depth 1 https://github.com/raysan5/raylib.git /tmp/raylib-mingw
	@echo ">> Compilando raylib ESTÁTICA..."
	@$(MAKE) -C /tmp/raylib-mingw/src \
		PLATFORM=PLATFORM_DESKTOP OS=Windows_NT \
		CC=$(MINGW_CC) AR=$(MINGW_AR) \
		RAYLIB_LIBTYPE=STATIC RAYLIB_BUILD_MODE=RELEASE
	@mkdir -p $(MINGW_RAYLIB_INC) $(MINGW_RAYLIB_LIB)
	@cp /tmp/raylib-mingw/src/raylib.h   $(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/rlgl.h     $(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/raymath.h  $(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/libraylib.a $(MINGW_RAYLIB_LIB)/
	@echo ">> Compilando raylib DLL (SHARED)..."
	@$(MAKE) -C /tmp/raylib-mingw/src clean
	@$(MAKE) -C /tmp/raylib-mingw/src \
		PLATFORM=PLATFORM_DESKTOP OS=Windows_NT \
		CC=$(MINGW_CC) AR=$(MINGW_AR) \
		RAYLIB_LIBTYPE=SHARED RAYLIB_BUILD_MODE=RELEASE
	@# MinGW genera raylib.dll y libraylibdll.a — renombrar a los nombres canónicos
	@cp /tmp/raylib-mingw/src/raylib.dll       $(MINGW_RAYLIB_DLL)
	@cp /tmp/raylib-mingw/src/libraylibdll.a   $(MINGW_RAYLIB_IMPLIB)
	@echo ">> raylib mingw lista en $(MINGW_VENDOR)"
	@ls -la $(MINGW_RAYLIB_LIB)/

# ---------------------------------------------------------------------------
dirs:
	@mkdir -p $(BUILD_DIR)/obj-linux $(BUILD_DIR)/obj-win

# ---------------------------------------------------------------------------
# Linux
# ---------------------------------------------------------------------------
linux: linux-dynamic

linux-dynamic: $(BUILD_DIR)/$(APP_NAME)
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME)  (dinámico)"

linux-static: $(BUILD_DIR)/$(APP_NAME)-static
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME)-static  (estático)"

$(BUILD_DIR)/obj-linux/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | dirs
	@echo "  CXX   $<"
	@$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) -c $< -o $@

# Dinámico (clásico)
$(BUILD_DIR)/$(APP_NAME): $(OBJ_LINUX)
	@echo "  LD (dynamic) $@"
	@$(CXX) $(OBJ_LINUX) -o $@ $(LDFLAGS) $(RAYLIB_LIBS)

# Estático
$(BUILD_DIR)/$(APP_NAME)-static: $(OBJ_LINUX)
	@echo "  LD (static) $@"
	@$(CXX) $(OBJ_LINUX) -o $@ $(LDFLAGS) \
		-Wl,-Bstatic -lraylib -Wl,-Bdynamic \
		-static-libgcc -static-libstdc++ \
		-lm -lpthread -ldl -lrt -lX11

# ---------------------------------------------------------------------------
# Windows
# ---------------------------------------------------------------------------
windows: windows-static

windows-static: $(MINGW_RAYLIB_A) $(BUILD_DIR)/$(APP_NAME).exe
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME).exe  (estático)"

windows-dynamic: $(MINGW_RAYLIB_IMPLIB) $(BUILD_DIR)/$(APP_NAME)-dynamic.exe
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME)-dynamic.exe  (dinámico)"

$(MINGW_RAYLIB_A) $(MINGW_RAYLIB_DLL) $(MINGW_RAYLIB_IMPLIB):
	@echo ">> raylib mingw no encontrada, compilando..."
	@$(MAKE) --no-print-directory deps-mingw

$(BUILD_DIR)/obj-win/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | dirs
	@echo "  MINGW $<"
	@$(MINGW_CXX) $(CXXFLAGS) -I$(MINGW_RAYLIB_INC) -c $< -o $@

$(BUILD_DIR)/$(APP_NAME).exe: $(OBJ_WIN)
	@echo "  MINGW LD (static) $@"
	@$(MINGW_CXX) $^ -o $@ $(MINGW_LDFLAGS_STATIC)

$(BUILD_DIR)/$(APP_NAME)-dynamic.exe: $(OBJ_WIN) $(MINGW_RAYLIB_IMPLIB)
	@echo "  MINGW LD (dynamic) $@"
	@$(MINGW_CXX) $(OBJ_WIN) -o $@ \
		-L$(MINGW_RAYLIB_LIB) -lraylib \
		-static-libgcc -static-libstdc++ \
		-lwinmm -lgdi32 -lopengl32 -luser32 -lkernel32 \
		-lshell32 -lole32 -lcomdlg32 -lcomctl32 -mwindows

# ---------------------------------------------------------------------------
# Distribución
# ---------------------------------------------------------------------------
dist: dist-linux dist-linux-dynamic dist-windows dist-windows-dynamic dist-web
	@echo ""
	@echo ">> Zips listos en $(DIST_DIR)/:"
	@ls -la $(DIST_DIR)/*.zip 2>/dev/null || true
	@echo ""
	@echo "Para MSI: make dist-windows-msi"

dist-all: dist dist-windows-msi
	@echo ""
	@echo ">> Todo listo (zips + MSI):"
	@ls -la $(DIST_DIR)/*.zip $(DIST_DIR)/*.msi 2>/dev/null || true

# ---------------------------------------------------------------------------
dist-linux: linux-static
	@echo ">> Empaquetando Linux (estático)..."
	@rm -rf $(DIST_LINUX_DIR)
	@mkdir -p $(DIST_LINUX_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME)-static $(DIST_LINUX_DIR)/$(APP_NAME)
	@chmod +x $(DIST_LINUX_DIR)/$(APP_NAME)
	@strip $(DIST_LINUX_DIR)/$(APP_NAME) || true
	@command -v upx >/dev/null && upx --best --lzma $(DIST_LINUX_DIR)/$(APP_NAME) || true
	@$(MAKE) --no-print-directory package-linux
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-linux.zip && \
		zip -qr $(APP_NAME)-linux.zip $(APP_NAME)-linux
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-linux.zip"

dist-linux-dynamic: linux-dynamic
	@echo ">> Empaquetando Linux (dinámico)..."
	@rm -rf $(DIST_LINUX_DYNAMIC_DIR)
	@mkdir -p $(DIST_LINUX_DYNAMIC_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME) $(DIST_LINUX_DYNAMIC_DIR)/$(APP_NAME)
	@chmod +x $(DIST_LINUX_DYNAMIC_DIR)/$(APP_NAME)
	@strip $(DIST_LINUX_DYNAMIC_DIR)/$(APP_NAME) || true
	@libraylib=$$(ldd $(BUILD_DIR)/$(APP_NAME) | grep libraylib | awk '{print $$3}'); \
		if [ -n "$$libraylib" ] && [ -f "$$libraylib" ]; then \
			cp "$$libraylib" $(DIST_LINUX_DYNAMIC_DIR)/; \
			echo "   libraylib.so copiada"; \
		fi
	@command -v upx >/dev/null && upx --best --lzma $(DIST_LINUX_DYNAMIC_DIR)/$(APP_NAME) || true
	@$(MAKE) --no-print-directory package-linux-dynamic
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-linux-dynamic.zip && \
		zip -qr $(APP_NAME)-linux-dynamic.zip $(APP_NAME)-linux-dynamic
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-linux-dynamic.zip"

dist-windows: windows-static
	@echo ">> Empaquetando Windows (estático)..."
	@rm -rf $(DIST_WIN_DIR)
	@mkdir -p $(DIST_WIN_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME).exe $(DIST_WIN_DIR)/$(APP_NAME).exe
	@command -v upx >/dev/null && upx --best --lzma $(DIST_WIN_DIR)/$(APP_NAME).exe || true
	@$(MAKE) --no-print-directory package-windows
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-windows.zip && \
		zip -qr $(APP_NAME)-windows.zip $(APP_NAME)-windows
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-windows.zip"

dist-windows-dynamic: windows-dynamic
	@echo ">> Empaquetando Windows (dinámico)..."
	@rm -rf $(DIST_WIN_DYNAMIC_DIR)
	@mkdir -p $(DIST_WIN_DYNAMIC_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME)-dynamic.exe $(DIST_WIN_DYNAMIC_DIR)/$(APP_NAME).exe
	@cp $(MINGW_RAYLIB_DLL) $(DIST_WIN_DYNAMIC_DIR)/
	@command -v upx >/dev/null && upx --best --lzma $(DIST_WIN_DYNAMIC_DIR)/$(APP_NAME).exe || true
	@$(MAKE) --no-print-directory package-windows-dynamic
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-windows-dynamic.zip && \
		zip -qr $(APP_NAME)-windows-dynamic.zip $(APP_NAME)-windows-dynamic
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-windows-dynamic.zip"

# ---------------------------------------------------------------------------
# MSI (requiere msitools: sudo pacman -S msitools)
# ---------------------------------------------------------------------------
dist-windows-msi: dist-windows
	@echo ">> Construyendo instalador MSI..."
	@command -v wixl >/dev/null || { \
		echo "ERROR: wixl no encontrado."; \
		echo "  sudo pacman -S msitools"; \
		exit 1; }
	@test -f $(WIX_DIR)/gen-msi-wxs.sh || { \
		echo "ERROR: falta $(WIX_DIR)/gen-msi-wxs.sh"; exit 1; }
	@echo ">> Generando XML WiX..."
	@sh $(WIX_DIR)/gen-msi-wxs.sh \
		$(CURDIR)/$(DIST_WIN_DIR) \
		$(VERSION_MSI) \
		$(APP_NAME) \
		"$(GAME_TITLE)" \
		> $(DIST_DIR)/installer.wxs
	@echo ">> Compilando MSI..."
	@wixl -D SourceDir=$(CURDIR)/$(DIST_WIN_DIR) \
		-o $(MSI_OUT) $(DIST_DIR)/installer.wxs
	@rm -f $(DIST_DIR)/installer.wxs
	@echo "   -> $(MSI_OUT)"

# ---------------------------------------------------------------------------
package-linux:
	@if [ -d "$(ASSET_DIRS)" ]; then cp -r $(ASSET_DIRS) $(DIST_LINUX_DIR)/; fi
	@[ -f README.md ]    && cp README.md    $(DIST_LINUX_DIR)/ || true
	@[ -f CHANGELOG.md ] && cp CHANGELOG.md $(DIST_LINUX_DIR)/ || true
	@[ -f ATRIBUTIONS ]  && cp ATRIBUTIONS  $(DIST_LINUX_DIR)/ || true
	@[ -f LICENSE ]      && cp LICENSE      $(DIST_LINUX_DIR)/ || true

package-linux-dynamic:
	@if [ -d "$(ASSET_DIRS)" ]; then cp -r $(ASSET_DIRS) $(DIST_LINUX_DYNAMIC_DIR)/; fi
	@[ -f README.md ]    && cp README.md    $(DIST_LINUX_DYNAMIC_DIR)/ || true
	@[ -f CHANGELOG.md ] && cp CHANGELOG.md $(DIST_LINUX_DYNAMIC_DIR)/ || true
	@[ -f ATRIBUTIONS ]  && cp ATRIBUTIONS  $(DIST_LINUX_DYNAMIC_DIR)/ || true
	@[ -f LICENSE ]      && cp LICENSE      $(DIST_LINUX_DYNAMIC_DIR)/ || true

package-windows:
	@if [ -d "$(ASSET_DIRS)" ]; then cp -r $(ASSET_DIRS) $(DIST_WIN_DIR)/; fi
	@[ -f README.md ]    && cp README.md    $(DIST_WIN_DIR)/ || true
	@[ -f CHANGELOG.md ] && cp CHANGELOG.md $(DIST_WIN_DIR)/ || true
	@[ -f ATRIBUTIONS ]  && cp ATRIBUTIONS  $(DIST_WIN_DIR)/ || true
	@[ -f LICENSE ]      && cp LICENSE      $(DIST_WIN_DIR)/ || true
	@for dll in libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll; do \
		src=$$(find /usr/x86_64-w64-mingw32 /usr/lib/gcc -name "$$dll" 2>/dev/null | head -1); \
		if [ -n "$$src" ]; then cp "$$src" $(DIST_WIN_DIR)/; fi; \
	done

package-windows-dynamic:
	@if [ -d "$(ASSET_DIRS)" ]; then cp -r $(ASSET_DIRS) $(DIST_WIN_DYNAMIC_DIR)/; fi
	@[ -f README.md ]    && cp README.md    $(DIST_WIN_DYNAMIC_DIR)/ || true
	@[ -f CHANGELOG.md ] && cp CHANGELOG.md $(DIST_WIN_DYNAMIC_DIR)/ || true
	@[ -f ATRIBUTIONS ]  && cp ATRIBUTIONS  $(DIST_WIN_DYNAMIC_DIR)/ || true
	@[ -f LICENSE ]      && cp LICENSE      $(DIST_WIN_DYNAMIC_DIR)/ || true

# ---------------------------------------------------------------------------
run: linux
	@echo ">> Ejecutando $(APP_NAME)..."
	@./$(BUILD_DIR)/$(APP_NAME)

clean:
	@echo ">> Limpiando build/ y dist/..."
	@rm -rf $(BUILD_DIR) $(DIST_DIR)

clean-vendor:
	@rm -rf vendor

.PRECIOUS: $(BUILD_DIR)/obj-linux/%.o $(BUILD_DIR)/obj-win/%.o

# ---------------------------------------------------------------------------
# Web
# ---------------------------------------------------------------------------
web: $(SOURCES) $(HEADERS) web/shell.html
	@echo ">> Compilando web..."
	@mkdir -p $(WEB_BUILD)
	@$(EMCC) $(WEB_CFLAGS) $(SOURCES) $(WEB_LDFLAGS)
	@echo ">> OK: $(WEB_BUILD)/index.html"

serve: web
	@cd $(WEB_BUILD) && python3 -m http.server 8000

dist-web: web
	@rm -rf $(WEB_DIST_DIR)
	@mkdir -p $(WEB_DIST_DIR)
	@cp build/web/index.html $(WEB_DIST_DIR)/
	@cp build/web/index.js   $(WEB_DIST_DIR)/
	@cp build/web/index.wasm $(WEB_DIST_DIR)/
	@cp build/web/index.data $(WEB_DIST_DIR)/
	@[ -f build/web/index.worker.js ] && cp build/web/index.worker.js $(WEB_DIST_DIR)/ || true
	@[ -f README.md ] && cp README.md $(WEB_DIST_DIR)/ || true
	@cd $(WEB_DIST_DIR) && rm -f ../escape_from_bea-web.zip && \
		zip -qr ../escape_from_bea-web.zip .
	@echo "   -> $(WEB_ZIP)"

# ---------------------------------------------------------------------------
editor: tools/level_designer.cpp
	$(CXX) -std=c++17 -O2 -Wall -Wextra -o tools/level_designer tools/level_designer.cpp

run-editor: editor
	./tools/level_designer
