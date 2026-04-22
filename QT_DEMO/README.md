# AOI Qt 测试界面

## 功能
- 打开单张图片或文件夹批量图片
- 选择检测项，调用 DLL 导出接口执行检测
- 显示检测结果图像
- 支持单步、连续运行、停止
- 预留海康/巴斯勒在线测试入口（按钮占位）

## 当前 DLL 映射
- 压盖 -> `DLL_InspCapOmni`
- 液位 -> `DLL_InspLev`
- 提手塑膜 -> `DLL_InspHandle`
- 装箱点数 -> `DLL_InspBox`
- 喷码 -> `DLL_InspCodePET`

## 构建
要求：Qt5/Qt6 + OpenCV + CMake 3.16+

```bash
cd QT_DEMO
cmake -S . -B build
cmake --build build --config Release
```

运行 `AOIQtDemo`，先选择并加载 DLL，再选择图片或文件夹即可。
