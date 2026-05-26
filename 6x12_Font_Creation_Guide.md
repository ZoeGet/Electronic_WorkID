# 6×12 字符库创建历程

## 背景

本项目使用 ST7735 LCD 显示屏（160×128 分辨率），初始预设字体为：
- `Font_7x10`：7×10 像素
- `Font_11x18`：11×18 像素

在实际使用中发现，这两种字体大小差距较大，需要一个中间尺寸的字体。

## 需求演变

### 第一次尝试：8×12 字体
最初请求创建 8×12 字体，但由于位对齐问题，显示效果不理想。

### 第二次尝试：12×12 字体
改为请求 12×12 字体，但遇到了更多问题：
1. **字体不显示**：字体数据对齐到 bit 23，但渲染代码检查 bit 15
2. **显示乱码**：修正位对齐后，字符形状失真

### 关键转折：参考 Smart_Card 项目
在多次尝试失败后，决定参考之前成功的项目 `D:\STM32Code\Smart_Card` 中的 12×12 字体实现。

## Smart_Card 项目的字体格式

通过分析 Smart_Card 项目，发现了关键信息：

### 字体命名
Smart_Card 项目中的字体数组定义为：
```c
unsigned char asc2_1206[95][12];
```

**命名含义**：
- `asc2`：ASCII 字符集
- `1206`：12 行 × 6 列（不是 12×12！）
- 实际字体大小：**6 像素宽 × 12 像素高**

### 数据结构
```c
// 每个字符 12 行，每行 12 字节
{0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00,0x00}, // "!"
```

**关键点**：
- 每行 12 字节，但只使用第 1 个字节
- 每个字节存储 6 个像素（bit0-bit5）
- 渲染时检查 bit0，右移，循环 6 次

### 渲染逻辑
Smart_Card 项目的渲染方式：
```c
for (i = 0; i < font->height; i++) {
    unsigned char temp = fontData[(c - 32) * font->height + i];
    for (j = 0; j < 6; j++) {
        if (temp & 0x01) {
            // 绘制前景色像素
        } else {
            // 绘制背景色像素
        }
        temp >>= 1;  // 右移，检查下一个 bit
    }
}
```

## 实现步骤

### 1. 复制字体数据
从 Smart_Card 项目复制完整的 95 个 ASCII 字符（32-126）点阵数据到 `fonts.c`：

```c
static const unsigned char Font12x6_Data[95][12] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // " ",0
    {0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00,0x00}, // "!",1
    // ... 共 95 个字符
    {0x02,0x25,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // "~",94
};
```

### 2. 创建 FontDef 结构体
```c
const FontDef Font_6x12 = {6, 12, (const uint32_t*)Font12x6_Data};
```

**注意**：
- `width = 6`：实际像素宽度
- `height = 12`：实际像素高度
- `data`：指向字体数据数组

### 3. 修改渲染代码
在 `st7735.c` 中添加对 6×12 字体的特殊渲染逻辑：

```c
// 对于 12x6 字体（实际 6x12），使用与 Smart_Card 项目完全一致的渲染方式
if (font->width == 6 && font->height == 12) {
    const unsigned char *fontData = (const unsigned char *)font->data;
    for (i = 0; i < font->height; i++) {
        unsigned char temp = fontData[(c - 32) * font->height + i];
        for (j = 0; j < 6; j++) {
            if (temp & 0x01) {
                uint8_t data[] = { color >> 8, color & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            } else {
                uint8_t data[] = { bgColor >> 8, bgColor & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            }
            temp >>= 1;
        }
    }
} else {
    // 其他字体使用原有的渲染方式
    // ...
}
```

### 4. 文件整合
最初将字体数据放在单独的 `fonts_12x12.c` 文件中，但为了保持项目整洁：
- 将 `Font12x6_Data` 数组移动到 `fonts.c` 中
- 删除 `fonts_12x12.c` 文件
- 更新 `CMakeLists.txt` 移除对该文件的引用

### 5. 变量命名修正
最初命名为 `Font_12x12`，但实际字体是 6×12 像素，为避免混淆：
- 将变量名改为 `Font_6x12`
- 更新所有使用该字体的地方（`lcd_display.c` 等）

## 最终结果

成功创建了 6×12 像素的字体库，包含 95 个可打印 ASCII 字符（32-126）。

### 字体规格
- **名称**：`Font_6x12`
- **尺寸**：6 像素宽 × 12 像素高
- **字符集**：ASCII 32-126（95 个字符）
- **数据格式**：`unsigned char Font12x6_Data[95][12]`
- **渲染方式**：每行检查 bit0，右移，循环 6 次

### 可用字体
现在项目中有三种字体可供选择：
1. `Font_7x10`：7×10 像素（最小）
2. `Font_6x12`：6×12 像素（中等）
3. `Font_11x18`：11×18 像素（最大）

### 使用示例
```c
ST7735_DrawString(0, 30, "16kHz Sampling", ST7735_CYAN, ST7735_BLACK, &Font_6x12);
ST7735_DrawString(0, 50, "Waiting...", ST7735_YELLOW, ST7735_BLACK, &Font_6x12);
```

## 经验总结

### 关键教训
1. **字体命名要准确**：Smart_Card 项目的 `asc2_1206` 命名具有误导性，实际是 6 像素宽而非 12 像素
2. **数据格式必须匹配渲染逻辑**：字体数据的位对齐和存储格式必须与渲染代码完全一致
3. **参考已有成功案例**：直接复制已验证成功的代码比重新创建更可靠
4. **文件组织要清晰**：将相关代码整合到同一文件中，保持项目结构整洁

### 技术要点
1. **位操作**：使用 `temp & 0x01` 检查最低位，`temp >>= 1` 右移
2. **数据对齐**：字体数据存储在 `unsigned char` 数组中，每个字节存储一行像素
3. **字符索引**：使用 `(c - 32)` 计算字符在数组中的索引（ASCII 32 为第一个字符）
4. **渲染循环**：外层循环遍历行（height），内层循环遍历列（width）

## 参考资料

- Smart_Card 项目字体定义：`D:\STM32Code\Smart_Card\Core\Inc\font.h`
- Smart_Card 项目渲染逻辑：`D:\STM32Code\Smart_Card\Core\Src\gui.c`
- 本项目字体定义：`Core\Inc\fonts.h`
- 本项目字体数据：`Core\Src\fonts.c`
- 本项目渲染逻辑：`Core\Src\st7735.c`
