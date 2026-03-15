// StateMachineTests.cpp
// Unit tests for the StateMachine base class.
// Each test uses ASSERT_TRUE — failure aborts via FaultHandler.

#include "StateMachineTests.h"
#include "state-machine/StateMachine.h"
#include "examples/Motor.h"
#include "delegate-mq/predef/util/Fault.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

using namespace std;
using namespace dmq;

// ---------------------------------------------------------------------------
// TestSM — minimal two-state SM used to test guards, entry/exit, and
// self-transitions independently of the example state machines.
// ---------------------------------------------------------------------------
class TestSM : public StateMachine
{
public:
    bool m_guardAllow  = true;
    int  m_actionACount = 0;
    int  m_actionBCount = 0;
    int  m_entryBCount  = 0;
    int  m_exitACount   = 0;

    enum States { ST_A, ST_B, ST_MAX_STATES };

    TestSM() : StateMachine(ST_MAX_STATES)
    {
    }

    void GoB()
    {
        BEGIN_TRANSITION_MAP
            TRANSITION_MAP_ENTRY(ST_B)  // ST_A
            TRANSITION_MAP_ENTRY(ST_B)  // ST_B (self-transition)
        END_TRANSITION_MAP(nullptr)
    }

    STATE_DECLARE(TestSM, StateA, NoEventData)
    STATE_DECLARE(TestSM, StateB, NoEventData)
    ENTRY_DECLARE(TestSM, EntryB, NoEventData)
    EXIT_DECLARE(TestSM, ExitA)
    GUARD_DECLARE(TestSM, Guard, NoEventData)

    BEGIN_STATE_MAP_EX
        STATE_MAP_ENTRY_ALL_EX(&StateA, nullptr, nullptr, &ExitA)
        STATE_MAP_ENTRY_ALL_EX(&StateB, &Guard, &EntryB, nullptr)
    END_STATE_MAP_EX
};

STATE_DEFINE(TestSM, StateA, NoEventData) { m_actionACount++; }
STATE_DEFINE(TestSM, StateB, NoEventData) { m_actionBCount++; }
ENTRY_DEFINE(TestSM, EntryB, NoEventData) { m_entryBCount++; }
EXIT_DEFINE(TestSM, ExitA)                { m_exitACount++;  }
GUARD_DEFINE(TestSM, Guard, NoEventData)  { return m_guardAllow; }

// ---------------------------------------------------------------------------
// Parent/Child SM — used to test PARENT_TRANSITION logic.
// ---------------------------------------------------------------------------
class ParentBase : public StateMachine
{
public:
    int m_baseActionCount = 0;
    enum States { ST_IDLE, ST_MAX_STATES };
    ParentBase(uint8_t max) : StateMachine(max) {}

    void CallBaseTransition(uint8_t derivedState)
    {
        PARENT_TRANSITION(derivedState)
        
        BEGIN_TRANSITION_MAP
            TRANSITION_MAP_ENTRY(ST_IDLE)
        END_TRANSITION_MAP(nullptr)
    }

    STATE_DECLARE(ParentBase, BaseIdle, NoEventData)
};
STATE_DEFINE(ParentBase, BaseIdle, NoEventData) { m_baseActionCount++; }

class ChildDerived : public ParentBase
{
public:
    int m_derivedActionCount = 0;
    enum States { ST_DERIVED = ParentBase::ST_MAX_STATES, ST_MAX_STATES };
    ChildDerived() : ParentBase(ST_MAX_STATES) {}

    void GoDerived() {
        BEGIN_TRANSITION_MAP
            TRANSITION_MAP_ENTRY(ST_DERIVED)  // ST_IDLE
            TRANSITION_MAP_ENTRY(ST_DERIVED)  // ST_DERIVED
        END_TRANSITION_MAP(nullptr)
    }

    STATE_DECLARE(ChildDerived, DerivedState, NoEventData)

    BEGIN_STATE_MAP
        STATE_MAP_ENTRY(&ParentBase::BaseIdle)
        STATE_MAP_ENTRY(&DerivedState)
    END_STATE_MAP
};
STATE_DEFINE(ChildDerived, DerivedState, NoEventData) { m_derivedActionCount++; }

