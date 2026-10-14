// =====================================================================
//  map_editor.cpp — Editor TUI para los .dgn de Escape From Marisa 3
//
//  Compilar:  g++ -std=c++17 -O2 -o tools/map_editor tools/map_editor.cpp
//  Ejecutar:  ./tools/map_editor [assets/levels/level_01_map.dgn]
//
//  Formato .dgn:
//    '#' muro    'L' taquilla   '.' suelo    's' spawn
//    'i' item    'e' enemigo    'f' finish   'F' fuente
// =====================================================================
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------
//  Terminal setup
// ---------------------------------------------------------------------
static termios g_oldT;
static bool g_raw = false;

static void restoreTerm() {
  if (g_raw) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_oldT);
    g_raw = false;
  }
}

static void onSignal(int) {
  restoreTerm();
  std::printf("\x1b[?25h\x1b[0m\x1b[?1049l\n");
  std::fflush(stdout);
  _exit(1);
}

static void enterRaw() {
  tcgetattr(STDIN_FILENO, &g_oldT);
  termios t = g_oldT;
  t.c_lflag &= ~(ICANON | ECHO | ISIG);
  t.c_iflag &= ~(IXON | ICRNL);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
  g_raw = true;

  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  std::signal(SIGSEGV, onSignal);

  std::printf("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H");
  std::fflush(stdout);
}

static void leaveRaw() {
  std::printf("\x1b[?25h\x1b[0m\x1b[?1049l");
  std::fflush(stdout);
  restoreTerm();
}

static void termSize(int &w, int &h) {
  winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    w = ws.ws_col;
    h = ws.ws_row;
  } else {
    w = 80;
    h = 24;
  }
}

// ---------------------------------------------------------------------
//  Screen helpers
// ---------------------------------------------------------------------
static void moveTo(int col, int row) { std::printf("\x1b[%d;%dH", row, col); }
static void reset() { std::printf("\x1b[0m"); }
static void clearAll() { std::printf("\x1b[2J\x1b[H"); }
static void clearLine() { std::printf("\x1b[2K"); }

// Drena bytes pendientes del buffer de entrada (útil tras ESC de flechas)
static void drainInput() {
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  termios saved = t;
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
  unsigned char buf[64];
  while (read(STDIN_FILENO, buf, sizeof(buf)) > 0) { /* drop */
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &saved);
}

// ---------------------------------------------------------------------
//  Tiles
// ---------------------------------------------------------------------
struct TileDef {
  char ch;
  int fg;
  int bg;
  const char *name;
};

static const TileDef TILES[] = {
    {'#', 250, 237, "muro"},   {'.', 240, 233, "suelo"},
    {'s', 16, 40, "spawn"},    {'i', 16, 178, "item"},
    {'e', 15, 124, "enemigo"}, {'f', 16, 44, "finish"},
    {'L', 15, 25, "taquilla"}, {'F', 16, 45, "fuente"},
    {'k', 220, 94, "llave"}};
static const int NTILES = (int)(sizeof(TILES) / sizeof(TILES[0]));

static const TileDef *tileFor(char c) {
  for (int i = 0; i < NTILES; ++i)
    if (TILES[i].ch == c)
      return &TILES[i];
  return nullptr;
}

// ---------------------------------------------------------------------
//  Map
// ---------------------------------------------------------------------
struct Map {
  int w = 25, h = 20;
  std::vector<std::string> rows;

  Map() { rows.assign(h, std::string(w, '.')); }

  void resize(int nw, int nh, char fill = '.') {
    std::vector<std::string> nr(nh, std::string(nw, fill));
    int cw = std::min(w, nw), ch = std::min(h, nh);
    for (int y = 0; y < ch; ++y)
      for (int x = 0; x < cw; ++x)
        nr[y][x] = rows[y][x];
    w = nw;
    h = nh;
    rows = std::move(nr);
  }

