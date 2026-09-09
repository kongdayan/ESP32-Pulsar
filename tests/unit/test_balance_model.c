/* balance_model 的纯逻辑 UT：解析、共享存储、金额格式化、过期判断。 */
#include "minitest.h"

#include <string.h>

#include "balance_model.h"

MT_TEST(test_balance_defaults_and_store)
{
    balance_data_t d;
    memset(&d, 0xAB, sizeof(d));
    balance_data_defaults(&d);
    CHECK_EQ(d.currency[0], '\0');
    CHECK_EQ(d.total_cents, 0);
    CHECK_FALSE(d.valid);
    CHECK_FALSE(d.is_available);

    balance_store_reset();
    balance_data_t got;
    CHECK_TRUE(balance_store_get(&got));
    CHECK_FALSE(got.valid);

    d.currency[0] = 'C'; d.currency[1] = 'N'; d.currency[2] = 'Y'; d.currency[3] = '\0';
    d.total_cents = 2855;
    d.granted_cents = 0;
    d.topped_up_cents = 2855;
    d.is_available = true;
    d.valid = true;
    d.rx_ms = 1234u;
    balance_store_set(&d);

    CHECK_TRUE(balance_store_get(&got));
    CHECK_STR_EQ(got.currency, "CNY");
    CHECK_EQ(got.total_cents, 2855);
    CHECK_EQ(got.topped_up_cents, 2855);
    CHECK_TRUE(got.is_available);
    CHECK_TRUE(got.valid);
    CHECK_EQ(got.rx_ms, 1234u);

    balance_store_reset();
    CHECK_TRUE(balance_store_get(&got));
    CHECK_FALSE(got.valid);
}

MT_TEST(test_balance_parse_real_payload)
{
    /* 与 DeepSeek /user/balance 实测响应同构（金额已由客户端换算成分） */
    const char *json = "{\"cur\":\"CNY\",\"tot\":2855,\"gr\":0,\"top\":2855,\"av\":1}";
    balance_data_t d;
    CHECK_TRUE(balance_parse_json(json, &d));
    CHECK_TRUE(d.valid);
    CHECK_STR_EQ(d.currency, "CNY");
    CHECK_EQ(d.total_cents, 2855);
    CHECK_EQ(d.granted_cents, 0);
    CHECK_EQ(d.topped_up_cents, 2855);
    CHECK_TRUE(d.is_available);
}

MT_TEST(test_balance_parse_tolerates_whitespace_and_order)
{
    const char *json = " { \"av\" : 0 , \"tot\" : 100 , \"cur\" : \"USD\" } ";
    balance_data_t d;
    CHECK_TRUE(balance_parse_json(json, &d));
    CHECK_STR_EQ(d.currency, "USD");
    CHECK_EQ(d.total_cents, 100);
    CHECK_FALSE(d.is_available);
}

MT_TEST(test_balance_parse_clamps_negative)
{
    const char *json = "{\"cur\":\"CNY\",\"tot\":-5,\"gr\":-1,\"top\":-2}";
    balance_data_t d;
    CHECK_TRUE(balance_parse_json(json, &d));
    CHECK_EQ(d.total_cents, 0);
    CHECK_EQ(d.granted_cents, 0);
    CHECK_EQ(d.topped_up_cents, 0);
}

MT_TEST(test_balance_parse_rejects_bad_input)
{
    balance_data_t d;
    CHECK_FALSE(balance_parse_json(NULL, &d));
    CHECK_FALSE(balance_parse_json("", &d));
    CHECK_FALSE(balance_parse_json("[]", &d));
    CHECK_FALSE(balance_parse_json("{\"cur\":123}", &d));   /* 类型不符 */
    CHECK_FALSE(balance_parse_json("{\"cur\":\"CNY\"}", &d)); /* 缺 tot */
}

MT_TEST(test_balance_parse_ignores_unknown_keys)
{
    const char *json = "{\"zz\":9,\"tot\":42,\"extra\":\"x\"}";
    balance_data_t d;
    CHECK_TRUE(balance_parse_json(json, &d));
    CHECK_EQ(d.total_cents, 42);
}

MT_TEST(test_balance_format_amount)
{
    char buf[BALANCE_AMOUNT_TEXT_MAX];
    balance_format_amount(2855, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "28.55");
    balance_format_amount(0, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "0.00");
    balance_format_amount(5, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "0.05");
    balance_format_amount(100, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "1.00");
    balance_format_amount(123456, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "1234.56");
    balance_format_amount(-123, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "-1.23");

    CHECK_EQ(balance_format_amount(1, NULL, 0), 0);
}

MT_TEST(test_balance_is_stale)
{
    balance_data_t d;
    balance_data_defaults(&d);
    CHECK_TRUE(balance_is_stale(&d, 0u, 1000u));   /* invalid 一律过期 */

    d.valid = true;
    d.rx_ms = 5000u;
    CHECK_FALSE(balance_is_stale(&d, 5500u, 1000u));
    CHECK_TRUE(balance_is_stale(&d, 6001u, 1000u));
    CHECK_TRUE(balance_is_stale(NULL, 0u, 1u));
}