// ---------------------------------------------------------------------------
// StressMotor — two-state SM with no CANNOT_HAPPEN entries.
// ---------------------------------------------------------------------------
class StressMotor : public StateMachine
{
public:
    enum States { ST_IDLE, ST_RUNNING, ST_MAX_STATES };

    StressMotor() : StateMachine(ST_MAX_STATES)
    {
    }

    void Run()
    {
        BEGIN_TRANSITION_MAP
            TRANSITION_MAP_ENTRY(ST_RUNNING)    // ST_IDLE
            TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_RUNNING
        END_TRANSITION_MAP(nullptr)
    }

    void Stop()
    {
        BEGIN_TRANSITION_MAP
            TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_IDLE
            TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_RUNNING
        END_TRANSITION_MAP(nullptr)
    }

    STATE_DECLARE(StressMotor, StateIdle, NoEventData)
    STATE_DECLARE(StressMotor, StateRunning, NoEventData)

    BEGIN_STATE_MAP
        STATE_MAP_ENTRY(&StateIdle)
        STATE_MAP_ENTRY(&StateRunning)
    END_STATE_MAP
};

STATE_DEFINE(StressMotor, StateIdle, NoEventData)    {}
STATE_DEFINE(StressMotor, StateRunning, NoEventData) {}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestInitialState()
{
    Motor m;
    ASSERT_TRUE(m.GetCurrentState() == 0);  // ST_IDLE == 0
}

static void TestMaxStates()
{
    Motor m;
    ASSERT_TRUE(m.GetMaxStates() == 4);  // Motor::ST_MAX_STATES
}

static void TestBasicTransition()
{
    Motor m;
    auto d = new MotorData(); d->speed = 100;
    m.SetSpeed(d);
    ASSERT_TRUE(m.GetCurrentState() == 2);  // ST_START
}

static void TestSelfTransitionDoesNotChangeState()
{
    Motor m;
    auto d1 = new MotorData(); d1->speed = 100;
    m.SetSpeed(d1);
    ASSERT_TRUE(m.GetCurrentState() == 2);  // ST_START

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);
    ASSERT_TRUE(m.GetCurrentState() == 3);  // ST_CHANGE_SPEED

    auto d3 = new MotorData(); d3->speed = 300;
    m.SetSpeed(d3);
    ASSERT_TRUE(m.GetCurrentState() == 3);  // still ST_CHANGE_SPEED
}

static void TestInternalEventChain()
{
    // Motor::ST_Stop calls InternalEvent(ST_IDLE), so Halt() from START
    // should land in ST_IDLE, not ST_STOP.
    Motor m;
    auto d = new MotorData(); d->speed = 100;
    m.SetSpeed(d);
    ASSERT_TRUE(m.GetCurrentState() == 2);  // ST_START
    m.Halt();
    ASSERT_TRUE(m.GetCurrentState() == 0);  // ST_IDLE (via ST_STOP -> InternalEvent)
}

static void TestEventIgnored()
{
    Motor m;
    m.Halt();  // EVENT_IGNORED in ST_IDLE
    ASSERT_TRUE(m.GetCurrentState() == 0);  // ST_IDLE unchanged
}

static void TestOnTransitionSignal()
{
    Motor m;
    uint8_t capturedFrom = 0xFF;
    uint8_t capturedTo   = 0xFF;
    auto conn = m.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t f, uint8_t t) { capturedFrom = f; capturedTo = t; })));

    auto d = new MotorData(); d->speed = 100;
    m.SetSpeed(d);
    ASSERT_TRUE(capturedFrom == 0);  // from ST_IDLE
    ASSERT_TRUE(capturedTo   == 2);  // to ST_START
}

static void TestOnEntrySignalFiresOnStateChange()
{
    Motor m;
    int entryCount = 0;
    auto conn = m.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t) { entryCount++; })));

    auto d1 = new MotorData(); d1->speed = 100;
    m.SetSpeed(d1);  // IDLE -> START: entry fires
    ASSERT_TRUE(entryCount == 1);

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);  // START -> CHANGE_SPEED: entry fires
    ASSERT_TRUE(entryCount == 2);

    auto d3 = new MotorData(); d3->speed = 300;
    m.SetSpeed(d3);  // CHANGE_SPEED -> CHANGE_SPEED: self-transition, no entry
    ASSERT_TRUE(entryCount == 2);
}