  char at(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h)
      return '.';
    return rows[y][x];
  }
  void set(int x, int y, char c) {
    if (x < 0 || y < 0 || x >= w || y >= h)
      return;
    rows[y][x] = c;
  }
  void fillAll(char c) {
    for (auto &r : rows)
      std::fill(r.begin(), r.end(), c);
  }

  bool load(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
      return false;
    std::vector<std::string> lines;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
      std::string line = buf;
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
      if (!line.empty())
        lines.push_back(line);
    }
    std::fclose(f);
    if (lines.empty())
      return false;

    int nw = (int)lines[0].size();
    for (auto &l : lines) {
      if ((int)l.size() < nw)
        l.append(nw - l.size(), '.');
      else if ((int)l.size() > nw)
        l.resize(nw);
    }
    w = nw;
    h = (int)lines.size();
    rows = std::move(lines);
    return true;
  }

  bool save(const std::string &path) const {
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f)
      return false;
    for (auto &r : rows) {
      std::fputs(r.c_str(), f);
      std::fputc('\n', f);
    }
    std::fclose(f);
    return true;
  }
};

// ---------------------------------------------------------------------
//  Undo
// ---------------------------------------------------------------------
struct UndoRec {
  int x, y;
  char prev;
};

// ---------------------------------------------------------------------
//  Editor state
// ---------------------------------------------------------------------
struct Editor {
  Map map;
  int cursorX = 0, cursorY = 0;
  int viewX = 0, viewY = 0;
  char brush = '#';
  std::string path = "assets/levels/level_01_map.dgn";
  std::string status;
  std::vector<UndoRec> undoStack;
  bool showHelp = false;

  void pushUndo(int x, int y) {
    char prev = map.at(x, y);
    undoStack.push_back({x, y, prev});
    if (undoStack.size() > 500)
      undoStack.erase(undoStack.begin());
  }

  bool undo() {
    if (undoStack.empty())
      return false;
    UndoRec r = undoStack.back();
    undoStack.pop_back();
    map.set(r.x, r.y, r.prev);
    return true;
  }
};

// ---------------------------------------------------------------------
//  RNG global para automatizaciones
// ---------------------------------------------------------------------
static std::mt19937 &editorRng() {
  static std::mt19937 rng((uint32_t)std::time(nullptr));
  return rng;
}

// ---------------------------------------------------------------------
//  Diálogo modal (input de texto centrado)
// ---------------------------------------------------------------------
struct DialogResult {
  bool ok = false;
  std::string value;
};

static DialogResult dialogInput(const std::string &title,
                                const std::string &prompt,
                                const std::string &initial = "") {
  DialogResult res;
  std::string buf = initial;

  int W, H;
  termSize(W, H);

  int dlgW = 54;
  if (dlgW > W - 4)
    dlgW = W - 4;
  if (dlgW < 30)
    dlgW = 30;
  int dlgH = 7;
  int x0 = (W - dlgW) / 2;
  int y0 = (H - dlgH) / 2;
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;

  // Fondo
  for (int row = 0; row < dlgH; ++row) {
    moveTo(x0 + 1, y0 + row + 1);
    reset();
    std::printf("\x1b[48;5;238m");
    for (int i = 0; i < dlgW; ++i)
      std::putchar(' ');
    std::printf("\x1b[0m");
  }

  // Bordes
  moveTo(x0 + 1, y0 + 1);
  reset();
  std::printf("\x1b[38;5;226m\x1b[48;5;238m+");
  for (int i = 1; i < dlgW - 1; ++i)
    std::putchar('-');
  std::printf("+\x1b[0m");

  moveTo(x0 + 1, y0 + dlgH);
  reset();
  std::printf("\x1b[38;5;226m\x1b[48;5;238m+");
  for (int i = 1; i < dlgW - 1; ++i)
    std::putchar('-');
  std::printf("+\x1b[0m");

  for (int row = 1; row < dlgH - 1; ++row) {
    moveTo(x0 + 1, y0 + row + 1);
    reset();
    std::printf("\x1b[38;5;226m\x1b[48;5;238m|");
    moveTo(x0 + dlgW, y0 + row + 1);
    reset();
    std::printf("\x1b[38;5;226m\x1b[48;5;238m|");
  }

  // Título centrado sobre el borde superior
  int tl = (int)title.size();
  if (tl > dlgW - 4)
    tl = dlgW - 4;
  int tx = x0 + (dlgW - tl) / 2;
  moveTo(tx, y0 + 1);
  reset();
  std::printf("\x1b[38;5;231m\x1b[48;5;238m %.*s \x1b[0m", tl, title.c_str());

  // Prompt
  moveTo(x0 + 3, y0 + 2);
  reset();
  {
    std::string p = prompt;
    int maxP = dlgW - 6;
    if ((int)p.size() > maxP)
      p.resize(maxP);
    std::printf("\x1b[38;5;250m\x1b[48;5;238m%s\x1b[0m", p.c_str());
  }

  // Input
  int fieldW = dlgW - 6;
  auto drawField = [&]() {
    moveTo(x0 + 3, y0 + 4);
    reset();
    std::printf("\x1b[48;5;236m\x1b[38;5;255m ");
    std::string s = buf;
    if ((int)s.size() > fieldW - 1)
      s = s.substr(s.size() - fieldW + 1);
    std::printf("%s", s.c_str());
    for (int i = (int)s.size(); i < fieldW - 1; ++i)
      std::putchar(' ');
    std::printf(" \x1b[0m");
    std::printf("\x1b[?25h");
  };
  drawField();

  // Hint
  const char *hint = "ENTER: aceptar    ESC: cancelar";
  int hl = (int)std::strlen(hint);
  int hx = x0 + (dlgW - hl) / 2;
  moveTo(hx, y0 + dlgH - 1);
  reset();
  std::printf("\x1b[38;5;245m\x1b[48;5;238m%s\x1b[0m", hint);

  std::fflush(stdout);

  while (true) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;
    if (c == 27) {
      drainInput();
      std::printf("\x1b[?25l");
      res.ok = false;
      return res;
    }
    if (c == '\r' || c == '\n') {
      std::printf("\x1b[?25l");
      res.ok = true;
      res.value = buf;
      return res;
    }
    if (c == 127 || c == 8) {
      if (!buf.empty())
        buf.pop_back();
    } else if (c >= 32 && c < 127) {
      if ((int)buf.size() < fieldW - 1)
        buf += (char)c;
    } else {
      continue;
    }
    drawField();
    std::fflush(stdout);
  }

  std::printf("\x1b[?25l");
  return res;
}

