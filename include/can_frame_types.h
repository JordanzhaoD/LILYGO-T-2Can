#pragma once

#include <cstdint>
#include <cstring>

enum CanBusMask : uint8_t
{
    CAN_BUS_ANY = 0,
    CAN_BUS_CH = 1 << 0,
    CAN_BUS_VEH = 1 << 1,
    CAN_BUS_PARTY = 1 << 2,
};

#ifndef CAN_BUS_DEFAULT
#define CAN_BUS_DEFAULT CAN_BUS_ANY
#endif

struct CanFrame
{
    uint32_t id = 0;
    uint8_t dlc = 8;
    uint8_t data[8] = {};
    uint8_t bus = CAN_BUS_ANY;
};

// ── 新增帧ID常量（Phase 1 合并） ──────────────────────────
namespace CanId {
    // 车辆OTA状态（CAN-A TWAI）
    static constexpr uint32_t OTA_STATUS      = 0x318;  // data[6]&0x03==2 → OTA进行中

    // 后雾灯控制（CAN-B MCP2515）
    static constexpr uint32_t REAR_FOG_LIGHT  = 0x273;  // data[2]=0x90(亮)/0x10(灭)

    // 方向盘滚轮按钮（CAN-B MCP2515）
    static constexpr uint32_t STEERING_WHEEL  = 0x3C2;  // data[2]音量, data[3]速度
}

// 0x273 后雾灯基础帧
struct FogLightFrame {
    static constexpr uint8_t BASE[8] = {0x81, 0xE1, 0x10, 0x40, 0x0B, 0x03, 0x30, 0x12};
    static constexpr uint8_t ON_BYTE  = 0x90;  // data[2] = 雾灯亮
    static constexpr uint8_t OFF_BYTE = 0x10;  // data[2] = 雾灯灭
};

// 0x3C2 方向盘滚轮命令值
namespace WheelCmd {
    static constexpr uint8_t VOL_UP      = 0x01;  // data[2] 音量上
    static constexpr uint8_t VOL_DOWN    = 0x3F;  // data[2] 音量下
    static constexpr uint8_t RELEASE     = 0x00;  // data[2/3] 回弹/释放
    static constexpr uint8_t SPEED_UP    = 0x01;  // data[3] 速度上
    static constexpr uint8_t SPEED_DOWN  = 0x3F;  // data[3] 速度下
}
