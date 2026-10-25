#pragma once
#include "common.hpp"
#include "maze.hpp"
#include "raycaster.hpp"
#include <random>

class AttractMode {
public:
  // El mapa del attract está hardcodeado, no requiere generación
  // ni semilla. `assets` ya no es necesario aquí.
  void init();

  // Avisa al raycaster de que se va a usar en modo attract (resolución alta,
  // filtro bilinear) para que ajuste su framebuffer. Idempotente.
  void prepareRaycaster(Raycaster &rc);

  void update(float dt);
  void draw(Raycaster &rc, int W, int H);

  void unload();

private:
  Maze maze_;
  Vector3 pos_ = {0, 0, 0};
  float yaw_ = 0.0f;
  float targetYaw_ = 0.0f;
  float dirTimer_ = 0.0f;
  float pickCd_ = 0.0f;
  float bobT_ = 0.0f;
  std::mt19937 rng_;

  void pickNewDirection();
};
