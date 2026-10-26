#include "maze.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>
#include <random>
#include <stack>
#include <string>
#include <utility>

#include "assets.hpp"

// Helper para añadir un quad a un mesh (2 triángulos)
static void addQuad(std::vector<Vector3> &verts, std::vector<Vector2> &uvs,
                    std::vector<Vector3> &normals,
                    std::vector<unsigned short> &indices, Vector3 v0,
                    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 normal) {
  unsigned short base = (unsigned short)verts.size();
  verts.push_back(v0);
  verts.push_back(v1);
  verts.push_back(v2);
  verts.push_back(v3);
  uvs.push_back({0, 1});
  uvs.push_back({1, 1});
  uvs.push_back({1, 0});
  uvs.push_back({0, 0});
  for (int i = 0; i < 4; ++i)
    normals.push_back(normal);
  indices.push_back(base + 0);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base + 0);
  indices.push_back(base + 2);
  indices.push_back(base + 3);
}

static void addWallQuad(std::vector<Vector3> &verts, std::vector<Vector2> &uvs,
                        std::vector<Vector3> &normals,
                        std::vector<unsigned short> &indices, Vector3 bl,
                        Vector3 br, Vector3 tr, Vector3 tl, float vRepeat,
                        Vector3 normal) {
  unsigned short base = (unsigned short)verts.size();
  verts.push_back(bl);
  verts.push_back(br);
  verts.push_back(tr);
  verts.push_back(tl);
  uvs.push_back({0.0f, vRepeat});
  uvs.push_back({1.0f, vRepeat});
  uvs.push_back({1.0f, 0.0f});
  uvs.push_back({0.0f, 0.0f});
  for (int i = 0; i < 4; ++i)
    normals.push_back(normal);
  indices.push_back(base + 0);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base + 0);
  indices.push_back(base + 2);
  indices.push_back(base + 3);
}

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