static void TestOnExitSignalFiresOnStateChange()
{
    Motor m;
    int exitCount = 0;
    auto conn = m.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t) { exitCount++; })));

    auto d1 = new MotorData(); d1->speed = 100;
    m.SetSpeed(d1);  // IDLE -> START: exit fires for IDLE
    ASSERT_TRUE(exitCount == 1);

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);  // START -> CHANGE_SPEED: exit fires for START
    ASSERT_TRUE(exitCount == 2);

    auto d3 = new MotorData(); d3->speed = 300;
    m.SetSpeed(d3);  // CHANGE_SPEED -> CHANGE_SPEED: self-transition, no exit
    ASSERT_TRUE(exitCount == 2);
}

static void TestOnTransitionSelfTransition()
{
    // OnTransition fires even for self-transitions (fromState == toState).
    Motor m;
    int transCount = 0;
    auto conn = m.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) { transCount++; })));

    auto d1 = new MotorData(); d1->speed = 100;
    m.SetSpeed(d1);  // IDLE -> START
    ASSERT_TRUE(transCount == 1);

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);  // START -> CHANGE_SPEED
    ASSERT_TRUE(transCount == 2);

    auto d3 = new MotorData(); d3->speed = 300;
    m.SetSpeed(d3);  // CHANGE_SPEED -> CHANGE_SPEED (self-transition still fires)
    ASSERT_TRUE(transCount == 3);
}

static void TestGuardAllows()
{
    TestSM sm;
    sm.m_guardAllow = true;
    sm.GoB();  // ST_A -> ST_B, guard passes
    ASSERT_TRUE(sm.GetCurrentState() == TestSM::ST_B);
    ASSERT_TRUE(sm.m_actionBCount == 1);
    ASSERT_TRUE(sm.m_entryBCount  == 1);  // entry fires (A != B)
    ASSERT_TRUE(sm.m_exitACount   == 1);  // exit fires from A
}

static void TestGuardBlocks()
{
    TestSM sm;
    sm.m_guardAllow = false;
    sm.GoB();  // ST_A -> ST_B, guard vetoes
    ASSERT_TRUE(sm.GetCurrentState() == TestSM::ST_A);  // stayed in A
    ASSERT_TRUE(sm.m_actionBCount == 0);
    ASSERT_TRUE(sm.m_entryBCount  == 0);  // entry did not fire
    ASSERT_TRUE(sm.m_exitACount   == 0);  // exit did not fire
}

static void TestGuardSelfTransitionBlocked()
{
    TestSM sm;
    sm.m_guardAllow = true;
    sm.GoB();  // A -> B
    ASSERT_TRUE(sm.GetCurrentState() == TestSM::ST_B);

    sm.m_guardAllow = false;
    sm.GoB();  // B -> B self-transition, guard vetoes
    ASSERT_TRUE(sm.GetCurrentState() == TestSM::ST_B);  // still B
    ASSERT_TRUE(sm.m_actionBCount == 1);  // action did not fire again
}

static void TestMultipleTransitions()
{
    // Sequence through all Motor states and verify final state.
    Motor m;
    auto d1 = new MotorData(); d1->speed = 100;
    m.SetSpeed(d1);
    ASSERT_TRUE(m.GetCurrentState() == 2);  // ST_START

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);
    ASSERT_TRUE(m.GetCurrentState() == 3);  // ST_CHANGE_SPEED

    m.Halt();
    ASSERT_TRUE(m.GetCurrentState() == 0);  // ST_IDLE (via ST_STOP)
}

static void TestConcurrentEventDispatch()
{
    // N threads post Run/Stop events concurrently; the SM thread serialises
    // them via its dispatch queue. Completing without crash and landing in a
    // valid state verifies queue integrity under concurrent producers.
    const int N_THREADS    = 8;
    const int N_ITERATIONS = 200;

    Thread smThread("StressMotorSMThread");
    smThread.CreateThread();

    StressMotor motor;
    motor.SetThread(smThread);

    std::atomic<int> transitionCount{0};
    auto conn = motor.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) { transitionCount.fetch_add(1, std::memory_order_relaxed); })));

    std::vector<std::thread> producers;
    producers.reserve(N_THREADS);
    for (int i = 0; i < N_THREADS; ++i)
    {
        producers.emplace_back([&motor, N_ITERATIONS, i]() {
            for (int j = 0; j < N_ITERATIONS; ++j)
            {
                if ((i + j) % 2 == 0)
                    motor.Run();
                else
                    motor.Stop();
            }
        });
    }

    for (auto& t : producers)
        t.join();

    smThread.ExitThread();  // drains queue before reading final state

    ASSERT_TRUE(transitionCount > 0);
    ASSERT_TRUE(motor.GetCurrentState() < motor.GetMaxStates());
}