// ---------------------------------------------------------------------
//  Entrada de modo comando (estilo vim, abajo de la pantalla)
// ---------------------------------------------------------------------
static DialogResult commandInput() {
  DialogResult res;
  std::string buf;

  int W, H;
  termSize(W, H);
  int row = H;

  auto drawCmd = [&]() {
    moveTo(1, row);
    reset();
    clearLine();
    std::printf("\x1b[48;5;238m\x1b[38;5;226m:\x1b[38;5;255m%s\x1b[0m",
                buf.c_str());
    std::printf("\x1b[?25h");
  };
  drawCmd();
  std::fflush(stdout);

  while (true) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;
    if (c == 27) {
      drainInput();
      std::printf("\x1b[?25l");
      res.ok = false;
      return res;
    }
    if (c == '\r' || c == '\n') {
      std::printf("\x1b[?25l");
      res.ok = true;
      res.value = buf;
      return res;
    }
    if (c == 127 || c == 8) {
      if (!buf.empty())
        buf.pop_back();
    } else if (c >= 32 && c < 127) {
      buf += (char)c;
    } else {
      continue;
    }
    drawCmd();
    std::fflush(stdout);
  }
  std::printf("\x1b[?25l");
  return res;
}

// ---------------------------------------------------------------------
//  Automatizaciones
// ---------------------------------------------------------------------
static int autoSpawnEnemies(Editor &ed, int count) {
  std::vector<std::pair<int, int>> floorCells;
  std::vector<std::pair<int, int>> protectedCells;

  for (int y = 0; y < ed.map.h; ++y)
    for (int x = 0; x < ed.map.w; ++x) {
      char c = ed.map.at(x, y);
      if (c == '.')
        floorCells.push_back({x, y});
      else if (c == 's' || c == 'f')
        protectedCells.push_back({x, y});
    }

  auto tooClose = [&](int x, int y) {
    for (auto &p : protectedCells) {
      int dx = x - p.first, dy = y - p.second;
      if (dx * dx + dy * dy < 25)
        return true; // 5 celdas de radio
    }
    return false;
  };

  std::shuffle(floorCells.begin(), floorCells.end(), editorRng());

  int placed = 0;
  for (auto &c : floorCells) {
    if (placed >= count)
      break;
    if (tooClose(c.first, c.second))
      continue;
    ed.map.set(c.first, c.second, 'e');
    placed++;
  }
  return placed;
}

