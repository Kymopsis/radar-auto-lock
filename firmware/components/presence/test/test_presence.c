#include "presence.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            fprintf(stderr, "check failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            exit(1);                                                                  \
        }                                                                             \
    } while (0)

#define TICK 50

/* Runs the machine from t0 to t1 with a fixed input; returns the last event seen. */
static presence_event_t run(presence_t *p, const presence_cfg_t *c, int64_t *t, int64_t until,
                            bool link, bool seen, int *locks, int *wakes)
{
    presence_event_t last = PRESENCE_EVENT_NONE;
    for (; *t < until; *t += TICK) {
        presence_event_t e = presence_update(p, c, *t, link, seen);
        if (e == PRESENCE_EVENT_LOCK) {
            ++*locks;
        }
        if (e == PRESENCE_EVENT_WAKE) {
            ++*wakes;
        }
        if (e != PRESENCE_EVENT_NONE) {
            last = e;
        }
    }
    return last;
}

static void test_zone(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    CHECK(presence_point_in_zone(&c, 0.0f, 0.6f, -0.2f));
    CHECK(!presence_point_in_zone(&c, 0.0f, 1.5f, 0.0f));   /* too far */
    CHECK(!presence_point_in_zone(&c, 0.9f, 0.3f, 0.0f));   /* 71 deg off axis */
    CHECK(!presence_point_in_zone(&c, 0.0f, -0.5f, 0.0f));  /* behind */
}

static void test_leave_and_return(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    presence_t p;
    int64_t t = 0;
    int locks = 0, wakes = 0;
    presence_init(&p, t);

    run(&p, &c, &t, 10000, true, true, &locks, &wakes);          /* sitting */
    CHECK(locks == 0 && wakes == 0 && p.state == PRESENCE_STATE_PRESENT);
    run(&p, &c, &t, 10000 + 29000, true, false, &locks, &wakes);  /* left 29 s ago */
    CHECK(locks == 0);
    run(&p, &c, &t, 10000 + 31000, true, false, &locks, &wakes);  /* 31 s */
    CHECK(locks == 1 && p.state == PRESENCE_STATE_AWAY);
    run(&p, &c, &t, 120000, true, false, &locks, &wakes);         /* still away */
    CHECK(locks == 1);
    run(&p, &c, &t, 120500, true, true, &locks, &wakes);          /* 0.5 s back */
    CHECK(wakes == 0);
    run(&p, &c, &t, 121200, true, true, &locks, &wakes);          /* 1.2 s back */
    CHECK(wakes == 1 && p.state == PRESENCE_STATE_PRESENT);
}

static void test_short_absence_and_flicker(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    presence_t p;
    int64_t t = 0;
    int locks = 0, wakes = 0;
    presence_init(&p, t);
    run(&p, &c, &t, 5000, true, true, &locks, &wakes);
    /* Seen once every 20 s (someone sitting still, occasional detection). */
    for (int i = 0; i < 10; ++i) {
        run(&p, &c, &t, t + 20000, true, false, &locks, &wakes);
        run(&p, &c, &t, t + TICK, true, true, &locks, &wakes);
    }
    CHECK(locks == 0);
}

static void test_radar_down_never_locks(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    presence_t p;
    int64_t t = 0;
    int locks = 0, wakes = 0;
    presence_init(&p, t);
    run(&p, &c, &t, 5000, true, true, &locks, &wakes);
    run(&p, &c, &t, 300000, false, false, &locks, &wakes);
    CHECK(locks == 0);
    /* Link back, nobody there: the away timer starts from the reconnect. */
    run(&p, &c, &t, 300000 + 20000, true, false, &locks, &wakes);
    CHECK(locks == 0);
    run(&p, &c, &t, 300000 + 31000, true, false, &locks, &wakes);
    CHECK(locks == 1);
}

static void test_not_armed_and_paused(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    presence_t p;
    int64_t t = 0;
    int locks = 0, wakes = 0;
    presence_init(&p, t);
    /* Booted with nobody around: never lock a PC nobody was using. */
    run(&p, &c, &t, 60000, true, false, &locks, &wakes);
    CHECK(locks == 0 && p.state == PRESENCE_STATE_AWAY);
    /* Someone arrives: nothing was locked, so nothing to wake. */
    run(&p, &c, &t, 62000, true, true, &locks, &wakes);
    CHECK(wakes == 0 && p.state == PRESENCE_STATE_PRESENT);

    c.lock_enabled = false;
    presence_init(&p, t);
    run(&p, &c, &t, t + 5000, true, true, &locks, &wakes);
    run(&p, &c, &t, t + 40000, true, false, &locks, &wakes);
    CHECK(locks == 0 && p.state == PRESENCE_STATE_AWAY);
    run(&p, &c, &t, t + 2000, true, true, &locks, &wakes);
    CHECK(wakes == 0); /* paused: no lock, so no wake either */
}

static void test_brief_passerby_does_not_wake(void)
{
    presence_cfg_t c;
    presence_default_cfg(&c);
    presence_t p;
    int64_t t = 0;
    int locks = 0, wakes = 0;
    presence_init(&p, t);
    run(&p, &c, &t, 5000, true, true, &locks, &wakes);
    run(&p, &c, &t, 40000, true, false, &locks, &wakes);
    CHECK(locks == 1);
    /* Three 300 ms glimpses with gaps: no wake. */
    for (int i = 0; i < 3; ++i) {
        run(&p, &c, &t, t + 300, true, true, &locks, &wakes);
        run(&p, &c, &t, t + 2000, true, false, &locks, &wakes);
    }
    CHECK(wakes == 0);
}

int main(void)
{
    test_zone();
    test_leave_and_return();
    test_short_absence_and_flicker();
    test_radar_down_never_locks();
    test_not_armed_and_paused();
    test_brief_passerby_does_not_wake();
    puts("presence tests: PASS");
    return 0;
}
