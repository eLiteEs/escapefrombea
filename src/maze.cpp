#include "maze.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <random>
#include <stack>
#include <string>
#include <utility>

void Maze::generate(int mw, int mh, uint32_t seed) {
  w = mw;
  h = mh;
  g.assign(h, std::vector<int>(w, 1));
  explored.assign(h, std::vector<bool>(w, false));
  tents.assign(h, std::vector<bool>(w, false)); // NUEVO

  std::mt19937 rng(seed);
  std::vector<std::vector<bool>> vis(h, std::vector<bool>(w, false));
  std::stack<std::pair<int, int>> st;
  st.push({1, 1});
  vis[1][1] = true;
  g[1][1] = 0;

  const int DX[4] = {2, -2, 0, 0};
  const int DY[4] = {0, 0, 2, -2};

  while (!st.empty()) {
    auto cur = st.top();
    int cx = cur.first, cy = cur.second;
    std::vector<std::pair<int, int>> opts;
    for (int i = 0; i < 4; ++i) {
      int nx = cx + DX[i], ny = cy + DY[i];
      if (nx > 0 && ny > 0 && nx < w - 1 && ny < h - 1 && !vis[ny][nx])
        opts.push_back({nx, ny});
    }
    if (opts.empty()) {
      st.pop();
      continue;
    }
    auto n = opts[rng() % opts.size()];
    g[(cy + n.second) / 2][(cx + n.first) / 2] = 0;
    g[n.second][n.first] = 0;
    vis[n.second][n.first] = true;
    st.push(n);
  }

  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      if (g[y][x] != 1)
        continue;
      bool lr = (g[y][x - 1] == 0 && g[y][x + 1] == 0);
      bool ud = (g[y - 1][x] == 0 && g[y + 1][x] == 0);
      if ((lr || ud) && (rng() % 100) < 18)
        g[y][x] = 0;
    }

  // ---------- Variantes de pared ----------
  wallVariant.assign(h, std::vector<unsigned char>(w, 0));
  std::mt19937 varRng(seed ^ 0x9E3779B9u);
  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      if (g[y][x] != 1)
        continue;

      int exposed = 0;
      if (!wallAt(x - 1, y))
        exposed++;
      if (!wallAt(x + 1, y))
        exposed++;
      if (!wallAt(x, y - 1))
        exposed++;
      if (!wallAt(x, y + 1))
        exposed++;
      if (exposed == 0)
        continue;

      int r = (int)(varRng() % 100);
      if (r < 8)
        wallVariant[y][x] = 1; // taquillas
      else if (r < 12)
        wallVariant[y][x] = 2; // ladrillo
      else
        wallVariant[y][x] = 0; // azulejo
    }

  // ---------- Fuentes ----------
  fountains.assign(h, std::vector<bool>(w, false));

  std::vector<std::pair<int, int>> deadEnds;
  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      if (g[y][x] != 0)
        continue;
      int free = 0;
      if (!wallAt(x - 1, y))
        free++;
      if (!wallAt(x + 1, y))
        free++;
      if (!wallAt(x, y - 1))
        free++;
      if (!wallAt(x, y + 1))
        free++;
      if (free == 1)
        deadEnds.push_back({x, y});
    }

  int numFountains = (w * h >= 400) ? 2 : 1;
  if ((int)deadEnds.size() < numFountains)
    numFountains = (int)deadEnds.size();

  std::shuffle(deadEnds.begin(), deadEnds.end(), varRng);
  for (int i = 0; i < numFountains; ++i) {
    if (deadEnds[i].first == 1 && deadEnds[i].second == 1)
      continue;
    fountains[deadEnds[i].second][deadEnds[i].first] = true;
  }
}

// ---------- Paredes ----------
bool Maze::wallAt(int cx, int cy) const {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return true;
  return g[cy][cx] == 1;
}

bool Maze::wallAtWorld(float x, float z) const {
  return wallAt((int)floorf(x / C::CELL), (int)floorf(z / C::CELL));
}

bool Maze::circleHitsWall(float x, float z, float r) const {
  return wallAtWorld(x - r, z - r) || wallAtWorld(x + r, z - r) ||
         wallAtWorld(x - r, z + r) || wallAtWorld(x + r, z + r);
}

// ---------- Tiendas ----------
bool Maze::hasTent(int cx, int cy) const {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return false;
  if ((int)tents.size() != h)
    return false;
  return tents[cy][cx];
}

bool Maze::hasTentWorld(float x, float z) const {
  return hasTent((int)floorf(x / C::CELL), (int)floorf(z / C::CELL));
}

void Maze::placeTent(int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return;
  if ((int)tents.size() != h)
    return;
  tents[cy][cx] = true;
}

void Maze::removeTent(int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return;
  if ((int)tents.size() != h)
    return;
  tents[cy][cx] = false;
}

// ---------- Colisión "bloqueante" (paredes + tiendas) ----------
bool Maze::blockedAt(int cx, int cy) const {
  return wallAt(cx, cy) || hasTent(cx, cy);
}

bool Maze::blockedAtWorld(float x, float z) const {
  return blockedAt((int)floorf(x / C::CELL), (int)floorf(z / C::CELL));
}

bool Maze::circleHitsBlocked(float x, float z, float r) const {
  return blockedAtWorld(x - r, z - r) || blockedAtWorld(x + r, z - r) ||
         blockedAtWorld(x - r, z + r) || blockedAtWorld(x + r, z + r);
}

