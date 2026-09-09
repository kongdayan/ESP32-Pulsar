/*
 * usage_model 的纯逻辑 UT：JSON 解析、共享存储、倒计时/文案格式化、过期判断。
 * 全部可在主机上精确断言，不依赖 BLE / LVGL。
 */
#include "minitest.h"

#include <string.h>

#include "usage_model.h"

/* ── 默认值 / 存储 ────────────────────────────────────────────────────────── */

MT_TEST(test_usage_defaults)
{
    usage_data_t d;
    memset(&d, 0xAB, sizeof(d));
    usage_data_defaults(&d);

    CHECK_EQ(d.current_used_pct, 0);
    CHECK_EQ(d.weekly_used_pct, 0);
    CHECK_EQ(d.current_resets_in, 0);
    CHECK_EQ(d.weekly_resets_in, 0);
    CHECK_EQ(d.weekly_reset_label[0], '\0');
    CHECK_EQ((int)d.provider, (int)USAGE_PROVIDER_CODEX);
    CHECK_EQ((int)d.plan, (int)USAGE_PLAN_UNKNOWN);
    CHECK_FALSE(d.has_credits);
    CHECK_FALSE(d.unlimited);
    CHECK_FALSE(d.limit_reached);
    CHECK_FALSE(d.valid);
}

MT_TEST(test_usage_store_roundtrip)
{
    usage_store_reset();
    usage_data_t got;
    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CODEX, &got));
    CHECK_FALSE(got.valid);

    usage_data_t d;
    usage_data_defaults(&d);
    d.provider = USAGE_PROVIDER_CODEX;
    d.current_used_pct = 27;
    d.weekly_used_pct = 73;
    d.valid = true;
    usage_store_set(&d);

    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CODEX, &got));
    CHECK_EQ(got.current_used_pct, 27);
    CHECK_EQ(got.weekly_used_pct, 73);
    CHECK_TRUE(got.valid);

    usage_store_reset();
    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CODEX, &got));
    CHECK_FALSE(got.valid);
}

MT_TEST(test_usage_store_isolates_providers)
{
    usage_store_reset();

    usage_data_t codex;
    usage_data_defaults(&codex);
    codex.provider = USAGE_PROVIDER_CODEX;
    codex.current_used_pct = 11;
    codex.valid = true;
    usage_store_set(&codex);

    usage_data_t claude;
    usage_data_defaults(&claude);
    claude.provider = USAGE_PROVIDER_CLAUDE;
    claude.current_used_pct = 22;
    claude.valid = true;
    usage_store_set(&claude);

    usage_data_t got;
    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CODEX, &got));
    CHECK_EQ(got.current_used_pct, 11);
    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CLAUDE, &got));
    CHECK_EQ(got.current_used_pct, 22);

    /* 非法 provider 不崩、不命中 */
    CHECK_FALSE(usage_store_get(USAGE_PROVIDER_COUNT, &got));
    CHECK_FALSE(usage_store_get((usage_provider_t)-1, &got));

    usage_store_reset();
}

MT_TEST(test_usage_store_drops_invalid_provider)
{
    usage_store_reset();

    usage_data_t good;
    usage_data_defaults(&good);
    good.provider = USAGE_PROVIDER_CODEX;
    good.current_used_pct = 33;
    good.valid = true;
    usage_store_set(&good);

    usage_data_t bad;
    usage_data_defaults(&bad);
    bad.provider = (usage_provider_t)99;
    bad.current_used_pct = 77;
    bad.valid = true;
    usage_store_set(&bad);   /* 非法 provider 应被丢弃，不覆盖 Codex 槽 */

    usage_data_t got;
    CHECK_TRUE(usage_store_get(USAGE_PROVIDER_CODEX, &got));
    CHECK_EQ(got.current_used_pct, 33);

    usage_store_reset();
}

