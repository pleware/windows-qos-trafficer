#ifndef QOS_FAIR_H
#define QOS_FAIR_H

// qos_fair.h
// Fair-share QoS ("anti-hog") controller.
//
// Goal: prevent a single process from monopolising upload or download
// bandwidth. Link capacity is auto-detected as the decaying high-water mark
// of observed total throughput, so no line rate needs to be configured.
//
// Each tick the controller snapshots per-PID traffic (via the shaper core),
// computes per-PID throughput, and caps any single PID that consumes more
// than QOS_FAIR_SHARE_PERCENT of the detected capacity while at least one
// other PID is also active. A cap is released once the PID's demand stays
// below half the fair share for QOS_RELEASE_TICKS consecutive ticks.

#include "shaper_core.h"
#include "qos_math.h"

#define QOS_TICK_INTERVAL_MS   1000  // measurement period

typedef struct QosFairController QosFairController;

// Allocate a controller. Returns NULL on OOM.
QosFairController *qos_fair_create(void);

// Free the controller and all state it holds.
void qos_fair_destroy(QosFairController *qos);

// Run one control iteration. Call periodically from the CLI main loop; the
// controller self-throttles to QOS_TICK_INTERVAL_MS. `quiet` suppresses the
// cap/release console messages.
void qos_fair_tick(QosFairController *qos, ShaperInstance *shaper, bool quiet);

#endif // QOS_FAIR_H
