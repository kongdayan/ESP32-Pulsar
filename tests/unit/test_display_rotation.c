#include "minitest.h"

#include <stddef.h>

#include "display_rotation.h"

static void expect_rotation(uint32_t rot, bool swap, bool mx, bool my)
{
    disp_rotation_t out;
    CHECK(disp_rotation_resolve(rot, &out));
    CHECK_EQ(out.swap_xy, swap);
    CHECK_EQ(out.mirror_x, mx);
    CHECK_EQ(out.mirror_y, my);
}

MT_TEST(test_rotation_all_quadrants)
{
    expect_rotation(0u, false, false, false);
    expect_rotation(1u, true, true, false);
    expect_rotation(2u, false, true, true);
    expect_rotation(3u, true, false, true);
}

MT_TEST(test_rotation_rejects_invalid)
{
    disp_rotation_t out;

    out.swap_xy = true;
    out.mirror_x = true;
    out.mirror_y = true;

    CHECK_FALSE(disp_rotation_resolve(DISP_ROTATION_MAX + 1u, &out));
    /* 失败时不得写出参 */
    CHECK_EQ(out.swap_xy, true);
    CHECK_EQ(out.mirror_x, true);
    CHECK_EQ(out.mirror_y, true);

    CHECK_FALSE(disp_rotation_resolve(255u, &out));
    CHECK_FALSE(disp_rotation_resolve(0u, NULL));
}

MT_TEST(test_rotation_max_constant)
{
    CHECK_EQ(DISP_ROTATION_MAX, 3u);
}
