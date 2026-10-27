#include <unity.h>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include "dash_legacy_stalk_burst.h"

// 锁定 Legacy Model X 0x292 STW_ACTN_RQ 拨杆爆闪模块的 bit 语义与 CRC 行为。
// bit 布局来自 opendbc（HiBm=data[2]bit2-3, Turn=data[2]bit0-1, MC=data[6]bit4-7,
// CRC=data[7]）。CRC 默认 SAE J1850(0x1D/0xFF/0xFF)——台架用真实 0x292 帧复验，
// 若不符改 dash_legacy_stalk_burst.h 顶部三个 constexpr（这些测试随之更新期望值）。

void setUp() {}
void tearDown() {}

// CRC8/SAE-J1850 已知向量。期望值由独立 Python 实现算得（该校验实现自身对
// "123456789" 产出 0x4B，即 SAE J1850 规范校验值），非循环。
void test_crc8_sae_j1850_vectors()
{
    uint8_t v[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};
    TEST_ASSERT_EQUAL_UINT8(0xB1, legacy_stalk::crc8(v));
    uint8_t z[7] = {0};
    TEST_ASSERT_EQUAL_UINT8(0x0A, legacy_stalk::crc8(z));
}

void test_crc8_is_deterministic_and_sensitive()
{
    uint8_t a[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};
    uint8_t b[7];
    memcpy(b, a, 7);
    TEST_ASSERT_EQUAL_UINT8(legacy_stalk::crc8(a), legacy_stalk::crc8(b)); // determinism
    b[0] ^= 0x01;                                                          // flip 1 bit
    TEST_ASSERT_NOT_EQUAL(legacy_stalk::crc8(a), legacy_stalk::crc8(b));   // sensitivity
}

// setter/getter 往返，且不破坏相邻字段。
void test_hibm_setget_preserves_turn()
{
    uint8_t f[8] = {0};
    legacy_stalk::setTurn(f, kLegacyTurnLeft);     // data[2] bit0-1
    legacy_stalk::setHibm(f, kLegacyHibmFlash);    // data[2] bit2-3
    TEST_ASSERT_EQUAL_UINT8(kLegacyHibmFlash, legacy_stalk::hibmStatus(f));
    TEST_ASSERT_EQUAL_UINT8(kLegacyTurnLeft, legacy_stalk::turnStatus(f)); // 未被破坏
}

void test_counter_setget_preserves_low_nibble()
{
    uint8_t f[8] = {0};
    f[6] = 0x05;                                    // low nibble 保留
    legacy_stalk::setCounter(f, 0x0B);              // high nibble
    TEST_ASSERT_EQUAL_UINT8(0x0B, legacy_stalk::counter(f));
    TEST_ASSERT_EQUAL_UINT8(0x05, static_cast<uint8_t>(f[6] & 0x0F)); // low nibble 不变
}

void test_setters_mask_to_field_width()
{
    uint8_t f[8] = {0};
    legacy_stalk::setHibm(f, 0xFF); // 0xFF & 0x03 = 3（SNA），证明 setter 掩码到 2 位
    TEST_ASSERT_EQUAL_UINT8(3, legacy_stalk::hibmStatus(f));
    legacy_stalk::setTurn(f, 0xFF); // 0xFF & 0x03 = 3
    TEST_ASSERT_EQUAL_UINT8(3, legacy_stalk::turnStatus(f));
    legacy_stalk::setCounter(f, 0xFF); // 0xFF & 0x0F = 15
    TEST_ASSERT_EQUAL_UINT8(0x0F, legacy_stalk::counter(f));
}

// buildFrame：端到端变异模板，写 hibm/turn/counter，重算 CRC，保留其它字节。
void test_buildFrame_writes_fields_and_preserves_template()
{
    uint8_t tmpl[8] = {0xAA, 0xBB, 0x00, 0xCC, 0xDD, 0xEE, 0x05, 0x00};
    uint8_t out[8];
    legacy_stalk::buildFrame(tmpl, out, 0x0B, kLegacyHibmFlash, kLegacyTurnIdle);

    // 保留：模板未被修改
    TEST_ASSERT_EQUAL_UINT8(0x00, tmpl[2]);
    TEST_ASSERT_EQUAL_UINT8(0x05, tmpl[6]);
    // 保留：输出里模板的非字段字节
    TEST_ASSERT_EQUAL_UINT8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, out[3]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, out[4]);
    TEST_ASSERT_EQUAL_UINT8(0xEE, out[5]);
    TEST_ASSERT_EQUAL_UINT8(0x05, static_cast<uint8_t>(out[6] & 0x0F)); // low nibble 保留
    // 写入的字段
    TEST_ASSERT_EQUAL_UINT8(kLegacyHibmFlash, legacy_stalk::hibmStatus(out));
    TEST_ASSERT_EQUAL_UINT8(kLegacyTurnIdle, legacy_stalk::turnStatus(out));
    TEST_ASSERT_EQUAL_UINT8(0x0B, legacy_stalk::counter(out));
}

