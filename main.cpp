#include <iostream>
#include "_EventBus.h"
#include "BMP_Reader.h"
#include "cmdDrawer.h"

int main() {
	// 1. 加载图片
	BMP_Data img = Read_A_BMP("E:\\Projects\\C++Projects\\OFGal_Engine\\莉可丽丝1.bmp");
	if (img.width <= 0 || img.height <= 0) {
		std::cout << "图片加载失败" << std::endl;
		system("pause");
		return 1;
	}
	std::cout << "图片加载成功: " << img.width << " x " << img.height << std::endl;

	// 2. 创建绘图器并初始化控制台
	CmdDrawer drawer;
	if (!drawer.initialize()) {
		std::cerr << "控制台初始化失败" << std::endl;
		system("pause");
		return 1;
	}

	// 清屏（可选，也可以由绘图器内部完成）
	system("cls");

	// 3. 绘制图片
	drawer.draw(img);

	std::cout << "\n绘制完成。按 Enter 键退出..." << std::endl;
	system("pause");
	return 0;
}