// test_schedule.c
// Tests for schedule parsing and time-window matching (schedule.h).

#include "test.h"
#include "schedule.h"

void test_schedule(void) {
    Schedule s;

    // Empty string -> no constraint.
    CHECK(schedule_parse("", &s));
    CHECK(schedule_is_empty(&s));

    // Time + weekday range.
    CHECK(schedule_parse("0800-1600~1-5", &s));
    CHECK(s.has_time);
    CHECK_INT(s.start_min, 480);
    CHECK_INT(s.end_min, 960);
    CHECK(s.has_days);
    CHECK_INT(s.days_mask, 0x3E);   // bits 1..5 = Mon-Fri

    // Day list only.
    CHECK(schedule_parse("1,3,5", &s));
    CHECK(!s.has_time);
    CHECK(s.has_days);
    CHECK_INT(s.days_mask, 0x2A);   // bits 1,3,5 = Mon, Wed, Fri

    // Overnight (midnight-spanning) window.
    CHECK(schedule_parse("2200-0600", &s));
    CHECK(s.has_time);
    CHECK_INT(s.start_min, 1320);
    CHECK_INT(s.end_min, 360);
    CHECK(!s.has_days);

    // Explicit-time matching (no clock dependency).
    CHECK(schedule_parse("0800-1600~1-5", &s));
    CHECK(schedule_is_active(&s, 9, 0, 1));    // Mon 09:00
    CHECK(schedule_is_active(&s, 12, 30, 3));  // Wed 12:30
    CHECK(!schedule_is_active(&s, 20, 0, 1));  // Mon 20:00 (after window)
    CHECK(!schedule_is_active(&s, 7, 59, 1));  // Mon 07:59 (before window)
    CHECK(!schedule_is_active(&s, 9, 0, 6));   // Sat (outside Mon-Fri)
    CHECK(!schedule_is_active(&s, 9, 0, 7));   // Sun (outside Mon-Fri)

    // Overnight matching.
    CHECK(schedule_parse("2200-0600", &s));
    CHECK(schedule_is_active(&s, 23, 0, 1));   // Mon 23:00
    CHECK(schedule_is_active(&s, 2, 0, 2));    // Tue 02:00
    CHECK(!schedule_is_active(&s, 12, 0, 1));  // Mon 12:00 (midday gap)

    // Invalid strings are rejected.
    CHECK(!schedule_parse("2500-1600", &s));        // hour out of range
    CHECK(!schedule_parse("0800-1600~1-8", &s));    // day out of range
    CHECK(!schedule_parse("0800-1600x", &s));       // trailing garbage
}