static int autoSpawnItems(Editor &ed, int count) {
  std::vector<std::pair<int, int>> floorCells;
  for (int y = 0; y < ed.map.h; ++y)
    for (int x = 0; x < ed.map.w; ++x)
      if (ed.map.at(x, y) == '.')
        floorCells.push_back({x, y});

  std::shuffle(floorCells.begin(), floorCells.end(), editorRng());

  int placed = 0;
  for (auto &c : floorCells) {
    if (placed >= count)
      break;
    ed.map.set(c.first, c.second, 'i');
    placed++;
  }
  return placed;
}

static int autoRandomizeLockers(Editor &ed, int count) {
  std::vector<std::pair<int, int>> candidates;
  for (int y = 0; y < ed.map.h; ++y)
    for (int x = 0; x < ed.map.w; ++x) {
      if (ed.map.at(x, y) != '#')
        continue;
      // Solo muros que toquen al menos una celda no-muro
      bool adj = false;
      for (int dy = -1; dy <= 1 && !adj; ++dy)
        for (int dx = -1; dx <= 1 && !adj; ++dx) {
          if (dx == 0 && dy == 0)
            continue;
          char c = ed.map.at(x + dx, y + dy);
          if (c != '#' && c != 'L')
            adj = true;
        }
      if (adj)
        candidates.push_back({x, y});
    }

  std::shuffle(candidates.begin(), candidates.end(), editorRng());

  int placed = 0;
  for (auto &c : candidates) {
    if (placed >= count)
      break;
    ed.map.set(c.first, c.second, 'L');
    placed++;
  }
  return placed;
}

