// test_qos_math.c
// Tests for the pure fair-share QoS decision math (qos_math.h).

#include "test.h"
#include "qos_math.h"

void test_qos_math(void) {
    // ---- Capacity update: decaying high-water mark ---------------------
    CHECK_NEAR(qos_capacity_update(0.0, 100000.0), 100000.0, 0.001);   // first observation
    CHECK_NEAR(qos_capacity_update(100000.0, 50000.0), 99000.0, 0.001); // decay wins
    CHECK_NEAR(qos_capacity_update(99000.0, 100000.0), 100000.0, 0.001); // new peak
    CHECK_NEAR(qos_capacity_update(100000.0, 0.0), 99000.0, 0.001);     // idle decay

    // ---- Equal share / floor / enforcement ------------------------------
    CHECK_NEAR(qos_fair_share(100000.0, 1), 100000.0, 0.001);   // 1 flow -> 100%
    CHECK_NEAR(qos_fair_share(100000.0, 2), 50000.0, 0.001);    // 2 -> 50%
    CHECK_NEAR(qos_fair_share(100000.0, 3), 33333.333, 0.001);  // 3 -> 33%
    CHECK_NEAR(qos_fair_share(100000.0, 4), 25000.0, 0.001);    // 4 -> 25%
    CHECK_NEAR(qos_fair_share(100000.0, 0), 100000.0, 0.001);   // 0 -> fallback
    CHECK_NEAR(qos_active_floor(100000.0), 2000.0, 0.001);
    CHECK(qos_enforced(QOS_MIN_CAPACITY));
    CHECK(!qos_enforced(QOS_MIN_CAPACITY - 1.0));
    CHECK(!qos_enforced(0.0));

    double cap = 100000.0;                 // detected capacity
    int ctr = 0;

    // Single active PID (no competition): never capped, even near the link.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 90000.0, 90000.0, true, 1, 0.0, &ctr), 0.0, 0.001);

    // Two active PIDs, this one exceeds its 50% share: capped at 50%.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 90000.0, 90000.0, true, 2, 0.0, &ctr), 50000.0, 0.001);
    CHECK_INT(ctr, 0);

    // Two active, below the 50% share: not capped (doesn't exceed its share).
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 40000.0, 90000.0, false, 2, 0.0, &ctr), 0.0, 0.001);

    // Three active PIDs, this one exceeds its 33% share: capped at 33%.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 50000.0, 50000.0, true, 3, 0.0, &ctr), 33333.333, 0.001);

    // Four active PIDs, this one exceeds its 25% share: capped at 25%.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 40000.0, 40000.0, true, 4, 0.0, &ctr), 25000.0, 0.001);

    // Existing cap, still saturating: kept at share, release counter reset.
    ctr = 2;
    CHECK_NEAR(qos_decide_cap(cap, 60000.0, 60000.0, true, 2, 50000.0, &ctr), 50000.0, 0.001);
    CHECK_INT(ctr, 0);

    // Existing cap, demand dropped below half the share: held for
    // QOS_RELEASE_TICKS, then released.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 20000.0, 20000.0, false, 2, 50000.0, &ctr), 50000.0, 0.001);
    CHECK_INT(ctr, 1);
    CHECK_NEAR(qos_decide_cap(cap, 20000.0, 20000.0, false, 2, 50000.0, &ctr), 50000.0, 0.001);
    CHECK_INT(ctr, 2);
    CHECK_NEAR(qos_decide_cap(cap, 20000.0, 20000.0, false, 2, 50000.0, &ctr), 0.0, 0.001);
    CHECK_INT(ctr, 3);

    // Release counter resets when demand recovers before the threshold.
    ctr = 2;
    CHECK_NEAR(qos_decide_cap(cap, 60000.0, 60000.0, true, 2, 50000.0, &ctr), 50000.0, 0.001);
    CHECK_INT(ctr, 0);
}
