// test_token_bucket.c
// Tests for the token-bucket rate limiter (token_bucket.c). Timing-based
// checks use generous margins to stay robust on CI runners.

#include "test.h"
#include "token_bucket.h"
#include "localization_api.h"
#include <windows.h>

void test_token_bucket(void) {
    // C() resolves against g_cli_strings; initialise the language table so
    // the error path in token_bucket_consume is safe even if exercised.
    loc_set_language(LOC_LANG_EN);

    TokenBucket b;

    // Init: 1000 B/s, burst of 1000 bytes.
    CHECK(token_bucket_init(&b, 1000.0, 1000));

    // Full burst available immediately.
    CHECK(token_bucket_consume(&b, 1000));
    CHECK(!token_bucket_consume(&b, 1));   // bucket now empty

    // ~200 ms of refill at 1000 B/s -> ~200 tokens.
    Sleep(200);
    CHECK(token_bucket_consume(&b, 100));    // comfortably available
    CHECK(!token_bucket_consume(&b, 1000));  // far more than refilled

    // ~1.1 s refills to the burst cap (1000), no more.
    Sleep(1100);
    CHECK(token_bucket_consume(&b, 1000));
    CHECK(!token_bucket_consume(&b, 1));

    // update_rate refills to the new burst immediately.
    token_bucket_update_rate(&b, 10000.0, 1000);
    CHECK(token_bucket_consume(&b, 500));
    CHECK(!token_bucket_consume(&b, 600));   // only ~500 left

    token_bucket_destroy(&b);

    // Invalid init parameters are rejected.
    TokenBucket bad;
    CHECK(!token_bucket_init(&bad, 0.0, 1000));   // zero rate
    CHECK(!token_bucket_init(&bad, -1.0, 1000));  // negative rate
    CHECK(!token_bucket_init(&bad, 1000.0, 0));   // zero burst
}