static void TestParentTransition()
{
    ChildDerived child;
    ASSERT_TRUE(child.GetCurrentState() == ChildDerived::ST_IDLE);

    // Use derived class event to get into derived state.
    child.GoDerived(); 
    ASSERT_TRUE(child.GetCurrentState() == ChildDerived::ST_DERIVED);

    // Now call base event while in derived state. 
    // PARENT_TRANSITION macro in ParentBase should trigger because 
    // GetCurrentState() (ST_DERIVED) >= ParentBase::ST_MAX_STATES.
    child.CallBaseTransition(ChildDerived::ST_IDLE);
    ASSERT_TRUE(child.GetCurrentState() == ChildDerived::ST_IDLE);
    ASSERT_TRUE(child.m_baseActionCount == 1);
}

static void TestSignalDisconnection()
{
    Motor m;
    int callCount = 0;
    {
        auto conn = m.OnTransition.Connect(
            MakeDelegate(std::function<void(uint8_t, uint8_t)>(
                [&](uint8_t, uint8_t) { callCount++; })));

        auto d1 = new MotorData(); d1->speed = 100;
        m.SetSpeed(d1);
        ASSERT_TRUE(callCount == 1);
    } // ScopedConnection goes out of scope here

    auto d2 = new MotorData(); d2->speed = 200;
    m.SetSpeed(d2);
    ASSERT_TRUE(callCount == 1); // should not have incremented
}

static void TestNoEventDataCase()
{
    Motor m;
    // Halt() uses END_TRANSITION_MAP(nullptr). 
    // StateMachine should pass NoEventData to ST_Stop.
    m.Halt(); 
    ASSERT_TRUE(m.GetCurrentState() == 0); // ST_STOP -> ST_IDLE
}

// ---------------------------------------------------------------------------
// RunStateMachineTests
// ---------------------------------------------------------------------------
void RunStateMachineTests()
{
    cout << "\n=== StateMachine Unit Tests ===" << endl;

    TestInitialState();               cout << "  PASS TestInitialState" << endl;
    TestMaxStates();                  cout << "  PASS TestMaxStates" << endl;
    TestBasicTransition();            cout << "  PASS TestBasicTransition" << endl;
    TestSelfTransitionDoesNotChangeState(); cout << "  PASS TestSelfTransitionDoesNotChangeState" << endl;
    TestInternalEventChain();         cout << "  PASS TestInternalEventChain" << endl;
    TestEventIgnored();               cout << "  PASS TestEventIgnored" << endl;
    TestOnTransitionSignal();         cout << "  PASS TestOnTransitionSignal" << endl;
    TestOnEntrySignalFiresOnStateChange(); cout << "  PASS TestOnEntrySignalFiresOnStateChange" << endl;
    TestOnExitSignalFiresOnStateChange();  cout << "  PASS TestOnExitSignalFiresOnStateChange" << endl;
    TestOnTransitionSelfTransition(); cout << "  PASS TestOnTransitionSelfTransition" << endl;
    TestGuardAllows();                cout << "  PASS TestGuardAllows" << endl;
    TestGuardBlocks();                cout << "  PASS TestGuardBlocks" << endl;
    TestGuardSelfTransitionBlocked(); cout << "  PASS TestGuardSelfTransitionBlocked" << endl;
    TestMultipleTransitions();        cout << "  PASS TestMultipleTransitions" << endl;
    TestConcurrentEventDispatch();    cout << "  PASS TestConcurrentEventDispatch" << endl;
    TestParentTransition();           cout << "  PASS TestParentTransition" << endl;
    TestSignalDisconnection();        cout << "  PASS TestSignalDisconnection" << endl;
    TestNoEventDataCase();            cout << "  PASS TestNoEventDataCase" << endl;

    cout << "All StateMachine tests passed." << endl;
}
