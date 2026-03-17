#ifndef _CENTRIFUGE_TEST_H
#define _CENTRIFUGE_TEST_H

#include "SelfTest.h"
#include "delegate-mq/predef/util/Timer.h"

/// @brief CentrifugeTest demonstrates state machine inheritance, state function
/// override, guard/entry/exit actions, and timer-driven polling. The SM runs as
/// an active object on an owned SM thread. A Timer fires Poll() events onto the
/// SM thread periodically while the test is running.
class CentrifugeTest : public SelfTest
{
public:
    CentrifugeTest();
    ~CentrifugeTest();

    virtual void Start();

    // Fired on the SM thread when the test completes and returns to ST_IDLE.
    dmq::Signal<void()> OnComplete;

private:
    Thread                m_threadObj;
    Timer                 m_pollTimer;
    dmq::ScopedConnection m_pollTimerConn;
    int                   m_speed;

    void StartPoll() { m_pollTimer.Start(std::chrono::milliseconds(10)); }
    void StopPoll()  { m_pollTimer.Stop(); }

    // Internal poll event — drives state transitions while test is active.
    void Poll();

    enum States
    {
        ST_START_TEST = SelfTest::ST_MAX_STATES,
        ST_ACCELERATION,
        ST_WAIT_FOR_ACCELERATION,
        ST_DECELERATION,
        ST_WAIT_FOR_DECELERATION,
        ST_MAX_STATES
    };

    // States
    STATE_DECLARE(CentrifugeTest, Idle, NoEventData)
    STATE_DECLARE(CentrifugeTest, StartTest, NoEventData)
    STATE_DECLARE(CentrifugeTest, Acceleration, NoEventData)
    STATE_DECLARE(CentrifugeTest, WaitForAcceleration, NoEventData)
    STATE_DECLARE(CentrifugeTest, Deceleration, NoEventData)
    STATE_DECLARE(CentrifugeTest, WaitForDeceleration, NoEventData)

    // Guard
    GUARD_DECLARE(CentrifugeTest, GuardStartTest, NoEventData)

    // Exit
    EXIT_DECLARE(CentrifugeTest, ExitWaitForAcceleration)
    EXIT_DECLARE(CentrifugeTest, ExitWaitForDeceleration)

    // State map
    BEGIN_STATE_MAP_EX
        STATE_MAP_ENTRY_ALL_EX(&CentrifugeTest::Idle, nullptr, &SelfTest::EntryIdle, nullptr)
        STATE_MAP_ENTRY_EX(&SelfTest::Completed)
        STATE_MAP_ENTRY_EX(&SelfTest::Failed)
        STATE_MAP_ENTRY_ALL_EX(&StartTest, &GuardStartTest, nullptr, nullptr)
        STATE_MAP_ENTRY_EX(&Acceleration)
        STATE_MAP_ENTRY_ALL_EX(&WaitForAcceleration, nullptr, nullptr, &ExitWaitForAcceleration)
        STATE_MAP_ENTRY_EX(&Deceleration)
        STATE_MAP_ENTRY_ALL_EX(&WaitForDeceleration, nullptr, nullptr, &ExitWaitForDeceleration)
    END_STATE_MAP_EX
};

#endif
