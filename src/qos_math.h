#ifndef QOS_MATH_H
#define QOS_MATH_H

// qos_math.h
// Pure fair-share QoS decision math — no platform dependencies, no shaper
// state, no threads. Unit-testable in isolation. The controller in
// qos_fair.c is a thin, stateful wrapper around these functions.

#include <stdbool.h>

#define QOS_CAPACITY_DECAY    0.99     // per-tick decay of the capacity estimate
#define QOS_FAIR_SHARE_PERCENT 70      // a hog is capped at this % of capacity
#define QOS_RELEASE_RATIO     0.50     // release when rate < cap * this ratio
#define QOS_RELEASE_TICKS     3        // quiet ticks before releasing a cap
#define QOS_MIN_ACTIVE_SHARE  2        // a PID is "active" above this % of capacity
#define QOS_MIN_CAPACITY      16384.0  // bytes/sec; below this the link is "idle"

// Decaying high-water mark of observed total throughput.
static inline double qos_capacity_update(double capacity, double total_rate) {
    double decayed = capacity * QOS_CAPACITY_DECAY;
    return (decayed > total_rate) ? decayed : total_rate;
}

// The share a hog is capped at for a given detected capacity (bytes/sec).
static inline double qos_fair_share(double capacity) {
    return capacity * (QOS_FAIR_SHARE_PERCENT / 100.0);
}

// Throughput (bytes/sec) below which a PID is not counted as "active".
static inline double qos_active_floor(double capacity) {
    return capacity * (QOS_MIN_ACTIVE_SHARE / 100.0);
}

// True when the direction should be enforced at all (link is not idle).
static inline bool qos_enforced(double capacity) {
    return capacity >= QOS_MIN_CAPACITY;
}

// Decide the desired cap (bytes/sec; 0 = uncapped) for one PID in one
// direction. Pure and deterministic for a given set of inputs.
//
//   capacity     : detected link capacity (bytes/sec)
//   rate         : this PID's measured throughput this tick
//   top_rate     : highest active PID's throughput this tick
//   is_top       : true if this PID is the top hog
//   active_count : number of PIDs above the activity floor
//   applied      : currently-enforced cap for this PID (0 = none), read-only
//   release_ctr  : in/out, consecutive ticks below the release threshold
static inline double qos_decide_cap(double capacity,
                                    double rate,
                                    double top_rate,
                                    bool is_top,
                                    int active_count,
                                    double applied,
                                    int *release_ctr) {
    double fair = qos_fair_share(capacity);
    double desired = 0.0;

    // Keep or release an existing cap.
    if (applied > 0.0) {
        if (rate < fair * QOS_RELEASE_RATIO) {
            (*release_ctr)++;
            desired = (*release_ctr >= QOS_RELEASE_TICKS) ? 0.0 : applied;
        } else {
            *release_ctr = 0;
            desired = fair;
        }
    }

    // Apply a fresh cap when a single process dominates while others compete.
    if (active_count >= 2 && is_top && top_rate > fair) {
        desired = fair;
        *release_ctr = 0;
    }

    return desired;
}

#endif // QOS_MATH_H
