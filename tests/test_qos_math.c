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

    // ---- Fair share / floor / enforcement ------------------------------
    CHECK_NEAR(qos_fair_share(100000.0), 70000.0, 0.001);
    CHECK_NEAR(qos_active_floor(100000.0), 2000.0, 0.001);
    CHECK(qos_enforced(QOS_MIN_CAPACITY));
    CHECK(!qos_enforced(QOS_MIN_CAPACITY - 1.0));
    CHECK(!qos_enforced(0.0));

    double cap = 100000.0;                 // detected capacity
    double fair = qos_fair_share(cap);     // 70000
    int ctr = 0;

    // Single active PID (no competition): never capped, even at 90% of link.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 90000.0, 90000.0, true, 1, 0.0, &ctr), 0.0, 0.001);

    // Two active PIDs, this one dominates: capped at fair share.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 90000.0, 90000.0, true, 2, 0.0, &ctr), fair, 0.001);
    CHECK_INT(ctr, 0);

    // Two active, top rate below fair share: not a hog.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 50000.0, 50000.0, true, 2, 0.0, &ctr), 0.0, 0.001);

    // Two active, but this PID is not the top one: not capped.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 10000.0, 90000.0, false, 2, 0.0, &ctr), 0.0, 0.001);

    // Existing cap, still saturating: kept at fair, release counter reset.
    ctr = 2;
    CHECK_NEAR(qos_decide_cap(cap, 70000.0, 70000.0, true, 2, fair, &ctr), fair, 0.001);
    CHECK_INT(ctr, 0);

    // Existing cap, demand dropped: held for QOS_RELEASE_TICKS, then released.
    ctr = 0;
    CHECK_NEAR(qos_decide_cap(cap, 10000.0, 10000.0, false, 1, fair, &ctr), fair, 0.001);
    CHECK_INT(ctr, 1);
    CHECK_NEAR(qos_decide_cap(cap, 10000.0, 10000.0, false, 1, fair, &ctr), fair, 0.001);
    CHECK_INT(ctr, 2);
    CHECK_NEAR(qos_decide_cap(cap, 10000.0, 10000.0, false, 1, fair, &ctr), 0.0, 0.001);
    CHECK_INT(ctr, 3);

    // Release counter resets when demand recovers before the threshold.
    ctr = 2;
    CHECK_NEAR(qos_decide_cap(cap, 70000.0, 70000.0, true, 2, fair, &ctr), fair, 0.001);
    CHECK_INT(ctr, 0);
}