// ---------------------------------------------------------------------
//  Ejecutor de comandos
// ---------------------------------------------------------------------
static void executeCommand(Editor &ed, const std::string &cmd, bool &running) {
  std::vector<std::string> tokens;
  {
    std::string cur;
    for (char c : cmd) {
      if (c == ' ' || c == '\t') {
        if (!cur.empty()) {
          tokens.push_back(cur);
          cur.clear();
        }
      } else
        cur += c;
    }
    if (!cur.empty())
      tokens.push_back(cur);
  }
  if (tokens.empty())
    return;

  const std::string &c0 = tokens[0];

  auto argInt = [&](size_t idx, int def = -1) -> int {
    if (idx >= tokens.size())
      return def;
    return std::atoi(tokens[idx].c_str());
  };

  if (c0 == "q" || c0 == "quit" || c0 == "q!") {
    running = false;
  } else if (c0 == "w" || c0 == "write") {
    std::string p = tokens.size() > 1 ? tokens[1] : ed.path;
    if (ed.map.save(p)) {
      ed.path = p;
      ed.status = "Guardado: " + p;
    } else
      ed.status = "Error al guardar: " + p;
  } else if (c0 == "wq" || c0 == "x") {
    std::string p = tokens.size() > 1 ? tokens[1] : ed.path;
    if (ed.map.save(p)) {
      ed.path = p;
      running = false;
    } else
      ed.status = "Error al guardar: " + p;
  } else if (c0 == "e" || c0 == "edit" || c0 == "open") {
    std::string p = tokens.size() > 1 ? tokens[1] : "";
    if (p.empty()) {
      DialogResult r = dialogInput("ABRIR MAPA", "Ruta:", ed.path);
      if (!r.ok || r.value.empty())
        return;
      p = r.value;
    }
    if (ed.map.load(p)) {
      ed.path = p;
      ed.cursorX = ed.cursorY = 0;
      ed.viewX = ed.viewY = 0;
      ed.undoStack.clear();
      ed.status = "Cargado: " + p;
    } else
      ed.status = "No se pudo abrir: " + p;
  } else if (c0 == "new") {
    int nw = argInt(1, -1);
    int nh = argInt(2, -1);
    if (nw < 0) {
      DialogResult r = dialogInput("NUEVO MAPA", "Ancho (5-200):", "25");
      if (!r.ok)
        return;
      nw = std::atoi(r.value.c_str());
    }
    if (nh < 0) {
      DialogResult r = dialogInput("NUEVO MAPA", "Alto (5-200):", "20");
      if (!r.ok)
        return;
      nh = std::atoi(r.value.c_str());
    }
    if (nw < 5 || nw > 200 || nh < 5 || nh > 200) {
      ed.status = "Tamaño inválido (5-200)";
      return;
    }
    ed.map.resize(nw, nh, '.');
    ed.cursorX = ed.cursorY = 0;
    ed.viewX = ed.viewY = 0;
    ed.undoStack.clear();
    ed.status = "Nuevo mapa " + std::to_string(nw) + "x" + std::to_string(nh);
  } else if (c0 == "enemies" || c0 == "spawn-enemies") {
    int n = argInt(1, -1);
    if (n < 0) {
      DialogResult r =
          dialogInput("SPAWN ENEMIGOS", "Cantidad a colocar:", "5");
      if (!r.ok)
        return;
      n = std::atoi(r.value.c_str());
    }
    if (n <= 0) {
      ed.status = "Cantidad inválida";
      return;
    }
    int placed = autoSpawnEnemies(ed, n);
    ed.status = "Enemigos colocados: " + std::to_string(placed) + "/" +
                std::to_string(n);
  } else if (c0 == "items" || c0 == "spawn-items") {
    int n = argInt(1, -1);
    if (n < 0) {
      DialogResult r = dialogInput("SPAWN ITEMS", "Cantidad a colocar:", "5");
      if (!r.ok)
        return;
      n = std::atoi(r.value.c_str());
    }
    if (n <= 0) {
      ed.status = "Cantidad inválida";
      return;
    }
    int placed = autoSpawnItems(ed, n);
    ed.status =
        "Items colocados: " + std::to_string(placed) + "/" + std::to_string(n);
  } else if (c0 == "lockers" || c0 == "randomize") {
    int n = argInt(1, -1);
    if (n < 0) {
      DialogResult r =
          dialogInput("RANDOMIZAR TAQUILLAS", "Cuántas taquillas:", "5");
      if (!r.ok)
        return;
      n = std::atoi(r.value.c_str());
    }
    if (n <= 0) {
      ed.status = "Cantidad inválida";
      return;
    }
    int placed = autoRandomizeLockers(ed, n);
    ed.status = "Taquillas colocadas: " + std::to_string(placed) + "/" +
                std::to_string(n);
  } else if (c0 == "clear") {
    ed.undoStack.clear();
    ed.map.fillAll('.');
    ed.status = "Mapa vaciado";
  } else if (c0 == "border") {
    for (int x = 0; x < ed.map.w; ++x) {
      ed.map.set(x, 0, '#');
      ed.map.set(x, ed.map.h - 1, '#');
    }
    for (int y = 0; y < ed.map.h; ++y) {
      ed.map.set(0, y, '#');
      ed.map.set(ed.map.w - 1, y, '#');
    }
    ed.status = "Borde dibujado";
  } else if (c0 == "fill") {
    if (tokens.size() < 2) {
      ed.status = "Uso: :fill <char>";
      return;
    }
    char ch = tokens[1][0];
    ed.undoStack.clear();
    ed.map.fillAll(ch);
    ed.status = std::string("Relleno con '") + ch + "'";
  } else if (c0 == "help" || c0 == "h") {
    ed.showHelp = true;
  } else {
    ed.status = "Comando desconocido: " + c0;
  }
}

// ---------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------
static void clampView(Editor &ed, int vpW, int vpH) {
  if (ed.cursorX < ed.viewX)
    ed.viewX = ed.cursorX;
  if (ed.cursorY < ed.viewY)
    ed.viewY = ed.cursorY;
  if (ed.cursorX >= ed.viewX + vpW)
    ed.viewX = ed.cursorX - vpW + 1;
  if (ed.cursorY >= ed.viewY + vpH)
    ed.viewY = ed.cursorY - vpH + 1;

  if (ed.viewX < 0)
    ed.viewX = 0;
  if (ed.viewY < 0)
    ed.viewY = 0;
  if (ed.viewX > ed.map.w - vpW)
    ed.viewX = std::max(0, ed.map.w - vpW);
  if (ed.viewY > ed.map.h - vpH)
    ed.viewY = std::max(0, ed.map.h - vpH);
}

