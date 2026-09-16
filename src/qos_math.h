#ifndef QOS_MATH_H
#define QOS_MATH_H

// qos_math.h
// Pure fair-share QoS decision math — no platform dependencies, no shaper
// state, no threads. Unit-testable in isolation. The controller in
// qos_fair.c is a thin, stateful wrapper around these functions.

#include <stdbool.h>

#define QOS_CAPACITY_DECAY    0.99     // per-tick decay of the capacity estimate
#define QOS_RELEASE_RATIO     0.50     // release when rate < share * this ratio
#define QOS_RELEASE_TICKS     3        // quiet ticks before releasing a cap
#define QOS_MIN_ACTIVE_SHARE  2        // a PID is "active" above this % of capacity
#define QOS_MIN_CAPACITY      16384.0  // bytes/sec; below this the link is "idle"

// Decaying high-water mark of observed total throughput.
static inline double qos_capacity_update(double capacity, double total_rate) {
    double decayed = capacity * QOS_CAPACITY_DECAY;
    return (decayed > total_rate) ? decayed : total_rate;
}

// Equal share of the detected capacity across N active flows:
//   1 flow  -> 100%
//   2 flows -> 50%
//   3 flows -> ~33%
//   4 flows -> 25%
// and so on. Every active PID is entitled to at most this much.
static inline double qos_fair_share(double capacity, int active_count) {
    if (active_count <= 0) return capacity;
    return capacity / (double)active_count;
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
// direction. Equal-share policy: any PID that exceeds its share of the
// detected capacity is capped at exactly that share.
//
//   capacity     : detected link capacity (bytes/sec)
//   rate         : this PID's measured throughput this tick
//   top_rate     : (unused, kept for signature compatibility)
//   is_top       : (unused, kept for signature compatibility)
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
    (void)top_rate;
    (void)is_top;

    double share = qos_fair_share(capacity, active_count);
    double desired = 0.0;

    // Keep or release an existing cap.
    if (applied > 0.0) {
        if (rate < share * QOS_RELEASE_RATIO) {
            (*release_ctr)++;
            desired = (*release_ctr >= QOS_RELEASE_TICKS) ? 0.0 : applied;
        } else {
            *release_ctr = 0;
            desired = share;
        }
    }

    // Cap any active PID that exceeds its equal share (not just the top hog).
    if (active_count >= 2 && rate > share) {
        desired = share;
        *release_ctr = 0;
    }

    return desired;
}

#endif // QOS_MATH_H