bool Maze::circleHitsRiser(float x, float z, float r, float fromZ) const {
  float threshold = fromZ + C::STEP_MAX;
  auto check = [&](float px, float pz) -> bool {
    int cx = (int)floorf(px / C::CELL);
    int cy = (int)floorf(pz / C::CELL);
    if (cx < 0 || cy < 0 || cx >= w || cy >= h)
      return false;
    if (wallAt(cx, cy))
      return false; // muros ya los cubre circleHitsWall
    if (stairAt(cx, cy) != 0)
      return false; // rampas transitables
    return groundZAtWorld(px, pz) > threshold;
  };
  return check(x - r, z - r) || check(x + r, z - r) || check(x - r, z + r) ||
         check(x + r, z + r);
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

int Maze::heightAt(int cx, int cy) const {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return 0;
  if ((int)heights.size() != h)
    return 0;
  if ((int)heights[cy].size() != w)
    return 0;
  return heights[cy][cx];
}

int Maze::heightAtWorld(float x, float z) const {
  return heightAt((int)floorf(x / C::CELL), (int)floorf(z / C::CELL));
}

unsigned char Maze::stairAt(int cx, int cy) const {
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return 0;
  if ((int)stairs.size() != h)
    return 0;
  if ((int)stairs[cy].size() != w)
    return 0;
  return stairs[cy][cx];
}

unsigned char Maze::stairAtWorld(float x, float z) const {
  return stairAt((int)floorf(x / C::CELL), (int)floorf(z / C::CELL));
}

float Maze::floorZAtWorld(float x, float z) const {
  int cx = (int)floorf(x / C::CELL);
  int cy = (int)floorf(z / C::CELL);
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return 0.0f;
  if (wallAt(cx, cy))
    return 0.0f;
  return (float)heightAt(cx, cy) * C::HEIGHT_STEP;
}

float Maze::groundZAtWorld(float x, float z) const {
  int cx = (int)floorf(x / C::CELL);
  int cy = (int)floorf(z / C::CELL);
  if (cx < 0 || cy < 0 || cx >= w || cy >= h)
    return 0.0f;
  if (wallAt(cx, cy))
    return 0.0f;

  float baseZ = (float)heightAt(cx, cy) * C::HEIGHT_STEP;

  unsigned char sdir = stairAt(cx, cy);
  if (sdir == 0)
    return baseZ;

  int nx = cx, ny = cy;
  switch (sdir) {
  case 1:
    ny = cy - 1;
    break;
  case 2:
    nx = cx + 1;
    break;
  case 3:
    ny = cy + 1;
    break;
  case 4:
    nx = cx - 1;
    break;
  default:
    return baseZ;
  }
  if (nx < 0 || ny < 0 || nx >= w || ny >= h)
    return baseZ;
  if (wallAt(nx, ny))
    return baseZ;

  float neighborZ = (float)heightAt(nx, ny) * C::HEIGHT_STEP;
  if (neighborZ <= baseZ + 0.001f)
    return baseZ;

  float localX = x - cx * C::CELL;
  float localZ = z - cy * C::CELL;
  float t = 0.0f;
  switch (sdir) {
  case 1:
    t = 1.0f - localZ / C::CELL;
    break;
  case 2:
    t = localX / C::CELL;
    break;
  case 3:
    t = localZ / C::CELL;
    break;
  case 4:
    t = 1.0f - localX / C::CELL;
    break;
  }
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return baseZ + (neighborZ - baseZ) * t;
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

  // Localizar secciones @...
  struct Section {
    std::string name;
    int start;
    int end;
  };
  std::vector<Section> sections;
  int cellsEnd = (int)lines.size();
  {
    std::vector<int> marks;
    for (int i = 0; i < (int)lines.size(); ++i) {
      const std::string &s = lines[i];
      if (!s.empty() && s[0] == '@')
        marks.push_back(i);
    }
    if (!marks.empty())
      cellsEnd = marks[0];
    for (size_t i = 0; i < marks.size(); ++i) {
      int start = marks[i] + 1;
      int end = (i + 1 < marks.size()) ? marks[i + 1] : (int)lines.size();
      sections.push_back({lines[marks[i]], start, end});
    }
  }

  if (cellsEnd < 3)
    return false;

  int nw = (int)lines[0].size();
  int nh = cellsEnd;
  if (nw < 3 || nh < 3)
    return false;
  for (int i = 0; i < cellsEnd; ++i) {
    if ((int)lines[i].size() != nw)
      return false;
  }

  w = nw;
  h = nh;
  g.assign(h, std::vector<int>(w, 0));
  explored.assign(h, std::vector<bool>(w, false));
  wallVariant.assign(h, std::vector<unsigned char>(w, 0));
  fountains.assign(h, std::vector<bool>(w, false));
  tents.assign(h, std::vector<bool>(w, false));
  heights.assign(h, std::vector<unsigned char>(w, 0));
  stairs.assign(h, std::vector<unsigned char>(w, 0));

  extras.keys.clear();
  extras.spawns.clear();
  extras.items.clear();
  extras.enemies.clear();
  extras.signs.clear();
  extras.npcs.clear();
  extras.hasExit = false;
  extras.exit = {0, 0, 0};

  // --- Celdas ---
  for (int y = 0; y < h; ++y) {
    const std::string &row = lines[y];
    for (int x = 0; x < w; ++x) {
      char c = (x < (int)row.size()) ? row[x] : '.';
      Vector3 center = cellCenter(x, y);
      switch (c) {
      case '#':
        g[y][x] = 1;
        wallVariant[y][x] = 0;
        break;
      case 'L':
        g[y][x] = 1;
        wallVariant[y][x] = 1;
        break;
      case 'B':
        g[y][x] = 1;
        wallVariant[y][x] = 2;
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
      case 'k':
        g[y][x] = 0;
        extras.keys.push_back(center);
        break;
      case '^':
        g[y][x] = 0;
        stairs[y][x] = 1;
        break;
      case '>':
        g[y][x] = 0;
        stairs[y][x] = 2;
        break;
      case 'v':
        g[y][x] = 0;
        stairs[y][x] = 3;
        break;
      case '<':
        g[y][x] = 0;
        stairs[y][x] = 4;
        break;
      case 'T':
      case 'N':
        g[y][x] = 0;
        break;
      default:
        g[y][x] = 0;
        break;
      }
    }
  }

  // --- Secciones ---
  auto parseAlturas = [&](const Section &sec) {
    for (int y = 0; y < h; ++y) {
      int li = sec.start + y;
      if (li >= sec.end)
        break;
      const std::string &row = lines[li];
      for (int x = 0; x < w && x < (int)row.size(); ++x) {
        char c = row[x];
        if (c >= '0' && c <= '9') {
          int hv = c - '0';
          if (hv > C::MAX_HEIGHT)
            hv = C::MAX_HEIGHT;
          heights[y][x] = (unsigned char)hv;
        }
      }
    }
  };

  auto splitCommas = [](const std::string &s) -> std::vector<std::string> {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
      size_t comma = s.find(',', start);
      if (comma == std::string::npos) {
        out.push_back(s.substr(start));
        break;
      }
      out.push_back(s.substr(start, comma - start));
      start = comma + 1;
    }
    return out;
  };

  auto unescapeNewlines = [](const std::string &s) -> std::string {
    std::string r;
    for (size_t k = 0; k < s.size(); ++k) {
      if (s[k] == '\\' && k + 1 < s.size() && s[k + 1] == 'n') {
        r += '\n';
        ++k;
      } else {
        r += s[k];
      }
    }
    return r;
  };

  for (const Section &sec : sections) {
    if (sec.name == "@HEIGHTS") {
      parseAlturas(sec);
    } else if (sec.name == "@SIGNS") {
      for (int i = sec.start; i < sec.end; ++i) {
        auto parts = splitCommas(lines[i]);
        if (parts.size() < 3)
          continue;
        int cx = atoi(parts[0].c_str());
        int cy = atoi(parts[1].c_str());
        if (cx < 0 || cy < 0 || cx >= w || cy >= h)
          continue;
        std::string txt;
        for (size_t k = 2; k < parts.size(); ++k) {
          if (k > 2)
            txt += ',';
          txt += parts[k];
        }
        LevelExtras::SignData sd;
        sd.pos = cellCenter(cx, cy);
        sd.text = unescapeNewlines(txt);
        extras.signs.push_back(sd);
      }
    } else if (sec.name == "@NPCS") {
      for (int i = sec.start; i < sec.end; ++i) {
        auto parts = splitCommas(lines[i]);
        if (parts.size() < 4)
          continue;
        int cx = atoi(parts[0].c_str());
        int cy = atoi(parts[1].c_str());
        if (cx < 0 || cy < 0 || cx >= w || cy >= h)
          continue;
        std::string nm = parts[2];
        std::string txt;
        for (size_t k = 3; k < parts.size(); ++k) {
          if (k > 3)
            txt += ',';
          txt += parts[k];
        }
        LevelExtras::NpcData nd;
        nd.pos = cellCenter(cx, cy);
        nd.name = nm;
        nd.text = unescapeNewlines(txt);
        extras.npcs.push_back(nd);
      }
    }
  }

  // Salida por defecto
  if (!extras.hasExit) {
    int ex = w - 2, ey = h - 2;
    while (ex > 0 && wallAt(ex, ey))
      --ex;
    extras.exit = cellCenter(ex, ey);
    extras.hasExit = true;
  }
  // Spawn por defecto
  if (extras.spawns.empty()) {
    for (int y = 1; y < h - 1 && extras.spawns.empty(); ++y)
      for (int x = 1; x < w - 1 && extras.spawns.empty(); ++x)
        if (!wallAt(x, y))
          extras.spawns.push_back(cellCenter(x, y));
  }

  // Aplicar altura del suelo a los puntos clave
  auto applyHeight = [&](Vector3 &p) { p.y = groundZAtWorld(p.x, p.z); };
  for (auto &p : extras.spawns)
    applyHeight(p);
  for (auto &p : extras.items)
    applyHeight(p);
  for (auto &p : extras.enemies)
    applyHeight(p);
  for (auto &s : extras.signs)
    applyHeight(s.pos);
  for (auto &n : extras.npcs)
    applyHeight(n.pos);
  applyHeight(extras.exit);

  return true;
}