static void drawCell(char ch, bool cursor) {
  if (cursor) {
    std::printf("\x1b[48;5;226m\x1b[38;5;16m%c", ch);
    return;
  }
  const TileDef *t = tileFor(ch);
  int fg = 15, bg = 234;
  if (t) {
    fg = t->fg;
    bg = t->bg;
  }
  std::printf("\x1b[48;5;%dm\x1b[38;5;%dm%c", bg, fg, ch);
}

static void renderHelp(Editor &ed, int W, int H) {
  (void)ed;
  (void)H;
  clearAll();
  moveTo(1, 1);
  reset();

  std::printf("\x1b[1m\x1b[38;5;226m  MAP EDITOR — AYUDA\x1b[0m\n\n");

  std::printf("\x1b[38;5;226m--- MODO NORMAL ---\x1b[0m\n");
  std::printf("  Flechas / WASD / HJKL   Mover cursor\n");
  std::printf("  1..8                    Seleccionar brush\n");
  std::printf("  # . i e f F             Brush directo\n");
  std::printf("  Espacio                 Pintar celda\n");
  std::printf("  g                       Grab (copiar tile bajo cursor)\n");
  std::printf("  u                       Undo\n");
  std::printf("  ?                       Esta ayuda\n");
  std::printf("  :                       Modo comando (vim-like)\n\n");

  std::printf("\x1b[38;5;226m--- COMANDOS (:...) ---\x1b[0m\n");
  std::printf("  :w [ruta]               Guardar\n");
  std::printf("  :wq [ruta]              Guardar y salir\n");
  std::printf("  :q / :q!                Salir\n");
  std::printf("  :e [ruta]               Abrir mapa\n");
  std::printf("  :new [ancho alto]       Nuevo mapa\n");
  std::printf("  :enemies [n]            Colocar n enemigos aleatorios\n");
  std::printf("  :items [n]              Colocar n items aleatorios\n");
  std::printf("  :lockers [n]            Convertir n muros en taquillas\n");
  std::printf("  :border                 Dibujar borde de muros\n");
  std::printf("  :fill <char>            Rellenar todo con char\n");
  std::printf("  :clear                  Vaciar mapa a suelo\n\n");

  std::printf("\x1b[38;5;226m--- PALETA ---\x1b[0m\n");
  for (int i = 0; i < NTILES; ++i) {
    const TileDef &t = TILES[i];
    std::printf("  %d) ", i + 1);
    std::printf("\x1b[48;5;%dm\x1b[38;5;%dm %c \x1b[0m  %s\n", t.bg, t.fg, t.ch,
                t.name);
  }

  (void)W;
  std::printf("\n\x1b[38;5;245mPulsa cualquier tecla para volver.\x1b[0m");
  std::fflush(stdout);
}

