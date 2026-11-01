// ---------------------------------------------------------------------------
// Titik masuk aplikasi. Seluruh antarmuka berbahasa Indonesia.
// ---------------------------------------------------------------------------
#include "core/App.hpp"

int main(int, char**) {
  App app;
  if (!app.init()) return 1;
  return app.run();
}
