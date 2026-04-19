/*地图数据*/
#pragma once
#include "common.h"

class Map {
private:
    int data[MAP_LENGTH][MAP_WIDTH]; // 0:空地, 1:货架, 2:出货点
public:
    Map();      //构造函数，用来初始化地图数据。
    bool isWalkable(float x, float y) const;        //判断指定位置是否可行走（即不是障碍物）
    void setObstacle(float x,  float y, int type);       //设置指定位置的障碍物类型（0：空地，1：货架，2：出货点）
    void draw() const; // 调用 EasyX 画出静态背景
};