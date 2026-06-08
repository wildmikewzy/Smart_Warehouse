/* 全局地图资源与拓扑渲染引擎接口 */
#include <graphics.h>
#include <cstring>
#include "Map.h"

/**
 * @brief Map 类的构造函数
 * @note 默认构建空闲拓扑网络。将 20x20 离散状态矩阵全部初始化为 0（标称空地区域），
 * 为后续静态障碍物注入、货架部署以及时空轨迹标定提供高纯净度的底层矩阵基底。
 */
Map::Map() {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            data[i][j] = 0;
        }
    }
}

/**
 * @brief 动态可行走性区域安全拓扑校验函数
 * @param x 待校验网格点在物理场景中的横向绝对离散坐标
 * @param y 待校验网格点在物理场景中的纵向绝对离散坐标
 * @retval bool 若当前格点属于标称空闲区域或合法物理出货接驳口返回 true，否则返回 false
 */
bool Map::isWalkable(int x, int y) const {
    if (x < 0 || x >= MAP_LENGTH || y < 0 || y >= MAP_WIDTH) {
        return false; // 越界直接触发边界软拦截，判定为不可通行
    }
    // 逻辑判定：0 代表标称无阻碍可行走网络，2 代表全局出货接驳口（允许智能体执行清退或接驳切入）
    return (data[x][y] == 0 || data[x][y] == 2);
}

/**
 * @brief 静态网格状态注入与障碍物类型配置接口
 * @param x 目标网格点的横向绝对离散坐标
 * @param y 目标网格点的纵向绝对离散坐标
 * @param type 赋予当前网格点的特定拓扑元素类别（0:空地, 1:货架障碍物, 2:出货接驳点）
 */
void Map::setObstacle(int x, int y, int type) {
    if (x >= 0 && x < MAP_LENGTH && y >= 0 && y < MAP_WIDTH) {
        data[x][y] = type;
    }
}

/**
 * @brief 离散网格物理属性访问器
 * @param x 查询网格点的横向绝对离散坐标
 * @param y 查询网格点的纵向绝对离散坐标
 * @retval int 返回当前网格点的物理拓扑类型代码；若输入的空间坐标发生越界，安全返回 -1
 */
int Map::getType(int x, int y) const {
    if (x < 0 || x >= MAP_LENGTH || y < 0 || y >= MAP_WIDTH) return -1;
    return data[x][y];
}

/** * @brief 全局静态地图高频图形学渲染与 ABC 热度分类可视化引擎
 * @note 1. 自动计算离散网格在视口中的绝对物理像素投影边界；
 * 2. 同步上游仓储管理系统（WMS）中基于曼哈顿距离的空间流频（ABC热度）拓扑度量算子对货架进行色彩分级；
 * 3. 采用伪三维几何浮雕特征投影技术渲染立体仓储货架单元；
 * 4. 针对出货口（Output Node）进行高明度圆角矩形与文本图层语义强标注渲染。
 */
void Map::draw() const {
    for (int i = 0; i < MAP_LENGTH; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            // 计算当前网格点在 GUI 视口中的物理像素左上角与右下角投影边界
            int left = i * GRID_SIZE;
            int top = j * GRID_SIZE;
            int right = (i + 1) * GRID_SIZE;
            int bottom = (j + 1) * GRID_SIZE;

            // ====================================================================
            // 1. 静态货架仓储单元（Shelf Unit）分级立体渲染分支
            // ====================================================================
            if (data[i][j] == 1) {
                // 核心度量：解算当前货架格点到收发货作业中心中心点 (19, 10) 的一阶曼哈顿空间绝对物理距离
                int dist = abs(i - 19) + abs(j - 10);

                // 声明高阶物理仓储拓扑单元实体表层色（Main）与三维空间浮雕特征阴影基底色（Shadow）
                COLORREF colorShadow = RGB(80, 80, 80);
                COLORREF colorMain = RGB(150, 150, 150);

                // 根据空间拓扑距离区间，执行运筹学 ABC 商品流频周转率的热度特征映射
                if (dist <= 12) {
                    // 区域一：高频核心作业区（A类仓储单元） -> 采用高对比度暖橙色色彩矩阵表示
                    colorShadow = RGB(210, 120, 40);
                    colorMain = RGB(255, 185, 110);
                }
                else if (dist <= 20) {
                    // 区域二：次高频周转缓冲区（B类仓储单元） -> 采用中置调和蓝色色彩矩阵表示
                    colorShadow = RGB(85, 140, 185);
                    colorMain = RGB(150, 206, 245);
                }
                else {
                    // 区域三：低频呆滞周转区（C类仓储单元） -> 采用低饱和度灰青色色彩矩阵表示
                    colorShadow = RGB(140, 145, 150);
                    colorMain = RGB(205, 212, 215);
                }

                // 步骤 1.1：计算并投影三维空间浮雕特征阴影基底，构筑纵深立体视觉特征
                setlinecolor(colorShadow);
                setfillcolor(colorShadow);
                fillrectangle(left + 5, top + 5, right + 3, bottom + 3);

                // 步骤 1.2：绘制高阶物理仓储拓扑单元实体表层，完成最终视口栅格装配
                setlinecolor(colorMain);
                setfillcolor(colorMain);
                fillrectangle(left + 3, top + 3, right - 2, bottom - 2);
            }
            // ====================================================================
            // 2. 物流主出货口（Output Node）高亮语义强标注分支
            // ====================================================================
            else if (data[i][j] == 2) {
                // 步骤 2.1：动态绘制双层高饱和度拓扑绿色圆角矩形，表征物理出货真空安全区
                setlinecolor(RGB(100, 180, 100));
                setfillcolor(RGB(100, 180, 100));
                solidroundrect(left + 6, top + 6, right - 2, bottom - 2, 8, 8);

                setlinecolor(RGB(144, 238, 144));
                setfillcolor(RGB(144, 238, 144));
                solidroundrect(left + 2, top + 2, right - 6, bottom - 6, 8, 8);

                // 步骤 2.2：启动透明文本图层覆盖，执行红字“OUT”语义指示符的强写入，提供全视口高辨识度
                settextcolor(RED);
                settextstyle(14, 0, _T("Arial"));
                setbkmode(TRANSPARENT);
                outtextxy(left + 8, top + 10, _T("OUT"));
            }
        }
    }
}