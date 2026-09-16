// qos_fair.c
// Fair-share QoS ("anti-hog") controller implementation.
//
// The controller runs on the CLI main thread and only talks to the shaper
// core through its public API (shaper_snapshot_traffic, add/remove rule,
// shaper_reload_rules) — the same pattern the schedule/quota sync already
// uses. Caps are enforced by adding a per-PID rule (see shaper_add_process_rule
// with a numeric identifier), which the packet loop applies via its
// reverse-index PID lookup.

#include "qos_fair.h"
#include "localization_api.h"

#define QOS_CAPACITY_DECAY 0.99    // per-tick decay of the capacity estimate
#define QOS_RELEASE_RATIO  0.50    // release when rate < cap * this ratio
#define QOS_RERATE_RATIO   0.10    // re-apply cap when fair share drifts >10%
#define QOS_MIN_CAPACITY   16384.0 // bytes/sec; below this the link is "idle"
#define QOS_PRUNE_TICKS    10      // drop state for PIDs unseen this many ticks

typedef struct QosPidState {
    DWORD pid;
    uint64_t prev_dl;      // cumulative bytes at the previous tick
    uint64_t prev_ul;
    double cur_dl;         // throughput this tick (bytes/sec)
    double cur_ul;
    double desired_dl;     // 0 = uncapped, else bytes/sec
    double desired_ul;
    double applied_dl;     // what the shaper currently enforces
    double applied_ul;
    int dl_release;        // consecutive ticks below the release threshold
    int ul_release;
    uint64_t last_seen_tick;
    UT_hash_handle hh;
} QosPidState;

struct QosFairController {
    double dl_capacity;    // auto-detected download capacity (bytes/sec)
    double ul_capacity;    // auto-detected upload capacity (bytes/sec)
    ULONGLONG last_tick_ms;
    uint64_t tick_count;
    QosPidState *pids;     // uthash keyed by pid
};

QosFairController *qos_fair_create(void) {
    return (QosFairController *)calloc(1, sizeof(QosFairController));
}

void qos_fair_destroy(QosFairController *qos) {
    if (!qos) return;
    QosPidState *st, *tmp;
    HASH_ITER(hh, qos->pids, st, tmp) {
        HASH_DEL(qos->pids, st);
        free(st);
    }
    free(qos);
}

static QosPidState *find_pid(QosFairController *qos, DWORD pid) {
    QosPidState *st = NULL;
    HASH_FIND(hh, qos->pids, &pid, sizeof(DWORD), st);
    return st;
}

// Release a PID's dynamic rule in the shaper (if it has one) and clear the
// applied state. Logs once when the cap is actually removed.
static void release_pid(ShaperInstance *shaper, QosPidState *st, bool quiet) {
    if (st->applied_dl <= 0.0 && st->applied_ul <= 0.0) return;

    char pidstr[32];
    snprintf(pidstr, sizeof(pidstr), "%u", (unsigned)st->pid);
    shaper_remove_process_rule(shaper, pidstr);
    if (!quiet) {
        printf(C(CLI_QOS_RELEASED), (unsigned)st->pid);
    }
    st->applied_dl = 0.0;
    st->applied_ul = 0.0;
    st->dl_release = 0;
    st->ul_release = 0;
}

