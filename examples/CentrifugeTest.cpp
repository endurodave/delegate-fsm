#include "CentrifugeTest.h"
#include <iostream>

using namespace std;
using namespace dmq;

CentrifugeTest::CentrifugeTest() :
    SelfTest(ST_MAX_STATES),
    m_threadObj("CentrifugeTestSMThread"),
    m_speed(0)
{
    m_threadObj.CreateThread();
    SetThread(m_threadObj);

    // When the poll timer fires (via ProcessTimers on the caller's thread),
    // dispatch Poll() asynchronously onto the SM thread.
    m_pollTimerConn = m_pollTimer.OnExpired.Connect(
        MakeDelegate(this, &CentrifugeTest::Poll, m_threadObj));
}

CentrifugeTest::~CentrifugeTest()
{
    m_pollTimer.Stop();
    m_threadObj.ExitThread();
}

void CentrifugeTest::Start()
{
    BEGIN_TRANSITION_MAP(CentrifugeTest, Start)     // - Current State -
        TRANSITION_MAP_ENTRY(ST_START_TEST)         // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_COMPLETED
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_FAILED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_START_TEST
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_ACCELERATION
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_WAIT_FOR_ACCELERATION
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_DECELERATION
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_WAIT_FOR_DECELERATION
    END_TRANSITION_MAP(nullptr)
}

void CentrifugeTest::Poll()
{
    BEGIN_TRANSITION_MAP(CentrifugeTest, Poll)              // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_IDLE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_COMPLETED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_FAILED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)                 // ST_START_TEST
        TRANSITION_MAP_ENTRY(ST_WAIT_FOR_ACCELERATION)      // ST_ACCELERATION
        TRANSITION_MAP_ENTRY(ST_WAIT_FOR_ACCELERATION)      // ST_WAIT_FOR_ACCELERATION
        TRANSITION_MAP_ENTRY(ST_WAIT_FOR_DECELERATION)      // ST_DECELERATION
        TRANSITION_MAP_ENTRY(ST_WAIT_FOR_DECELERATION)      // ST_WAIT_FOR_DECELERATION
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(CentrifugeTest, Idle, NoEventData)
{
    cout << "CentrifugeTest::ST_Idle" << endl;
    SelfTest::ST_Idle(data);
    StopPoll();
    OnComplete();
}

STATE_DEFINE(CentrifugeTest, StartTest, NoEventData)
{
    cout << "CentrifugeTest::ST_StartTest" << endl;
    InternalEvent(ST_ACCELERATION);
}

GUARD_DEFINE(CentrifugeTest, GuardStartTest, NoEventData)
{
    cout << "CentrifugeTest::GD_GuardStartTest" << endl;
    return m_speed == 0;
}

STATE_DEFINE(CentrifugeTest, Acceleration, NoEventData)
{
    cout << "CentrifugeTest::ST_Acceleration" << endl;
    StartPoll();
}

STATE_DEFINE(CentrifugeTest, WaitForAcceleration, NoEventData)
{
    cout << "CentrifugeTest::ST_WaitForAcceleration : Speed is " << m_speed << endl;
    if (++m_speed >= 5)
        InternalEvent(ST_DECELERATION);
}

EXIT_DEFINE(CentrifugeTest, ExitWaitForAcceleration)
{
    cout << "CentrifugeTest::EX_ExitWaitForAcceleration" << endl;
    StopPoll();
}

STATE_DEFINE(CentrifugeTest, Deceleration, NoEventData)
{
    cout << "CentrifugeTest::ST_Deceleration" << endl;
    StartPoll();
}

STATE_DEFINE(CentrifugeTest, WaitForDeceleration, NoEventData)
{
    cout << "CentrifugeTest::ST_WaitForDeceleration : Speed is " << m_speed << endl;
    if (m_speed-- == 0)
        InternalEvent(ST_COMPLETED);
}

EXIT_DEFINE(CentrifugeTest, ExitWaitForDeceleration)
{
    cout << "CentrifugeTest::EX_ExitWaitForDeceleration" << endl;
    StopPoll();
}