void Maze::buildChunks(Assets &assets) {
  unloadChunks();
  chunksW = (w + C::CHUNK_SIZE - 1) / C::CHUNK_SIZE;
  chunksH = (h + C::CHUNK_SIZE - 1) / C::CHUNK_SIZE;
  chunks.resize(chunksW * chunksH);

  Texture2D whiteTex = {};
  auto fallbackWhite = [&]() -> Texture2D {
    if (whiteTex.id == 0) {
      Image img = GenImageColor(1, 1, WHITE);
      whiteTex = LoadTextureFromImage(img);
      UnloadImage(img);
    }
    return whiteTex;
  };

  Texture2D texTile = assets.hasWallTile ? assets.wallTile : fallbackWhite();
  Texture2D texLocker = assets.hasWallLocker ? assets.wallLocker : texTile;
  Texture2D texBrick = assets.hasWallBrick ? assets.wallBrick : texTile;
  Texture2D texFloor = assets.hasFloorTile ? assets.floorTile : texTile;

  struct MeshBuf {
    std::vector<Vector3> verts;
    std::vector<Vector2> uvs;
    std::vector<Vector3> normals;
    std::vector<unsigned short> indices;
    void clear() {
      verts.clear();
      uvs.clear();
      normals.clear();
      indices.clear();
    }
    bool empty() const { return verts.empty(); }
  };

  auto levelZ = [&](int cx, int cy) -> float {
    if (cx < 0 || cy < 0 || cx >= w || cy >= h)
      return 0.0f;
    return (float)heightAt(cx, cy) * C::HEIGHT_STEP;
  };

  // Techo único para todo el mundo, en la cota más alta posible.
  const float worldCeilingY = (float)(C::MAX_HEIGHT + 1) * C::HEIGHT_STEP;

  for (int cy = 0; cy < chunksH; ++cy) {
    for (int cx = 0; cx < chunksW; ++cx) {
      Chunk &chunk = chunks[cy * chunksW + cx];

      MeshBuf bufTile, bufLocker, bufBrick, bufFloor;

      int startX = cx * C::CHUNK_SIZE;
      int startY = cy * C::CHUNK_SIZE;
      int endX = std::min(startX + C::CHUNK_SIZE, w);
      int endY = std::min(startY + C::CHUNK_SIZE, h);

      for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
          float x0 = (float)x * C::CELL;
          float x1 = (float)(x + 1) * C::CELL;
          float z0 = (float)y * C::CELL;
          float z1 = (float)(y + 1) * C::CELL;

          if (g[y][x] == 1) {
            // =====================================================
            //  MURO
            // =====================================================
            // La pared cubre desde el suelo MÁS BAJO de sus vecinos
            // hasta el techo MÁS ALTO de sus vecinos + WALL_H. Así no
            // hay huecos entre plantas y los enemigos/jugador no ven
            // "a través" de ninguna transición.
            float ownZ = levelZ(x, y);
            float baseY = ownZ;
            float topY = ownZ + C::WALL_H;

            auto extendWithNeighbor = [&](int nx2, int ny2) {
              if (wallAt(nx2, ny2))
                return;
              float nz = levelZ(nx2, ny2);
              if (nz < baseY)
                baseY = nz;
              if (nz + C::WALL_H > topY)
                topY = nz + C::WALL_H;
            };
            extendWithNeighbor(x, y - 1);
            extendWithNeighbor(x, y + 1);
            extendWithNeighbor(x - 1, y);
            extendWithNeighbor(x + 1, y);

            float vRepeat = (topY - baseY) / C::WALL_H;
            if (vRepeat < 1.0f)
              vRepeat = 1.0f;

            int variant = 0;
            if (y < (int)wallVariant.size() && x < (int)wallVariant[y].size())
              variant = wallVariant[y][x];
            MeshBuf *buf = (variant == 1)   ? &bufLocker
                           : (variant == 2) ? &bufBrick
                                            : &bufTile;

            // 4 caras laterales
            if (!wallAt(x, y - 1))
              addWallQuad(buf->verts, buf->uvs, buf->normals, buf->indices,
                          {x0, baseY, z0}, {x1, baseY, z0}, {x1, topY, z0},
                          {x0, topY, z0}, vRepeat, {0, 0, -1});
            if (!wallAt(x, y + 1))
              addWallQuad(buf->verts, buf->uvs, buf->normals, buf->indices,
                          {x1, baseY, z1}, {x0, baseY, z1}, {x0, topY, z1},
                          {x1, topY, z1}, vRepeat, {0, 0, 1});
            if (!wallAt(x - 1, y))
              addWallQuad(buf->verts, buf->uvs, buf->normals, buf->indices,
                          {x0, baseY, z1}, {x0, baseY, z0}, {x0, topY, z0},
                          {x0, topY, z1}, vRepeat, {-1, 0, 0});
            if (!wallAt(x + 1, y))
              addWallQuad(buf->verts, buf->uvs, buf->normals, buf->indices,
                          {x1, baseY, z0}, {x1, baseY, z1}, {x1, topY, z1},
                          {x1, topY, z0}, vRepeat, {1, 0, 0});

            // Tapa superior
            addQuad(buf->verts, buf->uvs, buf->normals, buf->indices,
                    {x0, topY, z1}, {x1, topY, z1}, {x1, topY, z0},
                    {x0, topY, z0}, {0, 1, 0});
          } else {
            // =====================================================
            //  SUELO / RAMPA / TECHO
            // =====================================================
            float fz = levelZ(x, y);

            unsigned char sdir = stairAt(x, y);

            bool hasRamp = false;
            float rampHighZ = fz;
            int rnx = x, rny = y;
            if (sdir != 0) {
              switch (sdir) {
              case 1:
                rny = y - 1;
                break;
              case 2:
                rnx = x + 1;
                break;
              case 3:
                rny = y + 1;
                break;
              case 4:
                rnx = x - 1;
                break;
              }
              if (rnx >= 0 && rny >= 0 && rnx < w && rny < h &&
                  !wallAt(rnx, rny)) {
                float hz = levelZ(rnx, rny);
                if (hz > fz + 0.001f) {
                  hasRamp = true;
                  rampHighZ = hz;
                }
              }
            }

            if (hasRamp) {
              // Quad inclinado con winding CCW (normal apunta arriba)
              switch (sdir) {
              case 1: // ^ alto al norte (z0)
                addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                        bufFloor.indices, {x0, fz, z1}, {x1, fz, z1},
                        {x1, rampHighZ, z0}, {x0, rampHighZ, z0}, {0, 1, 0});
                break;
              case 2: // > alto al este (x1)
                addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                        bufFloor.indices, {x0, fz, z1}, {x1, rampHighZ, z1},
                        {x1, rampHighZ, z0}, {x0, fz, z0}, {0, 1, 0});
                break;
              case 3: // v alto al sur (z1)
                addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                        bufFloor.indices, {x0, rampHighZ, z1},
                        {x1, rampHighZ, z1}, {x1, fz, z0}, {x0, fz, z0},
                        {0, 1, 0});
                break;
              case 4: // < alto al oeste (x0)
                addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                        bufFloor.indices, {x0, rampHighZ, z1}, {x1, fz, z1},
                        {x1, fz, z0}, {x0, rampHighZ, z0}, {0, 1, 0});
                break;
              }
            } else {
              // Suelo plano
              addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                      bufFloor.indices, {x0, fz, z1}, {x1, fz, z1},
                      {x1, fz, z0}, {x0, fz, z0}, {0, 1, 0});
            }

            // Techo en la cota MÁXIMA del mundo (18 m)
            addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                    bufFloor.indices, {x0, worldCeilingY, z0},
                    {x1, worldCeilingY, z0}, {x1, worldCeilingY, z1},
                    {x0, worldCeilingY, z1}, {0, -1, 0});

            // Riser hacia cada vecino MÁS BAJO (solo si no es rampa)
            if (!hasRamp) {
              auto addRiser = [&](int nx2, int ny2, Vector3 a, Vector3 b,
                                  Vector3 c, Vector3 d, Vector3 n) {
                if (wallAt(nx2, ny2))
                  return;
                float nz = levelZ(nx2, ny2);
                if (nz >= fz - 0.001f)
                  return;
                addQuad(bufFloor.verts, bufFloor.uvs, bufFloor.normals,
                        bufFloor.indices, a, b, c, d, n);
              };
              addRiser(x, y - 1, {x0, fz, z0}, {x1, fz, z0},
                       {x1, levelZ(x, y - 1), z0}, {x0, levelZ(x, y - 1), z0},
                       {0, 0, -1});
              addRiser(x, y + 1, {x1, fz, z1}, {x0, fz, z1},
                       {x0, levelZ(x, y + 1), z1}, {x1, levelZ(x, y + 1), z1},
                       {0, 0, 1});
              addRiser(x - 1, y, {x0, fz, z1}, {x0, fz, z0},
                       {x0, levelZ(x - 1, y), z0}, {x0, levelZ(x - 1, y), z1},
                       {-1, 0, 0});
              addRiser(x + 1, y, {x1, fz, z0}, {x1, fz, z1},
                       {x1, levelZ(x + 1, y), z1}, {x1, levelZ(x + 1, y), z0},
                       {1, 0, 0});
            }
          }
        }
      }

      auto buildModel = [&](MeshBuf &buf, Texture2D tex) -> Model {
        if (buf.empty())
          return {};
        Mesh m = {};
        m.vertexCount = (int)buf.verts.size();
        m.triangleCount = (int)buf.indices.size() / 3;
        m.vertices = (float *)MemAlloc(buf.verts.size() * sizeof(Vector3));
        m.texcoords = (float *)MemAlloc(buf.uvs.size() * sizeof(Vector2));
        m.normals = (float *)MemAlloc(buf.normals.size() * sizeof(Vector3));
        m.indices = (unsigned short *)MemAlloc(buf.indices.size() *
                                               sizeof(unsigned short));
        memcpy(m.vertices, buf.verts.data(),
               buf.verts.size() * sizeof(Vector3));
        memcpy(m.texcoords, buf.uvs.data(), buf.uvs.size() * sizeof(Vector2));
        memcpy(m.normals, buf.normals.data(),
               buf.normals.size() * sizeof(Vector3));
        memcpy(m.indices, buf.indices.data(),
               buf.indices.size() * sizeof(unsigned short));
        Model model = LoadModelFromMesh(m);
        if (model.materialCount > 0) {
          model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
          model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        }
        UploadMesh(&model.meshes[0], false);
        return model;
      };

      chunk.wallsTile = buildModel(bufTile, texTile);
      chunk.wallsLocker = buildModel(bufLocker, texLocker);
      chunk.wallsBrick = buildModel(bufBrick, texBrick);
      chunk.floor = buildModel(bufFloor, texFloor);
      chunk.built = true;
    }
  }
  (void)whiteTex;
}

void Maze::unloadChunks() {
  for (auto &ch : chunks) {
    Model *models[4] = {&ch.wallsTile, &ch.wallsLocker, &ch.wallsBrick,
                        &ch.floor};
    for (Model *m : models) {
      if (!m->meshes)
        continue;
      if (m->materials) {
        for (int i = 0; i < m->materialCount; ++i) {
          if (!m->materials[i].maps)
            continue;
          m->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = {};
        }
      }
      UnloadModel(*m);
      *m = {};
    }
    ch.built = false;
  }
  chunks.clear();
  chunksW = 0;
  chunksH = 0;
}