MT_TEST(test_usage_provider_table)
{
    CHECK(usage_provider_is_valid(USAGE_PROVIDER_CODEX));
    CHECK(usage_provider_is_valid(USAGE_PROVIDER_CLAUDE));
    CHECK(usage_provider_is_valid(USAGE_PROVIDER_NVIDIA));
    CHECK(usage_provider_is_valid(USAGE_PROVIDER_AMD));
    CHECK(usage_provider_is_valid(USAGE_PROVIDER_GLM));
    CHECK_FALSE(usage_provider_is_valid(USAGE_PROVIDER_COUNT));
    CHECK_FALSE(usage_provider_is_valid((usage_provider_t)-1));

    CHECK_STR_EQ(usage_provider_key(USAGE_PROVIDER_CODEX), "codex");
    CHECK_STR_EQ(usage_provider_title(USAGE_PROVIDER_CLAUDE), "CLAUDE");
    CHECK_STR_EQ(usage_provider_key(USAGE_PROVIDER_COUNT), "unknown");
    CHECK_STR_EQ(usage_provider_title((usage_provider_t)-5), "?");

    for (int p = 0; p < USAGE_PROVIDER_COUNT; p++) {
        CHECK(usage_providers[p].key != NULL);
        CHECK(usage_providers[p].title != NULL);
        CHECK(usage_providers[p].key[0] != '\0');
    }
}

MT_TEST(test_usage_parse_provider_field)
{
    usage_data_t d;
    CHECK_TRUE(usage_parse_json("{\"p\":1,\"cu\":10,\"wu\":20}", &d));
    CHECK_EQ((int)d.provider, (int)USAGE_PROVIDER_CLAUDE);

    /* 缺省 → Codex（向后兼容） */
    CHECK_TRUE(usage_parse_json("{\"cu\":10,\"wu\":20}", &d));
    CHECK_EQ((int)d.provider, (int)USAGE_PROVIDER_CODEX);

    /* 越界 provider 忽略，保持默认 */
    CHECK_TRUE(usage_parse_json("{\"p\":99,\"cu\":10,\"wu\":20}", &d));
    CHECK_EQ((int)d.provider, (int)USAGE_PROVIDER_CODEX);
}

/* ── JSON 解析 ───────────────────────────────────────────────────────────── */

MT_TEST(test_usage_parse_full_payload)
{
    const char *json =
        "{\"cu\":27,\"ci\":79140,\"wu\":73,\"wi\":604740,"
        "\"wl\":\"16:14 on 18 May\",\"pl\":4,\"cc\":1,\"un\":0,\"rl\":0}";
    usage_data_t d;
    CHECK_TRUE(usage_parse_json(json, &d));
    CHECK_TRUE(d.valid);
    CHECK_EQ(d.current_used_pct, 27);
    CHECK_EQ(d.weekly_used_pct, 73);
    CHECK_EQ(d.current_resets_in, 79140);
    CHECK_EQ(d.weekly_resets_in, 604740);
    CHECK_STR_EQ(d.weekly_reset_label, "16:14 on 18 May");
    CHECK_EQ((int)d.plan, (int)USAGE_PLAN_PRO);
    CHECK_TRUE(d.has_credits);
    CHECK_FALSE(d.unlimited);
    CHECK_FALSE(d.limit_reached);
}

MT_TEST(test_usage_parse_tolerates_whitespace_and_order)
{
    const char *json = " { \"wu\" : 50 , \"cu\" : 10 } ";
    usage_data_t d;
    CHECK_TRUE(usage_parse_json(json, &d));
    CHECK_EQ(d.current_used_pct, 10);
    CHECK_EQ(d.weekly_used_pct, 50);
    CHECK_EQ(d.weekly_reset_label[0], '\0');
}

MT_TEST(test_usage_parse_clamps_and_flags)
{
    const char *json = "{\"cu\":150,\"wu\":-20,\"ci\":-5,\"un\":1,\"rl\":1}";
    usage_data_t d;
    CHECK_TRUE(usage_parse_json(json, &d));
    CHECK_EQ(d.current_used_pct, USAGE_PCT_MAX);
    CHECK_EQ(d.weekly_used_pct, USAGE_PCT_MIN);
    CHECK_EQ(d.current_resets_in, 0);
    CHECK_TRUE(d.unlimited);
    CHECK_TRUE(d.limit_reached);
}

MT_TEST(test_usage_parse_rejects_bad_input)
{
    usage_data_t d;
    CHECK_FALSE(usage_parse_json(NULL, &d));
    CHECK_FALSE(usage_parse_json("", &d));
    CHECK_FALSE(usage_parse_json("not json", &d));
    CHECK_FALSE(usage_parse_json("[1,2,3]", &d));
    CHECK_FALSE(usage_parse_json("{\"cu\":\"x\"}", &d));      /* 类型不符 */
    /* 缺关键字段（只有 current）→ 不视为有效 */
    CHECK_FALSE(usage_parse_json("{\"cu\":10}", &d));
    CHECK_FALSE(usage_parse_json("{\"wu\":10}", &d));
}