static void renderEditor(Editor &ed, int W, int H) {
  int vpW = std::max(10, W - 2);
  int vpH = std::max(5, H - 9);
  clampView(ed, vpW, vpH);

  clearAll();

  // Header
  moveTo(1, 1);
  reset();
  char hdr[512];
  std::snprintf(hdr, sizeof(hdr), " Map Editor   %s   [%dx%d]", ed.path.c_str(),
                ed.map.w, ed.map.h);
  std::printf("\x1b[48;5;60m\x1b[38;5;231m%s", hdr);
  int hlen = (int)std::strlen(hdr);
  for (int i = hlen; i < W; ++i)
    std::putchar(' ');
  std::printf("\x1b[0m");

  // Borde superior
  moveTo(1, 3);
  reset();
  std::printf("\x1b[38;5;240m+");
  for (int i = 0; i < vpW; ++i)
    std::putchar('-');
  std::printf("+\x1b[0m");

  // Celdas
  for (int vy = 0; vy < vpH; ++vy) {
    moveTo(1, 4 + vy);
    reset();
    int my = ed.viewY + vy;
    std::printf("\x1b[38;5;240m|\x1b[0m");
    for (int vx = 0; vx < vpW; ++vx) {
      int mx = ed.viewX + vx;
      if (mx < ed.map.w && my < ed.map.h) {
        char c = ed.map.at(mx, my);
        bool isCur = (mx == ed.cursorX && my == ed.cursorY);
        drawCell(c, isCur);
      } else {
        std::printf("\x1b[48;5;232m ");
      }
    }
    std::printf("\x1b[38;5;240m|\x1b[0m");
  }

  // Borde inferior
  int botRow = 4 + vpH;
  moveTo(1, botRow);
  reset();
  std::printf("\x1b[38;5;240m+");
  for (int i = 0; i < vpW; ++i)
    std::putchar('-');
  std::printf("+\x1b[0m");

  // Línea de estado
  int statusRow = botRow + 1;
  char cur = ed.map.at(ed.cursorX, ed.cursorY);
  const TileDef *curT = tileFor(cur);
  const TileDef *bruT = tileFor(ed.brush);

  moveTo(1, statusRow);
  reset();
  clearLine();
  std::printf(" \x1b[38;5;250mCursor:\x1b[0m (%3d, %3d)   "
              "\x1b[38;5;250mCell:\x1b[0m ",
              ed.cursorX, ed.cursorY);
  if (curT) {
    std::printf("\x1b[48;5;%dm\x1b[38;5;%dm %c \x1b[0m %s", curT->bg, curT->fg,
                cur, curT->name);
  } else {
    std::printf("'%c'", cur);
  }

  moveTo(1, statusRow + 1);
  reset();
  clearLine();
  std::printf(" \x1b[38;5;250mBrush:\x1b[0m  ");
  if (bruT) {
    std::printf(" [%d] \x1b[48;5;%dm\x1b[38;5;%dm %c \x1b[0m %s",
                (int)(bruT - TILES) + 1, bruT->bg, bruT->fg, ed.brush,
                bruT->name);
  } else {
    std::printf(" '%c'", ed.brush);
  }

  moveTo(1, statusRow + 2);
  reset();
  clearLine();
  std::printf(" \x1b[38;5;245m"
              "Flechas/HJKL mover   Space pintar   g grab   u undo   "
              "1-8 brush   : comandos   ? ayuda\x1b[0m");

  // Mensaje de estado
  if (!ed.status.empty()) {
    moveTo(1, H);
    reset();
    clearLine();
    std::printf("\x1b[48;5;238m\x1b[38;5;226m %s \x1b[0m", ed.status.c_str());
  }

  std::fflush(stdout);
}

static void render(Editor &ed) {
  int W, H;
  termSize(W, H);
  if (W < 40 || H < 14) {
    clearAll();
    moveTo(1, 1);
    reset();
    std::printf("\x1b[38;5;196mTerminal demasiado pequeña "
                "(%dx%d). Necesito al menos 40x14.\x1b[0m",
                W, H);
    std::fflush(stdout);
    return;
  }
  if (ed.showHelp)
    renderHelp(ed, W, H);
  else
    renderEditor(ed, W, H);
}

// ---------------------------------------------------------------------
//  Input
// ---------------------------------------------------------------------
enum Key {
  K_NONE = 0,
  K_UP,
  K_DOWN,
  K_LEFT,
  K_RIGHT,
  K_ESC,
  K_ENTER,
  K_BACKSPACE,
  K_CHAR
};

struct KeyEvent {
  Key kind = K_NONE;
  char ch = 0;
};

static KeyEvent readKey() {
  KeyEvent ev;
  unsigned char c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return ev;

  if (c == 3) {
    ev.kind = K_ESC;
    return ev;
  }

  if (c == 27) {
    termios t;
    tcgetattr(STDIN_FILENO, &t);
    termios saved = t;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    unsigned char seq[2] = {0, 0};
    int n = (int)read(STDIN_FILENO, seq, 2);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);

    if (n >= 2 && seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        ev.kind = K_UP;
        return ev;
      case 'B':
        ev.kind = K_DOWN;
        return ev;
      case 'C':
        ev.kind = K_RIGHT;
        return ev;
      case 'D':
        ev.kind = K_LEFT;
        return ev;
      }
    }
    ev.kind = K_ESC;
    return ev;
  }

  if (c == '\r' || c == '\n') {
    ev.kind = K_ENTER;
    return ev;
  }
  if (c == 127 || c == 8) {
    ev.kind = K_BACKSPACE;
    return ev;
  }
  ev.kind = K_CHAR;
  ev.ch = (char)c;
  return ev;
}

// ---------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------
static void usage(const char *exe) {
  std::printf("Uso: %s [ruta/al/mapa.dgn]\n", exe);
  std::printf("     %s --help\n", exe);
}

