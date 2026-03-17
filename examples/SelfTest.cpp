#include "SelfTest.h"
#include <iostream>

using namespace std;

SelfTest::SelfTest(uint8_t maxStates) :
    StateMachine(maxStates)
{
}

void SelfTest::Cancel()
{
    PARENT_TRANSITION(ST_FAILED)

    BEGIN_TRANSITION_MAP(SelfTest, Cancel)      // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)     // ST_COMPLETED
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)     // ST_FAILED
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(SelfTest, Idle, NoEventData)
{
    cout << "SelfTest::ST_Idle" << endl;
}

ENTRY_DEFINE(SelfTest, EntryIdle, NoEventData)
{
    cout << "SelfTest::EN_EntryIdle" << endl;
}

STATE_DEFINE(SelfTest, Completed, NoEventData)
{
    cout << "SelfTest::ST_Completed" << endl;
    InternalEvent(ST_IDLE);
}

STATE_DEFINE(SelfTest, Failed, NoEventData)
{
    cout << "SelfTest::ST_Failed" << endl;
    InternalEvent(ST_IDLE);
}