MT_TEST(test_usage_parse_ignores_unknown_keys)
{
    const char *json = "{\"zz\":1,\"cu\":11,\"wu\":22,\"extra\":\"x\"}";
    usage_data_t d;
    CHECK_TRUE(usage_parse_json(json, &d));
    CHECK_EQ(d.current_used_pct, 11);
    CHECK_EQ(d.weekly_used_pct, 22);
}

/* ── 时间 / 过期 ─────────────────────────────────────────────────────────── */

MT_TEST(test_usage_elapsed_and_countdown)
{
    CHECK_EQ(usage_elapsed_s(1000u, 3000u), 2);
    CHECK_EQ(usage_elapsed_s(0u, 999u), 0);
    /* millis 回绕：从 0xFFFFFF00 到 0x00000100 只过了 512ms */
    CHECK_EQ(usage_elapsed_s(0xFFFFFF00u, 0x00000100u), 0);

    usage_data_t d;
    usage_data_defaults(&d);
    d.current_resets_in = 100;
    d.rx_ms = 1000u;
    CHECK_EQ(usage_countdown_s(&d, 1000u), 100);
    CHECK_EQ(usage_countdown_s(&d, 11000u), 90);
    CHECK_EQ(usage_countdown_s(&d, 999999u), 0);     /* 不会变负 */
    CHECK_EQ(usage_countdown_s(NULL, 0u), 0);
}

MT_TEST(test_usage_is_stale)
{
    usage_data_t d;
    usage_data_defaults(&d);
    CHECK_TRUE(usage_is_stale(&d, 0u, 1000u));       /* invalid 一律过期 */

    d.valid = true;
    d.rx_ms = 5000u;
    CHECK_FALSE(usage_is_stale(&d, 5500u, 1000u));
    CHECK_TRUE(usage_is_stale(&d, 6001u, 1000u));
    CHECK_TRUE(usage_is_stale(NULL, 0u, 1u));
}

/* ── 文案 ────────────────────────────────────────────────────────────────── */

MT_TEST(test_usage_format_countdown)
{
    char buf[USAGE_RESET_TEXT_MAX];
    usage_format_countdown(21 * USAGE_SEC_PER_HOUR + 59 * USAGE_SEC_PER_MINUTE,
                           buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resets in 21:59");

    usage_format_countdown(0, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resetting...");
    usage_format_countdown(-5, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resetting...");

    usage_format_countdown(USAGE_SEC_PER_DAY + 3 * USAGE_SEC_PER_HOUR, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resets in 1d 3h");

    CHECK_EQ(usage_format_countdown(60, NULL, 0), 0);
}

MT_TEST(test_usage_format_weekly)
{
    char buf[USAGE_RESET_TEXT_MAX];
    usage_format_weekly("16:14 on 18 May", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resets 16:14 on 18 May");

    usage_format_weekly("", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resets soon");
    usage_format_weekly(NULL, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Resets soon");
}

MT_TEST(test_usage_plan_name)
{
    CHECK_STR_EQ(usage_plan_name(USAGE_PLAN_FREE), "FREE");
    CHECK_STR_EQ(usage_plan_name(USAGE_PLAN_PRO), "PRO");
    CHECK_STR_EQ(usage_plan_name(USAGE_PLAN_ENTERPRISE), "ENT");
    CHECK_STR_EQ(usage_plan_name(USAGE_PLAN_UNKNOWN), "?");
    CHECK_STR_EQ(usage_plan_name((usage_plan_t)99), "?");
}

MT_TEST(test_usage_parse_label_is_bounded)
{
    const char *json =
        "{\"cu\":1,\"wu\":2,\"wl\":\"this label is definitely far too long to fit\"}";
    usage_data_t d;
    CHECK_TRUE(usage_parse_json(json, &d));
    CHECK(strlen(d.weekly_reset_label) < USAGE_LABEL_MAX);
    CHECK(strlen(d.weekly_reset_label) > 0u);
}