void qos_fair_tick(QosFairController *qos, ShaperInstance *shaper, bool quiet) {
    if (!qos || !shaper) return;

    ULONGLONG now = GetTickCount64();
    if (qos->last_tick_ms != 0 &&
        (now - qos->last_tick_ms) < QOS_TICK_INTERVAL_MS) {
        return;
    }
    double dt = (qos->last_tick_ms == 0)
                    ? 1.0
                    : (double)(now - qos->last_tick_ms) / 1000.0;
    bool first = (qos->last_tick_ms == 0);
    qos->last_tick_ms = now;
    qos->tick_count++;

    TrafficSnapshot snap;
    if (!shaper_snapshot_traffic(shaper, &snap)) return;

    // Reset per-PID current-tick rates; PIDs absent from this snapshot stay
    // at zero and are treated as inactive (handles processes that exited).
    QosPidState *st0, *tmp0;
    HASH_ITER(hh, qos->pids, st0, tmp0) {
        st0->cur_dl = 0.0;
        st0->cur_ul = 0.0;
    }

    // ---- Pass 1: per-PID throughput + totals ---------------------------
    double total_dl = 0.0, total_ul = 0.0;
    for (int i = 0; i < snap.count; i++) {
        DWORD pid = snap.entries[i].pid;
        uint64_t dl = snap.entries[i].dl_bytes;
        uint64_t ul = snap.entries[i].ul_bytes;

        QosPidState *st = find_pid(qos, pid);
        if (!st) {
            st = (QosPidState *)calloc(1, sizeof(*st));
            if (!st) continue;
            st->pid = pid;
            st->prev_dl = dl;
            st->prev_ul = ul;
            st->last_seen_tick = qos->tick_count;
            HASH_ADD(hh, qos->pids, pid, sizeof(DWORD), st);
            continue;
        }

        st->cur_dl = (dl >= st->prev_dl) ? (double)(dl - st->prev_dl) / dt : 0.0;
        st->cur_ul = (ul >= st->prev_ul) ? (double)(ul - st->prev_ul) / dt : 0.0;
        st->prev_dl = dl;
        st->prev_ul = ul;
        st->last_seen_tick = qos->tick_count;
        total_dl += st->cur_dl;
        total_ul += st->cur_ul;
    }

    if (first) {
        // Baseline only: no decisions on the first observation.
        shaper_free_traffic_snapshot(&snap);
        return;
    }

    // ---- Capacity: decaying high-water mark ----------------------------
    double dl_cap = qos->dl_capacity * QOS_CAPACITY_DECAY;
    qos->dl_capacity = (dl_cap > total_dl) ? dl_cap : total_dl;
    double ul_cap = qos->ul_capacity * QOS_CAPACITY_DECAY;
    qos->ul_capacity = (ul_cap > total_ul) ? ul_cap : total_ul;

    double fair_dl = qos->dl_capacity * (QOS_FAIR_SHARE_PERCENT / 100.0);
    double fair_ul = qos->ul_capacity * (QOS_FAIR_SHARE_PERCENT / 100.0);
    double floor_dl = qos->dl_capacity * (QOS_MIN_ACTIVE_SHARE / 100.0);
    double floor_ul = qos->ul_capacity * (QOS_MIN_ACTIVE_SHARE / 100.0);

    // ---- Find the top hog per direction and count active PIDs ----------
    DWORD top_dl_pid = 0, top_ul_pid = 0;
    double top_dl_rate = 0.0, top_ul_rate = 0.0;
    int active_dl = 0, active_ul = 0;

    QosPidState *st, *tmp;
    HASH_ITER(hh, qos->pids, st, tmp) {
        if (st->cur_dl > floor_dl) {
            active_dl++;
            if (st->cur_dl > top_dl_rate) { top_dl_rate = st->cur_dl; top_dl_pid = st->pid; }
        }
        if (st->cur_ul > floor_ul) {
            active_ul++;
            if (st->cur_ul > top_ul_rate) { top_ul_rate = st->cur_ul; top_ul_pid = st->pid; }
        }
    }

    bool dl_enforced = qos->dl_capacity >= QOS_MIN_CAPACITY;
    bool ul_enforced = qos->ul_capacity >= QOS_MIN_CAPACITY;

    // ---- Compute desired cap state per PID -----------------------------
    HASH_ITER(hh, qos->pids, st, tmp) {
        st->desired_dl = 0.0;
        st->desired_ul = 0.0;

        if (dl_enforced) {
            if (st->applied_dl > 0.0) {
                if (st->cur_dl < fair_dl * QOS_RELEASE_RATIO) {
                    st->dl_release++;
                    st->desired_dl = (st->dl_release >= QOS_RELEASE_TICKS)
                                         ? 0.0 : st->applied_dl;
                } else {
                    st->dl_release = 0;
                    st->desired_dl = fair_dl;
                }
            }
            if (active_dl >= 2 && st->pid == top_dl_pid && top_dl_rate > fair_dl) {
                st->desired_dl = fair_dl;
                st->dl_release = 0;
            }
        }

        if (ul_enforced) {
            if (st->applied_ul > 0.0) {
                if (st->cur_ul < fair_ul * QOS_RELEASE_RATIO) {
                    st->ul_release++;
                    st->desired_ul = (st->ul_release >= QOS_RELEASE_TICKS)
                                         ? 0.0 : st->applied_ul;
                } else {
                    st->ul_release = 0;
                    st->desired_ul = fair_ul;
                }
            }
            if (active_ul >= 2 && st->pid == top_ul_pid && top_ul_rate > fair_ul) {
                st->desired_ul = fair_ul;
                st->ul_release = 0;
            }
        }
    }

    // ---- Apply transitions (add / remove / re-rate) --------------------
    bool changed = false;
    HASH_ITER(hh, qos->pids, st, tmp) {
        bool want_dl = st->desired_dl > 0.0;
        bool want_ul = st->desired_ul > 0.0;
        bool have_dl = st->applied_dl > 0.0;
        bool have_ul = st->applied_ul > 0.0;

        if (!want_dl && !want_ul && !have_dl && !have_ul) continue;

        bool dl_changed = (want_dl != have_dl);
        if (want_dl && have_dl) {
            dl_changed = fabs(st->desired_dl - st->applied_dl) >
                         st->applied_dl * QOS_RERATE_RATIO;
        }
        bool ul_changed = (want_ul != have_ul);
        if (want_ul && have_ul) {
            ul_changed = fabs(st->desired_ul - st->applied_ul) >
                         st->applied_ul * QOS_RERATE_RATIO;
        }

        if (!dl_changed && !ul_changed) continue;

        char pidstr[32];
        snprintf(pidstr, sizeof(pidstr), "%u", (unsigned)st->pid);

        bool was_dl = have_dl;
        bool was_ul = have_ul;

        if (have_dl || have_ul) {
            shaper_remove_process_rule(shaper, pidstr);
        }

        if (want_dl || want_ul) {
            if (shaper_add_process_rule(shaper, pidstr, st->desired_dl, st->desired_ul,
                                        false, false, 0, 0, NULL)) {
                if (!quiet) {
                    if (want_dl && !was_dl)
                        printf(C(CLI_QOS_CAP_DL), (unsigned)st->pid, st->desired_dl / 1024.0);
                    if (want_ul && !was_ul)
                        printf(C(CLI_QOS_CAP_UL), (unsigned)st->pid, st->desired_ul / 1024.0);
                    if (!want_dl && !want_ul)
                        printf(C(CLI_QOS_RELEASED), (unsigned)st->pid);
                }
                st->applied_dl = st->desired_dl;
                st->applied_ul = st->desired_ul;
            } else {
                st->applied_dl = 0.0;
                st->applied_ul = 0.0;
            }
        } else {
            if (!quiet) {
                printf(C(CLI_QOS_RELEASED), (unsigned)st->pid);
            }
            st->applied_dl = 0.0;
            st->applied_ul = 0.0;
            st->dl_release = 0;
            st->ul_release = 0;
        }
        changed = true;
    }

    if (changed) {
        shaper_reload_rules(shaper);
    }

    // ---- Prune state for PIDs that have disappeared --------------------
    HASH_ITER(hh, qos->pids, st, tmp) {
        if (qos->tick_count - st->last_seen_tick > QOS_PRUNE_TICKS) {
            release_pid(shaper, st, quiet);
            HASH_DEL(qos->pids, st);
            free(st);
        }
    }

    shaper_free_traffic_snapshot(&snap);
}
