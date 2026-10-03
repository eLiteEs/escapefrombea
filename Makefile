# ============================================================================
#  Escape From Marisa 3: Escape From Bea
#  Makefile de compilacion y redistribucion (itch.io)
#
#  Uso:
#    make deps-mingw    -> (una vez) compila raylib estatica para Windows
#    make linux	 -> binario Linux
#    make windows       -> .exe Windows (cross-compile, estatico)
#    make dist	  -> zips listos para itch.io en dist/
#    make run	   -> compila y ejecuta (Linux)
#    make clean	 -> limpia build/ y dist/
#    make check-deps    -> verifica dependencias
#
#  Variables:
#    VERSION=1.0.0
#    MINGW_VENDOR=vendor/mingw64    (ruta a raylib mingw estatica)
# ============================================================================

APP_NAME     := escape_from_bea
GAME_TITLE   := Escape From Marisa 3 - Escape From Bea
VERSION      ?= 1.0.0
SRC_DIR      := src
BUILD_DIR    := build
DIST_DIR     := dist

SOURCES      := $(wildcard $(SRC_DIR)/*.cpp)
HEADERS      := $(wildcard $(SRC_DIR)/*.h)
OBJ_LINUX    := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj-linux/%.o,$(SOURCES))
OBJ_WIN      := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj-win/%.o,$(SOURCES))

EMCC	 ?= em++
SRC_DIR      := src
WEB_VENDOR   ?= vendor/web
WEB_INC      := $(WEB_VENDOR)/include
WEB_LIB      := $(WEB_VENDOR)/lib
WEB_BUILD    := build/web
WEB_OUT      := $(WEB_BUILD)/index.html

WEB_CFLAGS   := -std=c++17 -O2 -I$(SRC_DIR) -I$(WEB_INC) \
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
CXX	  ?= g++
CXXFLAGS     := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -I$(SRC_DIR)
LDFLAGS      :=

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
    RAYLIB_LIBS := -lraylib -lm -lpthread -ldl -lrt -lX11
endif

# ---------------------------------------------------------------------------
# Toolchain Windows (mingw-w64) con raylib estatica vendorizada
# ---------------------------------------------------------------------------
MINGW_PREFIX   := x86_64-w64-mingw32-
MINGW_CXX      := $(MINGW_PREFIX)g++
MINGW_CC       := $(MINGW_PREFIX)gcc
MINGW_AR       := $(MINGW_PREFIX)ar

MINGW_VENDOR     ?= vendor/mingw64
MINGW_RAYLIB_INC := $(MINGW_VENDOR)/include
MINGW_RAYLIB_LIB := $(MINGW_VENDOR)/lib
MINGW_RAYLIB_A   := $(MINGW_RAYLIB_LIB)/libraylib.a

# Enlazamos estatico: raylib + libgcc + libstdc++. Solo quedara libwinpthread-1.dll
# que copiamos en el empaquetado por si acaso.
MINGW_LDFLAGS  := -L$(MINGW_RAYLIB_LIB) \
	          -Wl,-Bstatic -lraylib -Wl,-Bdynamic \
	          -static-libgcc -static-libstdc++ \
	          -lwinmm -lgdi32 -lopengl32 -luser32 -lkernel32 \
	          -lshell32 -lole32 -lcomdlg32 -lcomctl32

# ---------------------------------------------------------------------------
# Distribucion
# ---------------------------------------------------------------------------
DIST_LINUX_DIR := $(DIST_DIR)/$(APP_NAME)-linux
DIST_WIN_DIR   := $(DIST_DIR)/$(APP_NAME)-windows
WEB_DIST_DIR := dist/escape_from_bea-web
WEB_ZIP      := dist/escape_from_bea-web.zip
ASSET_DIRS     := assets

.PHONY: all help check-deps deps-mingw linux windows dist dist-linux dist-windows \
	run clean dirs package-linux package-windows web serve dist-web

all: linux

# ---------------------------------------------------------------------------
help:
	@echo ""
	@echo "  $(GAME_TITLE) v$(VERSION)"
	@echo ""
	@echo "  make deps-mingw     Compila raylib estatica para Windows (una vez)"
	@echo "  make linux	  Compila binario Linux"
	@echo "  make windows	Compila .exe Windows (estatico)"
	@echo "  make dist	   Genera zips para itch.io en dist/"
	@echo "  make dist-linux     Solo Linux"
	@echo "  make dist-windows   Solo Windows"
	@echo "  make run	    Compila y ejecuta (Linux)"
	@echo "  make clean	  Limpia build/ y dist/"
	@echo "  make check-deps     Comprueba dependencias"
	@echo ""

# ---------------------------------------------------------------------------
check-deps:
	@echo ">> Comprobando dependencias..."
	@command -v $(CXX) >/dev/null 2>&1 && echo "  g++ (Linux):	 OK" \
		|| { echo "  g++ (Linux):	 FALTA"; exit 1; }
	@pkg-config --exists raylib 2>/dev/null \
		&& echo "  raylib (pkg-config): OK ($$(pkg-config --modversion raylib))" \
		|| echo "  raylib (pkg-config): no detectada (se usara fallback -lraylib)"
	@command -v $(MINGW_CXX) >/dev/null 2>&1 \
		&& echo "  mingw-w64 g++:       OK" \
		|| echo "  mingw-w64 g++:       FALTA (make windows fallara)"
	@command -v zip >/dev/null 2>&1 \
		&& echo "  zip:	         OK" \
		|| echo "  zip:	         FALTA (necesario para make dist)"
	@[ -f "$(MINGW_RAYLIB_A)" ] \
		&& echo "  raylib mingw estatica: OK ($(MINGW_VENDOR))" \
		|| echo "  raylib mingw estatica: FALTA -> ejecuta 'make deps-mingw'"
	@echo ""

# ---------------------------------------------------------------------------
# Descarga y compila raylib estatica para mingw-w64 dentro del proyecto
# ---------------------------------------------------------------------------
deps-mingw:
	@echo ">> Comprobando mingw-w64..."
	@command -v $(MINGW_CC) >/dev/null 2>&1 \
		|| { echo "ERROR: $(MINGW_CC) no encontrado. Instala mingw-w64."; exit 1; }
	@echo ">> Descargando raylib..."
	@rm -rf /tmp/raylib-mingw
	@git clone --depth 1 https://github.com/raysan5/raylib.git /tmp/raylib-mingw
	@echo ">> Compilando raylib estatica para Windows (30-60s)..."
	@$(MAKE) -C /tmp/raylib-mingw/src \
		PLATFORM=PLATFORM_DESKTOP \
		OS=Windows_NT \
		CC=$(MINGW_CC) \
		AR=$(MINGW_AR) \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_BUILD_MODE=RELEASE
	@mkdir -p $(MINGW_RAYLIB_INC) $(MINGW_RAYLIB_LIB)
	@cp /tmp/raylib-mingw/src/raylib.h      $(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/rlgl.h	$(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/raymath.h     $(MINGW_RAYLIB_INC)/
	@cp /tmp/raylib-mingw/src/libraylib.a   $(MINGW_RAYLIB_LIB)/
	@echo ">> raylib mingw lista en $(MINGW_VENDOR)"
	@ls -la $(MINGW_RAYLIB_LIB)/libraylib.a

# ---------------------------------------------------------------------------
dirs:
	@mkdir -p $(BUILD_DIR)/obj-linux $(BUILD_DIR)/obj-win

# ---------------------------------------------------------------------------
# Linux
# ---------------------------------------------------------------------------
linux: $(BUILD_DIR)/$(APP_NAME)
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME)"

$(BUILD_DIR)/obj-linux/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | dirs
	@echo "  CXX   $<"
	@$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(APP_NAME): $(OBJ_LINUX)
	@echo "  LD    $@"
	@$(CXX) $(OBJ_LINUX) -o $@ $(LDFLAGS) $(RAYLIB_LIBS)

# ---------------------------------------------------------------------------
# Windows
# ---------------------------------------------------------------------------
windows: $(MINGW_RAYLIB_A) $(BUILD_DIR)/$(APP_NAME).exe
	@echo ">> OK: $(BUILD_DIR)/$(APP_NAME).exe"

# Si falta raylib estatica, se compila sola
$(MINGW_RAYLIB_A):
	@echo ">> raylib mingw no encontrada, compilando..."
	@$(MAKE) --no-print-directory deps-mingw

$(BUILD_DIR)/obj-win/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | dirs
	@echo "  MINGW $<"
	@$(MINGW_CXX) $(CXXFLAGS) -I$(MINGW_RAYLIB_INC) -c $< -o $@

$(BUILD_DIR)/$(APP_NAME).exe: $(OBJ_WIN)
	@echo "  MINGW LD $@"
	@$(MINGW_CXX) $^ -o $@ $(MINGW_LDFLAGS)

# ---------------------------------------------------------------------------
# Distribucion itch.io
# ---------------------------------------------------------------------------
dist: dist-linux dist-windows dist-web
	@echo ""
	@echo ">> Distribuciones listas:"
	@ls -la $(DIST_DIR)/*.zip 2>/dev/null || true
	@echo ""
	@echo "Sube los .zip a itch.io:"
	@echo "  $(APP_NAME)-linux.zip   -> Linux"
	@echo "  $(APP_NAME)-windows.zip -> Windows"
	@echo ""

dist-linux: linux
	@echo ">> Empaquetando Linux..."
	@rm -rf $(DIST_LINUX_DIR)
	@mkdir -p $(DIST_LINUX_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME) $(DIST_LINUX_DIR)/
	@chmod +x $(DIST_LINUX_DIR)/$(APP_NAME)
	@$(MAKE) --no-print-directory package-linux
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-linux.zip && \
		zip -qr $(APP_NAME)-linux.zip $(APP_NAME)-linux
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-linux.zip"

dist-windows: windows
	@echo ">> Empaquetando Windows..."
	@rm -rf $(DIST_WIN_DIR)
	@mkdir -p $(DIST_WIN_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME).exe $(DIST_WIN_DIR)/
	@cp libraries/libraylib.dll $(DIST_WIN_DIR)/
	@$(MAKE) --no-print-directory package-windows
	@cd $(DIST_DIR) && rm -f $(APP_NAME)-windows.zip && \
		zip -qr $(APP_NAME)-windows.zip $(APP_NAME)-windows
	@echo "   -> $(DIST_DIR)/$(APP_NAME)-windows.zip"

# ---------------------------------------------------------------------------
package-linux:
	@if [ -d "$(ASSET_DIRS)" ]; then \
		cp -r $(ASSET_DIRS) $(DIST_LINUX_DIR)/; \
		echo "   assets copiados"; \
	fi
	@echo "$(GAME_TITLE)"	        >  $(DIST_LINUX_DIR)/README.txt
	@echo "Version: $(VERSION)"	  >> $(DIST_LINUX_DIR)/README.txt
	@echo ""	                     >> $(DIST_LINUX_DIR)/README.txt
	@echo "Ejecutar:"	            >> $(DIST_LINUX_DIR)/README.txt
	@echo "  chmod +x $(APP_NAME)"       >> $(DIST_LINUX_DIR)/README.txt
	@echo "  ./$(APP_NAME)"	      >> $(DIST_LINUX_DIR)/README.txt
	@[ -f README.md ] && cp README.md $(DIST_LINUX_DIR)/ || true

package-windows:
	@if [ -d "$(ASSET_DIRS)" ]; then \
		cp -r $(ASSET_DIRS) $(DIST_WIN_DIR)/; \
		echo "   assets copiados"; \
	fi
	@echo "$(GAME_TITLE)"	        >  $(DIST_WIN_DIR)/README.txt
	@echo "Version: $(VERSION)"	  >> $(DIST_WIN_DIR)/README.txt
	@echo ""	                     >> $(DIST_WIN_DIR)/README.txt
	@echo "Ejecutar:"	            >> $(DIST_WIN_DIR)/README.txt
	@echo "  Doble clic en $(APP_NAME).exe" >> $(DIST_WIN_DIR)/README.txt
	@[ -f README.md ] && cp README.md $(DIST_WIN_DIR)/ || true
	@# Copia DLLs de runtime de mingw (por si acaso). Con -static no deberia
	@# hacer falta ninguna excepto libwinpthread, pero las incluimos por seguridad.
	@for dll in libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll; do \
		src=$$(find /usr/x86_64-w64-mingw32 /usr/lib/gcc -name "$$dll" 2>/dev/null | head -1); \
		if [ -n "$$src" ]; then \
			cp "$$src" $(DIST_WIN_DIR)/ ; \
			echo "   DLL: $$dll"; \
		fi; \
	done
	@# Comprobacion: avisa si el exe pide raylib dinamica
	@if $(MINGW_PREFIX)objdump -p $(DIST_WIN_DIR)/$(APP_NAME).exe 2>/dev/null | grep -q "DLL Name: libraylib.dll"; then \
		echo "   AVISO: el .exe pide libraylib.dll (raylib no es estatica)"; \
	fi

# ---------------------------------------------------------------------------
run: linux
	@echo ">> Ejecutando $(APP_NAME)..."
	@./$(BUILD_DIR)/$(APP_NAME)

# ---------------------------------------------------------------------------
clean:
	@echo ">> Limpiando build/ y dist/..."
	@rm -rf $(BUILD_DIR) $(DIST_DIR)

clean-vendor:
	@echo ">> Eliminando vendor/ (raylib mingw estatica)..."
	@rm -rf vendor

.PRECIOUS: $(BUILD_DIR)/obj-linux/%.o $(BUILD_DIR)/obj-win/%.o

web: $(SOURCES) $(HEADERS) web/shell.html
	@echo ">> Compilando para web (Emscripten)..."
	@mkdir -p $(WEB_BUILD)
	@$(EMCC) $(WEB_CFLAGS) $(SOURCES) $(WEB_LDFLAGS)
	@echo ">> OK: $(WEB_BUILD)/index.html"
	@echo "   Sirve con: make serve"
	@echo "   O manual: cd $(WEB_BUILD) && python3 -m http.server 8000"

serve: web
	@echo ">> Sirviendo en http://localhost:8000"
	@cd $(WEB_BUILD) && python3 -m http.server 8000

dist-web: web
	@echo ">> Empaquetando version web..."
	@rm -rf $(WEB_DIST_DIR)
	@mkdir -p $(WEB_DIST_DIR)
	@cp build/web/index.html $(WEB_DIST_DIR)/
	@cp build/web/index.js   $(WEB_DIST_DIR)/
	@cp build/web/index.wasm $(WEB_DIST_DIR)/
	@cp build/web/index.data $(WEB_DIST_DIR)/
	@if [ -f build/web/index.worker.js ]; then cp build/web/index.worker.js $(WEB_DIST_DIR)/; fi
	@echo "$(GAME_TITLE)"          >  $(WEB_DIST_DIR)/README.txt
	@echo "Version: $(VERSION)"    >> $(WEB_DIST_DIR)/README.txt
	@echo ""                       >> $(WEB_DIST_DIR)/README.txt
	@echo "Sube este zip a itch.io con 'Kind of project: HTML'" >> $(WEB_DIST_DIR)/README.txt
	@echo "No es necesario ningun servidor: itch.io lo sirve todo." >> $(WEB_DIST_DIR)/README.txt
	@[ -f README.md ] && cp README.md $(WEB_DIST_DIR)/ || true
	@cd dist && rm -f escape_from_bea-web.zip && \
		zip -qr escape_from_bea-web.zip escape_from_bea-web
	@echo "   -> $(WEB_ZIP)"
	@echo ""
	@echo "Sube $(WEB_ZIP) a itch.io:"
	@echo "  - Kind of project: HTML"
	@echo "  - 'This file will be played in the browser'"
