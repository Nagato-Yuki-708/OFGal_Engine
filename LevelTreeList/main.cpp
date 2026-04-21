// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "LevelTreeList.h"

int main() {
    LevelTreeList app;
    if (!app.Initialize()) {
        return 1;
    }
    app.Run();
    return 0;
}