// buildFrame 产出的 CRC 必须自洽：data[7] == crc8(data[0..6])。
void test_buildFrame_crc_self_consistent()
{
    uint8_t tmpl[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00};
    uint8_t out[8];
    legacy_stalk::buildFrame(tmpl, out, 0x09, kLegacyHibmIdle, kLegacyTurnRight);
    TEST_ASSERT_EQUAL_UINT8(legacy_stalk::crc8(out), out[7]);
    // 不同 hibm 应产生不同 CRC（除非巧合；用 Idle vs Flash 足够区分）
    uint8_t out2[8];
    legacy_stalk::buildFrame(tmpl, out2, 0x09, kLegacyHibmFlash, kLegacyTurnRight);
    TEST_ASSERT_EQUAL_UINT8(legacy_stalk::crc8(out2), out2[7]);
    TEST_ASSERT_NOT_EQUAL(out[7], out2[7]);
}

// 计数器回绕：0x0F + 1 → 0x00。
void test_counter_wraps()
{
    uint8_t f[8] = {0};
    legacy_stalk::setCounter(f, 0x0F);
    uint8_t next = static_cast<uint8_t>((legacy_stalk::counter(f) + 1) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(0, next);
}

// LegacyStalkTemplate 新鲜度逻辑。
void test_template_freshness()
{
    LegacyStalkTemplate t;
    uint8_t f[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    TEST_ASSERT_FALSE(t.isFresh(1000, 300)); // 未捕获
    t.capture(f, 1000);
    TEST_ASSERT_TRUE(t.isFresh(1000, 300));
    TEST_ASSERT_TRUE(t.isFresh(1300, 300));  // 边界：正好 300ms 仍新鲜
    TEST_ASSERT_FALSE(t.isFresh(1301, 300)); // 过期
    t.invalidate();
    TEST_ASSERT_FALSE(t.isFresh(1000, 300));
}

// inWake 尾流窗口：碰撞降噪的核心——只在真实帧后 [min,max]ms 内注入，
// 避开下一个真实帧（0x292~50ms / 0x238~100ms 节奏，窗口远小于间隔）。
void test_template_inWake_window()
{
    LegacyStalkTemplate t;
    uint8_t f[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    TEST_ASSERT_FALSE(t.inWake(1000, 3, 20)); // 未捕获
    t.capture(f, 1000);
    TEST_ASSERT_FALSE(t.inWake(1002, 3, 20)); // age 2 < min 3（真实帧刚结束，不发）
    TEST_ASSERT_TRUE(t.inWake(1003, 3, 20));  // age 3 边界
    TEST_ASSERT_TRUE(t.inWake(1010, 3, 20));  // age 10 在尾流窗口
    TEST_ASSERT_TRUE(t.inWake(1020, 3, 20));  // age 20 边界
    TEST_ASSERT_FALSE(t.inWake(1021, 3, 20)); // age 21 > max 20（接近下一帧，不发）
    // ageMs 辅助
    TEST_ASSERT_EQUAL_UINT32(10, t.ageMs(1010));
}

void test_inWake_window_smaller_than_real_cadence()
{
    // 语义锁定：尾流窗口 [3,20]ms 必须远小于真实帧间隔(~50ms)，
    // 保证注入落在两个真实帧之间，不与下一帧同时发。
    LegacyStalkTemplate t;
    uint8_t f[8] = {};
    t.capture(f, 1000); // 非 0 基线（lastMs==0 是"未捕获"哨兵）
    // 真实帧每 50ms：注入只允许在真实帧后 3-20ms（尾流），21-49ms 禁止（下一帧临近）
    for (uint32_t age = 3; age <= 20; ++age)
        TEST_ASSERT_TRUE(t.inWake(1000 + age, 3, 20));
    for (uint32_t age : {(uint32_t)21, (uint32_t)30, (uint32_t)49})
        TEST_ASSERT_FALSE(t.inWake(1000 + age, 3, 20));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_sae_j1850_vectors);
    RUN_TEST(test_crc8_is_deterministic_and_sensitive);
    RUN_TEST(test_hibm_setget_preserves_turn);
    RUN_TEST(test_counter_setget_preserves_low_nibble);
    RUN_TEST(test_setters_mask_to_field_width);
    RUN_TEST(test_buildFrame_writes_fields_and_preserves_template);
    RUN_TEST(test_buildFrame_crc_self_consistent);
    RUN_TEST(test_counter_wraps);
    RUN_TEST(test_template_freshness);
    RUN_TEST(test_template_inWake_window);
    RUN_TEST(test_inWake_window_smaller_than_real_cadence);
    return UNITY_END();
}
