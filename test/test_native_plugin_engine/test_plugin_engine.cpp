#include <unity.h>
#include "dash_plugin_engine.h"
#include "drivers/mock_driver.h"

static MockDriver driver;

static CanFrame frame2047(uint8_t mux = 3)
{
    CanFrame f{};
    f.id = 2047;
    f.bus = CAN_BUS_DEFAULT;
    f.dlc = 8;
    f.data[0] = mux;
    return f;
}

static CanFrame frame921()
{
    CanFrame f{};
    f.id = 921;
    f.bus = CAN_BUS_DEFAULT;
    f.dlc = 8;
    return f;
}

static DashPluginContext allowedContext()
{
    DashPluginContext ctx{};
    ctx.canActive = true;
    ctx.otaAllowed = true;
    ctx.apGateAllowed = true;
    ctx.fsdMasterEnabled = true;
    ctx.defaultBus = CAN_BUS_DEFAULT;
    return ctx;
}

void setUp()
{
    driver.reset();
}

void tearDown() {}

void test_valid_official_plugin_installs_disabled_by_default()
{
    DashPluginEngine engine;
    const char *json = R"json({
      "name":"My Plugin",
      "version":"1.0",
      "author":"Tester",
      "rules":[{"id":921,"mux":-1,"ops":[{"type":"set_bit","bit":13,"val":1}],"send":true}]
    })json";

    DashPluginResult result = engine.installJson(json, false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL(1, engine.pluginCount());
    TEST_ASSERT_FALSE(engine.pluginAt(0).enabled);
    TEST_ASSERT_EQUAL_STRING("My Plugin", engine.pluginAt(0).name.c_str());
    TEST_ASSERT_EQUAL(1, engine.pluginAt(0).priority);
}

void test_invalid_json_is_rejected_without_installing()
{
    DashPluginEngine engine;
    DashPluginResult result = engine.installJson("{not-json", false);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(0, engine.pluginCount());
    TEST_ASSERT_TRUE(result.message.length() > 0);
}

void test_name_longer_than_31_chars_is_rejected()
{
    DashPluginEngine engine;
    const char *json = R"json({"name":"abcdefghijklmnopqrstuvwxyzabcdef","rules":[{"id":921,"ops":[{"type":"checksum"}]}]})json";

    DashPluginResult result = engine.installJson(json, false);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(0, engine.pluginCount());
}

void test_installing_same_name_replaces_existing_plugin_disabled()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"ReplaceMe","rules":[{"id":921,"ops":[{"type":"set_bit","bit":1}]}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("ReplaceMe", true).ok);

    DashPluginResult result = engine.installJson(R"json({"name":"ReplaceMe","version":"2.0","rules":[{"id":2047,"ops":[{"type":"checksum"}]}]})json", false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL(1, engine.pluginCount());
    TEST_ASSERT_EQUAL_STRING("2.0", engine.pluginAt(0).version.c_str());
    TEST_ASSERT_FALSE(engine.pluginAt(0).enabled);
}

void test_disabled_plugin_does_not_send()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Disabled","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_FALSE(applied.sent);
    TEST_ASSERT_EQUAL(0, driver.sent.size());
}

void test_enabled_plugin_applies_set_bit_and_sends_once()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Enabled","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Enabled", true).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
    TEST_ASSERT_TRUE((driver.sent[0].data[1] & 0x20) != 0);
}

void test_safety_context_blocks_plugin_execution()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Safe","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Safe", true).ok);

    DashPluginContext ctx = allowedContext();
    ctx.apGateAllowed = false;
    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, ctx, driver);

    TEST_ASSERT_FALSE(applied.sent);
    TEST_ASSERT_EQUAL(0, driver.sent.size());
    TEST_ASSERT_EQUAL_STRING("ap_gate", applied.blockedBy.c_str());
}

void test_priority_prevents_lower_plugin_from_overwriting_same_bit()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"High","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13,"val":1}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Low","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13,"val":0}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("High", true).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Low", true).ok);
    TEST_ASSERT_TRUE(engine.setPriority("High", 1).ok);
    TEST_ASSERT_TRUE(engine.setPriority("Low", 2).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
    TEST_ASSERT_TRUE((driver.sent[0].data[1] & 0x20) != 0);
    TEST_ASSERT_TRUE(engine.statusJson().indexOf("Priority overlap") >= 0);
}

void test_gtw2047_replay_repeats_only_modified_2047_frames()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"GTW","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"set_byte","byte":1,"val":170},{"type":"checksum"}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("GTW", true).ok);
    TEST_ASSERT_TRUE(engine.setReplayCount(3).ok);

    CanFrame f = frame2047(3);
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(3, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[0].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[1].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[2].data[1]);
}

void test_replay_count_does_not_repeat_non_2047_frames()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"NonGTW","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("NonGTW", true).ok);
    TEST_ASSERT_TRUE(engine.setReplayCount(5).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
}

void test_emit_periodic_requires_gtw2047_mux3()
{
    DashPluginEngine engine;
    DashPluginResult bad = engine.installJson(R"json({"name":"BadPeriodic","rules":[{"id":921,"mux":3,"ops":[{"type":"emit_periodic","interval":100}],"send":true}]})json", false);
    TEST_ASSERT_FALSE(bad.ok);

    DashPluginResult badMux = engine.installJson(R"json({"name":"BadPeriodicMux","rules":[{"id":2047,"mux":2,"mux_mask":255,"ops":[{"type":"emit_periodic","interval":100}],"send":true}]})json", false);
    TEST_ASSERT_FALSE(badMux.ok);

    DashPluginResult good = engine.installJson(R"json({"name":"GoodPeriodic","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"emit_periodic","interval":100}],"send":true}]})json", false);
    TEST_ASSERT_TRUE(good.ok);
}

void test_gtw_silent_unavailable_without_custom_key()
{
    DashPluginEngine engine;
    DashPluginResult result = engine.installJson(R"json({"name":"Silent","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}],"send":true}]})json", false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(engine.statusJson().indexOf("custom key build required") >= 0);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_official_plugin_installs_disabled_by_default);
    RUN_TEST(test_invalid_json_is_rejected_without_installing);
    RUN_TEST(test_name_longer_than_31_chars_is_rejected);
    RUN_TEST(test_installing_same_name_replaces_existing_plugin_disabled);
    RUN_TEST(test_disabled_plugin_does_not_send);
    RUN_TEST(test_enabled_plugin_applies_set_bit_and_sends_once);
    RUN_TEST(test_safety_context_blocks_plugin_execution);
    RUN_TEST(test_priority_prevents_lower_plugin_from_overwriting_same_bit);
    RUN_TEST(test_gtw2047_replay_repeats_only_modified_2047_frames);
    RUN_TEST(test_replay_count_does_not_repeat_non_2047_frames);
    RUN_TEST(test_emit_periodic_requires_gtw2047_mux3);
    RUN_TEST(test_gtw_silent_unavailable_without_custom_key);
    return UNITY_END();
}
