#pragma once
#include "common.h"

class Map {
private:
    int data[MAP_LENGTH][MAP_WIDTH]; // 0:空地, 1:货架, 2:出货点

public:
    Map();
    bool isWalkable(int x, int y) const;
    void setObstacle(int x, int y, int type);
    void draw() const;
    int getType(int x, int y) const;
};