// ---------- Fuentes ----------
bool Maze::hasFountain(int cx, int cy) const {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return false;
  if ((int)fountains.size() != h)
    return false;
  return fountains[cy][cx];
}

// ---------- Utilidades ----------
Vector3 Maze::cellCenter(int cx, int cy) const {
  return {cx * C::CELL + C::CELL * 0.5f, 0.0f, cy * C::CELL + C::CELL * 0.5f};
}

Vector2 Maze::worldToCell(float x, float z) const {
  return {floorf(x / C::CELL), floorf(z / C::CELL)};
}

void Maze::markExploredCell(int cx, int cy, int radius) {
  for (int y = cy - radius; y <= cy + radius; ++y)
    for (int x = cx - radius; x <= cx + radius; ++x) {
      if (x < 0 || y < 0 || x >= w || y >= h)
        continue;
      int dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= radius * radius)
        explored[y][x] = true;
    }
}

void Maze::markExplored(float wx, float wz, int radius) {
  Vector2 c = worldToCell(wx, wz);
  markExploredCell((int)c.x, (int)c.y, radius);
}

bool Maze::findPath(Vector2 fromCell, Vector2 toCell,
                    std::vector<Vector2> &out) const {
  out.clear();
  int sx = (int)fromCell.x, sy = (int)fromCell.y;
  int ex = (int)toCell.x, ey = (int)toCell.y;
  if (wallAt(sx, sy) || wallAt(ex, ey))
    return false;
  if (sx == ex && sy == ey)
    return true;

  std::vector<std::vector<int>> prev(h, std::vector<int>(w, -1));
  std::queue<std::pair<int, int>> q;
  q.push({sx, sy});
  prev[sy][sx] = sy * w + sx;

  const int DX[4] = {1, -1, 0, 0};
  const int DY[4] = {0, 0, 1, -1};
  bool found = false;

  while (!q.empty()) {
    auto cur = q.front();
    q.pop();
    int cx = cur.first, cy = cur.second;
    if (cx == ex && cy == ey) {
      found = true;
      break;
    }
    for (int i = 0; i < 4; ++i) {
      int nx = cx + DX[i], ny = cy + DY[i];
      if (wallAt(nx, ny))
        continue; // <-- se queda como pared, ver nota
      if (prev[ny][nx] != -1)
        continue;
      prev[ny][nx] = cy * w + cx;
      q.push({nx, ny});
    }
  }
  if (!found)
    return false;

  int cx = ex, cy = ey;
  while (!(cx == sx && cy == sy)) {
    out.push_back({(float)cx, (float)cy});
    int p = prev[cy][cx];
    cx = p % w;
    cy = p / w;
  }
  std::reverse(out.begin(), out.end());
  return true;
}

bool Maze::loadFromFile(const char *path, LevelExtras &extras) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;

  std::vector<std::string> lines;
  char buf[4096];
  while (fgets(buf, sizeof(buf), f)) {
    std::string line = buf;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
      line.pop_back();
    if (!line.empty())
      lines.push_back(line);
  }
  fclose(f);

  if (lines.empty())
    return false;

  int nw = (int)lines[0].size();
  int nh = (int)lines.size();
  if (nw < 3 || nh < 3)
    return false; // mapa demasiado pequeño
  for (auto &l : lines)
    if ((int)l.size() != nw)
      return false; // filas de longitud desigual

  w = nw;
  h = nh;

  g.assign(h, std::vector<int>(w, 0));
  explored.assign(h, std::vector<bool>(w, false));
  wallVariant.assign(h, std::vector<unsigned char>(w, 0));
  fountains.assign(h, std::vector<bool>(w, false));
  tents.assign(h, std::vector<bool>(w, false));

  extras.spawns.clear();
  extras.items.clear();
  extras.enemies.clear();
  extras.hasExit = false;
  extras.exit = {0, 0, 0};

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      char c = lines[y][x];
      Vector3 center = cellCenter(x, y);

      switch (c) {
      case '#':
        g[y][x] = 1;
        wallVariant[y][x] = 0;
        break;
      case 'L':
        g[y][x] = 1;
        wallVariant[y][x] = 1; // taquilla
        break;
      case '.':
      case ' ':
        g[y][x] = 0;
        break;
      case 's':
        g[y][x] = 0;
        extras.spawns.push_back(center);
        break;
      case 'i':
        g[y][x] = 0;
        extras.items.push_back(center);
        break;
      case 'e':
        g[y][x] = 0;
        extras.enemies.push_back(center);
        break;
      case 'f':
        g[y][x] = 0;
        extras.hasExit = true;
        extras.exit = center;
        break;
      case 'F':
        g[y][x] = 0;
        fountains[y][x] = true;
        break;
      default:
        // Cualquier otra cosa -> suelo (permite decorar con letras libres)
        g[y][x] = 0;
        break;
      }
    }
  }

  // Si no hay salida, colocar una por defecto en la esquina inferior derecha
  if (!extras.hasExit) {
    int ex = w - 2, ey = h - 2;
    while (ex > 0 && wallAt(ex, ey))
      --ex;
    extras.exit = cellCenter(ex, ey);
    extras.hasExit = true;
  }

  // Si no hay spawns, colocar uno por defecto en la primera celda libre
  if (extras.spawns.empty()) {
    for (int y = 1; y < h - 1 && extras.spawns.empty(); ++y)
      for (int x = 1; x < w - 1 && extras.spawns.empty(); ++x)
        if (!wallAt(x, y))
          extras.spawns.push_back(cellCenter(x, y));
  }

  return true;
}