int main(int argc, char **argv) {
  if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 ||
                   std::strcmp(argv[1], "--help") == 0)) {
    usage(argv[0]);
    return 0;
  }

  Editor ed;
  enterRaw();

  if (argc > 1) {
    if (ed.map.load(argv[1]))
      ed.path = argv[1];
  } else {
    // Arranque sin argumentos: pedir tamaño vía diálogo modal
    ed.map.resize(25, 20, '.');
    render(ed); // fondo con editor vacío

    DialogResult r1 = dialogInput("NUEVO MAPA", "Ancho (5-200):", "25");
    if (r1.ok) {
      int nw = std::atoi(r1.value.c_str());
      if (nw >= 5 && nw <= 200) {
        DialogResult r2 = dialogInput("NUEVO MAPA", "Alto (5-200):", "20");
        if (r2.ok) {
          int nh = std::atoi(r2.value.c_str());
          if (nh >= 5 && nh <= 200)
            ed.map.resize(nw, nh, '.');
        }
      }
    }
    ed.status = "Mapa nuevo. Pulsa ? para ayuda o : para comandos.";
  }

  bool running = true;
  while (running) {
    render(ed);

    KeyEvent ev = readKey();

    if (ed.showHelp) {
      ed.showHelp = false;
      continue;
    }

    switch (ev.kind) {
    case K_UP:
      ed.cursorY--;
      break;
    case K_DOWN:
      ed.cursorY++;
      break;
    case K_LEFT:
      ed.cursorX--;
      break;
    case K_RIGHT:
      ed.cursorX++;
      break;
    case K_ESC:
      break; // no-op en modo normal
    case K_CHAR: {
      char c = ev.ch;

      // --- Movimiento (HJKL vim-style + WASD) ---
      if (c == 'h' || c == 'a')
        ed.cursorX--;
      else if (c == 'l' || c == 'd')
        ed.cursorX++;
      else if (c == 'k' || c == 'w')
        ed.cursorY--;
      else if (c == 'j' || c == 's')
        ed.cursorY++;

      // --- Brush por número ---
      else if (c >= '1' && c <= '9') {
        int idx = c - '1';
        if (idx < NTILES) {
          ed.brush = TILES[idx].ch;
          ed.status =
              std::string("Brush: ") + ed.brush + " (" + TILES[idx].name + ")";
        }
      }
      // --- Brush por char ---
      else if (c == '#' || c == '.' || c == 'i' || c == 'e' || c == 'f' ||
               c == 'F') {
        ed.brush = c;
      }
      // --- Pintar ---
      else if (c == ' ') {
        char prev = ed.map.at(ed.cursorX, ed.cursorY);
        if (prev != ed.brush) {
          ed.pushUndo(ed.cursorX, ed.cursorY);
          ed.map.set(ed.cursorX, ed.cursorY, ed.brush);
        }
      }
      // --- Grab ---
      else if (c == 'g') {
        ed.brush = ed.map.at(ed.cursorX, ed.cursorY);
        ed.status = std::string("Grab: '") + ed.brush + "'";
      }
      // --- Undo ---
      else if (c == 'u') {
        if (ed.undo())
          ed.status = "Undo";
        else
          ed.status = "Nada que deshacer";
      }
      // --- Modo comando vim-like ---
      else if (c == ':') {
        DialogResult r = commandInput();
        if (r.ok && !r.value.empty())
          executeCommand(ed, r.value, running);
      }
      // --- Ayuda ---
      else if (c == '?') {
        ed.showHelp = true;
      }
      // --- Ctrl+S (save rápido) ---
      else if (c == 19) {
        if (ed.map.save(ed.path))
          ed.status = "Guardado: " + ed.path;
        else
          ed.status = "Error al guardar: " + ed.path;
      }
      // --- Ctrl+L (clear) ---
      else if (c == 12) {
        ed.undoStack.clear();
        ed.map.fillAll('.');
        ed.status = "Mapa vaciado";
      }
      break;
    }
    default:
      break;
    }

    // Clamp cursor
    if (ed.cursorX < 0)
      ed.cursorX = 0;
    if (ed.cursorY < 0)
      ed.cursorY = 0;
    if (ed.cursorX >= ed.map.w)
      ed.cursorX = ed.map.w - 1;
    if (ed.cursorY >= ed.map.h)
      ed.cursorY = ed.map.h - 1;
  }

  leaveRaw();
  return 0;
}
