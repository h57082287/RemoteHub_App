# RemoteHub

Qt Widgets 四埠 USB Hub 介面。視窗預設為 760 × 370，以可自由旋轉的 USB-A Hub 為中心，包含金屬倒角、凹入插槽、藍色膠芯、金色接點，以及正面整圈 RGB 流動燈帶。

## 操作

- 在 Hub 上按住滑鼠左鍵拖曳：任意方向旋轉。
- 滾輪：縮放；最大尺寸會保留邊界，避免模型被裁切。
- **回到正視圖**、`Home` 或連按兩下模型：平滑轉回四個插槽朝前，並恢復縮放。
- **RGB 光效**：切換模型正面外框的燈效。
- 拖曳上方標題區：移動視窗。拖曳視窗邊緣：調整大小。
- 模型取得焦點後，可用方向鍵旋轉；`Tab` 可切換控制項。

目前是 UI 與 3D 燈效預覽，尚未連接實體 USB Hub 或韌體控制協定。

## 建置與執行

需要 C++17、Qt Widgets，以及支援 OpenGL 3.3 的顯示環境。Qt 6 額外需要 OpenGL 與 OpenGLWidgets 模組；不需要下載模型或圖片素材。

此工作環境已安裝 Qt 6.12，可執行：

```bash
/home/johnson/Qt/Tools/CMake/bin/cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/home/johnson/Qt/6.12.0/gcc_64
/home/johnson/Qt/Tools/CMake/bin/cmake --build build -j 4
./build/RemoteHub_App
```

其他環境可使用 PATH 中的 `cmake`，將 `CMAKE_PREFIX_PATH` 改為當地 Qt 安裝位置。也可以用 Qt Creator 開啟 `CMakeLists.txt`。

## 主題與玻璃效果

介面讀取系統 `QPalette` 與 Qt 的色彩模式，隨系統切換淺色／深色；按鈕與點綴色使用系統強調色。視窗本身具有 alpha 半透明、圓角、漸層高光與邊緣陰影。3D 模型保持清晰，不會一起被視窗透明度淡化。

背景模糊使用平台合成器提供的功能：Windows 11 22H2+ 會要求 DWM Acrylic；KDE 在建置環境安裝 KF6WindowSystem（Qt 5 使用 KF5WindowSystem ≥ 5.82）且合成器支援時啟用 blur-behind。GNOME、目前未整合原生材質的 macOS，以及其他不支援的桌面，保留半透明與玻璃高光，不會模糊桌面背景。實際模糊也受到作業系統的透明效果設定影響。

Qt OpenGL 透明元件的組合方式依照 [QOpenGLWidget 官方文件](https://doc.qt.io/qt-6/qopenglwidget.html#limitations-and-other-considerations)，使用 alpha buffer 與 `WA_AlwaysStackOnTop`。不要在 3D 元件的同一區域上方堆疊其他 QWidget 控制項。

## 檔案

- `mainwindow.ui`：可用 Qt Designer 編輯的視窗配置、按鈕與 promoted `HubView`。
- `mainwindow.cpp`：玻璃材質、系統主題、視窗移動與按鈕行為。
- `hubview.cpp`：程序式 3D 幾何、材質著色、深度測試、旋轉與 RGB 動畫。
- `glasswindow.cpp`：選用的平台背景模糊整合。

Qt Designer 可編輯配置；3D 模型需建置並執行程式才會顯示。

## 驗證

選用的互動測試需要 Qt Test 與可建立 OpenGL context 的顯示環境：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt \
  -DREMOTEHUB_BUILD_TESTS=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

涵蓋實際模型渲染與 alpha、滑鼠和鍵盤旋轉、正視圖復位、RGB 動畫與開關、系統主題變更，以及小視窗多角度顯示。測試預設將預覽圖存到系統暫存目錄的 `remotehub-smoke`；可用 `REMOTEHUB_TEST_OUTPUT_DIR` 指定位置。
