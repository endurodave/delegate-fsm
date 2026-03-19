// StateMachineHSMTests.cpp
// Unit tests for StateMachineHSM.
// Each test uses ASSERT_TRUE — failure aborts via FaultHandler.

#include "StateMachineHSMTests.h"
#include "state-machine/StateMachineHSM.h"
#include "examples/AlarmPanel.h"
#include "delegate-mq/predef/util/Fault.h"
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <future>

using namespace std;
using namespace dmq;

// ---------------------------------------------------------------------------
// TestHSM — minimal four-state HSM used to test LCA, entry/exit ordering,
// propagation, and guards independently of AlarmPanel.
//
// Hierarchy:
//   ST_TOP     (top-level)
//   ST_PARENT  (top-level, composite parent)
//     ST_CHILD1  (child of PARENT — has entry and exit)
//     ST_CHILD2  (child of PARENT — has entry and guard)
// ---------------------------------------------------------------------------
class TestHSM : public StateMachineHSM
{
public:
    vector<string> m_log;   // records calls in order: "EN_x", "EX_x", "ST_x"
    bool m_guardAllow = true;

    enum States { ST_TOP, ST_PARENT, ST_CHILD1, ST_CHILD2, ST_MAX_STATES };

    TestHSM() : StateMachineHSM(ST_MAX_STATES, ST_TOP) {}

    // Always transition to CHILD1 from any state.
    void GoChild1()
    {
        BEGIN_TRANSITION_MAP(TestHSM, GoChild1)
            TRANSITION_MAP_ENTRY(ST_CHILD1)     // ST_TOP
            TRANSITION_MAP_ENTRY(ST_CHILD1)     // ST_PARENT
            TRANSITION_MAP_ENTRY(ST_CHILD1)     // ST_CHILD1 (self)
            TRANSITION_MAP_ENTRY(ST_CHILD1)     // ST_CHILD2
        END_TRANSITION_MAP_HSM(nullptr)
    }

    // Always transition to CHILD2 from any state.
    void GoChild2()
    {
        BEGIN_TRANSITION_MAP(TestHSM, GoChild2)
            TRANSITION_MAP_ENTRY(ST_CHILD2)     // ST_TOP
            TRANSITION_MAP_ENTRY(ST_CHILD2)     // ST_PARENT
            TRANSITION_MAP_ENTRY(ST_CHILD2)     // ST_CHILD1
            TRANSITION_MAP_ENTRY(ST_CHILD2)     // ST_CHILD2 (self)
        END_TRANSITION_MAP_HSM(nullptr)
    }

    // PARENT and children go to ST_TOP (children propagate to parent).
    void GoTop()
    {
        BEGIN_TRANSITION_MAP(TestHSM, GoTop)
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_TOP
            TRANSITION_MAP_ENTRY(ST_TOP)            // ST_PARENT  <-- handles it
            TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT) // ST_CHILD1 --> ST_PARENT
            TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT) // ST_CHILD2 --> ST_PARENT
        END_TRANSITION_MAP_HSM(nullptr)
    }

    // Toggle between siblings; ignored elsewhere.
    void Toggle()
    {
        BEGIN_TRANSITION_MAP(TestHSM, Toggle)
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_TOP
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_PARENT
            TRANSITION_MAP_ENTRY(ST_CHILD2)         // ST_CHILD1
            TRANSITION_MAP_ENTRY(ST_CHILD1)         // ST_CHILD2
        END_TRANSITION_MAP_HSM(nullptr)
    }

    STATE_DECLARE(TestHSM, Top,    NoEventData)
    STATE_DECLARE(TestHSM, Parent, NoEventData)
    STATE_DECLARE(TestHSM, Child1, NoEventData)
    STATE_DECLARE(TestHSM, Child2, NoEventData)

    ENTRY_DECLARE(TestHSM, EntryParent, NoEventData)
    EXIT_DECLARE (TestHSM, ExitParent)
    ENTRY_DECLARE(TestHSM, EntryChild1, NoEventData)
    EXIT_DECLARE (TestHSM, ExitChild1)
    ENTRY_DECLARE(TestHSM, EntryChild2, NoEventData)
    GUARD_DECLARE(TestHSM, GuardChild2, NoEventData)

    //                                State     Guard         Entry          Exit         Parent
    BEGIN_STATE_MAP_HSM
        STATE_MAP_ENTRY_ALL_HSM(&Top,    nullptr,      nullptr,       nullptr,       NO_PARENT)
        STATE_MAP_ENTRY_ALL_HSM(&Parent, nullptr,      &EntryParent,  &ExitParent,   NO_PARENT)
        STATE_MAP_ENTRY_ALL_HSM(&Child1, nullptr,      &EntryChild1,  &ExitChild1,   ST_PARENT)
        STATE_MAP_ENTRY_ALL_HSM(&Child2, &GuardChild2, &EntryChild2,  nullptr,       ST_PARENT)
    END_STATE_MAP_HSM
};

STATE_DEFINE(TestHSM, Top,    NoEventData) { m_log.push_back("ST_TOP");    }
STATE_DEFINE(TestHSM, Parent, NoEventData) { m_log.push_back("ST_PARENT"); }
STATE_DEFINE(TestHSM, Child1, NoEventData) { m_log.push_back("ST_CHILD1"); }
STATE_DEFINE(TestHSM, Child2, NoEventData) { m_log.push_back("ST_CHILD2"); }

ENTRY_DEFINE(TestHSM, EntryParent, NoEventData) { m_log.push_back("EN_PARENT"); }
EXIT_DEFINE (TestHSM, ExitParent)               { m_log.push_back("EX_PARENT"); }
ENTRY_DEFINE(TestHSM, EntryChild1, NoEventData) { m_log.push_back("EN_CHILD1"); }
EXIT_DEFINE (TestHSM, ExitChild1)               { m_log.push_back("EX_CHILD1"); }
ENTRY_DEFINE(TestHSM, EntryChild2, NoEventData) { m_log.push_back("EN_CHILD2"); }
GUARD_DEFINE(TestHSM, GuardChild2, NoEventData) { return m_guardAllow; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// AlarmPanel state indices — mirrors the private enum in AlarmPanel.h.
enum AlarmState { AL_DISARMED = 0, AL_ARMED = 1, AL_ARMED_HOME = 2, AL_ARMED_AWAY = 3, AL_ALARMING = 4 };

// ---------------------------------------------------------------------------
// Tests — structural (TestHSM)
// ---------------------------------------------------------------------------

static void TestHSMInitialState()
{
    TestHSM sm;
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_TOP);
}

// TOP → CHILD1: parent entry fires before child entry, then state action.
static void TestHSMEntryOrderParentFirst()
{
    TestHSM sm;
    sm.GoChild1();
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD1);
    ASSERT_TRUE(sm.m_log.size() == 3);
    ASSERT_TRUE(sm.m_log[0] == "EN_PARENT");  // parent enters before child
    ASSERT_TRUE(sm.m_log[1] == "EN_CHILD1");
    ASSERT_TRUE(sm.m_log[2] == "ST_CHILD1");
}

// CHILD1 → CHILD2: only child exit/entry fires; PARENT is the LCA — not exited/entered.
static void TestHSMSiblingTransitionSkipsParentExitEntry()
{
    TestHSM sm;
    sm.GoChild1();
    sm.m_log.clear();

    sm.Toggle();  // CHILD1 → CHILD2
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD2);
    ASSERT_TRUE(sm.m_log.size() == 3);
    ASSERT_TRUE(sm.m_log[0] == "EX_CHILD1");
    ASSERT_TRUE(sm.m_log[1] == "EN_CHILD2");
    ASSERT_TRUE(sm.m_log[2] == "ST_CHILD2");
}

// CHILD1 → TOP: child exit and parent exit both fire; top has no entry action.
static void TestHSMExitOrderChildBeforeParent()
{
    TestHSM sm;
    sm.GoChild1();
    sm.m_log.clear();

    sm.GoTop();  // CHILD1 propagates → PARENT → ST_TOP
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_TOP);
    ASSERT_TRUE(sm.m_log.size() == 3);
    ASSERT_TRUE(sm.m_log[0] == "EX_CHILD1");  // child exits first
    ASSERT_TRUE(sm.m_log[1] == "EX_PARENT");  // then parent
    ASSERT_TRUE(sm.m_log[2] == "ST_TOP");
}

// TOP → CHILD1 → TOP: full round-trip with correct entry/exit sequencing.
static void TestHSMRoundTrip()
{
    TestHSM sm;
    sm.GoChild1();
    sm.GoTop();
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_TOP);

    sm.m_log.clear();
    sm.GoChild2();  // TOP → CHILD2: EN_PARENT, EN_CHILD2, ST_CHILD2
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD2);
    ASSERT_TRUE(sm.m_log[0] == "EN_PARENT");
    ASSERT_TRUE(sm.m_log[1] == "EN_CHILD2");
    ASSERT_TRUE(sm.m_log[2] == "ST_CHILD2");
}

// PROPAGATE_TO_PARENT resolves through parent's transition entry.
static void TestHSMPropagateToParent()
{
    TestHSM sm;
    sm.GoChild1();  // now in CHILD1
    sm.m_log.clear();

    sm.GoTop();     // CHILD1 propagates GoTop → PARENT handles → ST_TOP
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_TOP);
}

// Guard blocks transition — state stays unchanged, no entry/exit fires.
static void TestHSMGuardBlocks()
{
    TestHSM sm;
    sm.GoChild1();
    sm.m_log.clear();

    sm.m_guardAllow = false;
    sm.GoChild2();  // guard vetoes CHILD1 → CHILD2
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD1);
    ASSERT_TRUE(sm.m_log.empty());  // nothing fired
}

// Guard allows transition.
static void TestHSMGuardAllows()
{
    TestHSM sm;
    sm.GoChild1();
    sm.m_log.clear();

    sm.m_guardAllow = true;
    sm.GoChild2();
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD2);
    ASSERT_TRUE(sm.m_log[0] == "EX_CHILD1");
    ASSERT_TRUE(sm.m_log[1] == "EN_CHILD2");
}

// Self-transition does not fire entry/exit.
static void TestHSMSelfTransition()
{
    TestHSM sm;
    sm.GoChild1();
    sm.m_log.clear();

    sm.GoChild1();  // CHILD1 → CHILD1 self-transition
    ASSERT_TRUE(sm.GetCurrentState() == TestHSM::ST_CHILD1);
    // No exit or entry — only state action.
    ASSERT_TRUE(sm.m_log.size() == 1);
    ASSERT_TRUE(sm.m_log[0] == "ST_CHILD1");
}

// OnEntry signal fires for each state entered (parent then child).
static void TestHSMOnEntrySignalOrder()
{
    TestHSM sm;
    vector<uint8_t> entered;
    auto conn = sm.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { entered.push_back(s); })));

    sm.GoChild1();  // TOP → CHILD1: OnEntry fires for PARENT then CHILD1
    ASSERT_TRUE(entered.size() == 2);
    ASSERT_TRUE(entered[0] == TestHSM::ST_PARENT);
    ASSERT_TRUE(entered[1] == TestHSM::ST_CHILD1);
}

// OnExit signal fires for each state exited (child then parent).
static void TestHSMOnExitSignalOrder()
{
    TestHSM sm;
    sm.GoChild1();

    vector<uint8_t> exited;
    auto conn = sm.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { exited.push_back(s); })));

    sm.GoTop();  // CHILD1 → TOP: OnExit fires for CHILD1 then PARENT
    ASSERT_TRUE(exited.size() == 2);
    ASSERT_TRUE(exited[0] == TestHSM::ST_CHILD1);
    ASSERT_TRUE(exited[1] == TestHSM::ST_PARENT);
}

// OnTransition fires once per external event with correct from/to states.
static void TestHSMOnTransitionSignal()
{
    TestHSM sm;
    uint8_t capturedFrom = 0xFF, capturedTo = 0xFF;
    auto conn = sm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t f, uint8_t t) { capturedFrom = f; capturedTo = t; })));

    sm.GoChild1();
    ASSERT_TRUE(capturedFrom == TestHSM::ST_TOP);
    ASSERT_TRUE(capturedTo   == TestHSM::ST_CHILD1);
}

// ---------------------------------------------------------------------------
// Tests — behavioral (AlarmPanel)
// ---------------------------------------------------------------------------

static void TestAlarmInitialState()
{
    AlarmPanel alarm;
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
}

// ArmHome: DISARMED → ARMED_HOME — parent entry fires before child entry.
static void TestAlarmArmHome()
{
    AlarmPanel alarm;
    int entryArmedCount = 0, entryArmedHomeCount = 0;
    vector<uint8_t> entryOrder;

    auto conn = alarm.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { entryOrder.push_back(s); })));

    alarm.ArmHome();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_HOME);
    ASSERT_TRUE(entryOrder.size() == 2);
    ASSERT_TRUE(entryOrder[0] == AL_ARMED);       // parent first
    ASSERT_TRUE(entryOrder[1] == AL_ARMED_HOME);  // child second
}

// Toggle ARMED_HOME → ARMED_AWAY: sibling transition — ARMED not re-entered or exited.
static void TestAlarmToggleDoesNotRepeatParentEntryExit()
{
    AlarmPanel alarm;
    alarm.ArmHome();

    int armedEntryCount = 0, armedExitCount = 0;
    auto entryConn = alarm.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == AL_ARMED) armedEntryCount++; })));
    auto exitConn = alarm.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == AL_ARMED) armedExitCount++; })));

    alarm.Toggle();  // ARMED_HOME → ARMED_AWAY, LCA = ARMED
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_AWAY);
    ASSERT_TRUE(armedEntryCount == 0);  // ARMED not re-entered
    ASSERT_TRUE(armedExitCount  == 0);  // ARMED not exited
}

// Disarm from ARMED_HOME propagates to ARMED → DISARMED; ARMED exit fires.
static void TestAlarmDisarmFromArmedHomePropagatesToArmed()
{
    AlarmPanel alarm;
    alarm.ArmHome();

    int armedExitCount = 0;
    auto conn = alarm.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == AL_ARMED) armedExitCount++; })));

    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
    ASSERT_TRUE(armedExitCount == 1);
}

// Disarm from ARMED_AWAY also propagates correctly.
static void TestAlarmDisarmFromArmedAway()
{
    AlarmPanel alarm;
    alarm.ArmAway();
    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
}

// Trigger from ARMED_HOME propagates to ARMED → ALARMING; ARMED exit fires.
static void TestAlarmTriggerPropagatesFromChild()
{
    AlarmPanel alarm;
    alarm.ArmHome();

    int armedExitCount = 0;
    auto conn = alarm.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == AL_ARMED) armedExitCount++; })));

    auto t = xmake_shared<TriggerData>(); t->zone = 1;
    alarm.Trigger(t);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);
    ASSERT_TRUE(armedExitCount == 1);
}

// Trigger from ARMED_AWAY also propagates correctly.
static void TestAlarmTriggerFromArmedAway()
{
    AlarmPanel alarm;
    alarm.ArmAway();
    auto t = xmake_shared<TriggerData>(); t->zone = 2;
    alarm.Trigger(t);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);
}

// Disarm from ALARMING is a direct (non-propagated) transition.
static void TestAlarmDisarmFromAlarming()
{
    AlarmPanel alarm;
    alarm.ArmHome();
    auto t = xmake_shared<TriggerData>(); t->zone = 1;
    alarm.Trigger(t);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);

    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
}

// Events that should be ignored in DISARMED do not change state.
static void TestAlarmEventsIgnoredWhenDisarmed()
{
    AlarmPanel alarm;
    alarm.Toggle();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);

    auto t = xmake_shared<TriggerData>(); t->zone = 1;
    alarm.Trigger(t);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
}

// Trigger in ALARMING is ignored (alarm is already sounding).
static void TestAlarmTriggerIgnoredInAlarming()
{
    AlarmPanel alarm;
    alarm.ArmAway();
    auto t1 = xmake_shared<TriggerData>(); t1->zone = 1;
    alarm.Trigger(t1);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);

    auto t2 = xmake_shared<TriggerData>(); t2->zone = 2;
    alarm.Trigger(t2);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);  // no change
}

// Re-arm after disarm produces correct entry sequence again.
static void TestAlarmRearm()
{
    AlarmPanel alarm;
    alarm.ArmHome();
    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);

    int entryArmedCount = 0;
    auto conn = alarm.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == AL_ARMED) entryArmedCount++; })));

    alarm.ArmAway();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_AWAY);
    ASSERT_TRUE(entryArmedCount == 1);  // ARMED entry fired again on re-arm
}

// Full scenario: arm → trigger → disarm → re-arm → disarm.
static void TestAlarmFullScenario()
{
    AlarmPanel alarm;

    alarm.ArmHome();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_HOME);

    alarm.Toggle();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_AWAY);

    auto t = xmake_shared<TriggerData>(); t->zone = 5;
    alarm.Trigger(t);
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ALARMING);

    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);

    alarm.ArmHome();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_ARMED_HOME);

    alarm.Disarm();
    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);
}

// ---------------------------------------------------------------------------
// StressHSM — minimal HSM for concurrent dispatch testing.
// No CANNOT_HAPPEN entries so any event is safe from any state.
//
// Hierarchy:
//   ST_IDLE    (top-level)
//   ST_PARENT  (top-level, composite parent)
//     ST_CHILD (child of PARENT)
//
// Activate:   any state → ST_CHILD (enters hierarchy)
// Deactivate: ST_CHILD propagates to ST_PARENT → ST_IDLE (exits hierarchy)
// ---------------------------------------------------------------------------
class StressHSM : public StateMachineHSM
{
public:
    enum States { ST_IDLE, ST_PARENT, ST_CHILD, ST_MAX_STATES };

    StressHSM() : StateMachineHSM(ST_MAX_STATES, ST_IDLE) {}

    void Activate()
    {
        BEGIN_TRANSITION_MAP(StressHSM, Activate)
            TRANSITION_MAP_ENTRY(ST_CHILD)              // ST_IDLE
            TRANSITION_MAP_ENTRY(ST_CHILD)              // ST_PARENT
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CHILD
        END_TRANSITION_MAP_HSM(nullptr)
    }

    void Deactivate()
    {
        BEGIN_TRANSITION_MAP(StressHSM, Deactivate)
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_IDLE
            TRANSITION_MAP_ENTRY(ST_IDLE)               // ST_PARENT  <-- handles it
            TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT)   // ST_CHILD   --> ST_PARENT
        END_TRANSITION_MAP_HSM(nullptr)
    }

    STATE_DECLARE(StressHSM, Idle,   NoEventData)
    STATE_DECLARE(StressHSM, Parent, NoEventData)
    STATE_DECLARE(StressHSM, Child,  NoEventData)

    BEGIN_STATE_MAP_HSM
        STATE_MAP_ENTRY_HSM(&Idle,   NO_PARENT)
        STATE_MAP_ENTRY_HSM(&Parent, NO_PARENT)
        STATE_MAP_ENTRY_HSM(&Child,  ST_PARENT)
    END_STATE_MAP_HSM
};

STATE_DEFINE(StressHSM, Idle,   NoEventData) {}
STATE_DEFINE(StressHSM, Parent, NoEventData) {}
STATE_DEFINE(StressHSM, Child,  NoEventData) {}

// ---------------------------------------------------------------------------
// Async tests
//
// promise/future is used to synchronize instead of ExitThread() because the
// thread uses a priority queue: MSG_EXIT_THREAD has the same NORMAL priority
// as MSG_DISPATCH_DELEGATE, so ExitThread() cannot guarantee all queued
// events are processed before the exit message is dequeued.
//
// promise::set_value() synchronizes-with future::get() in the C++ memory
// model, so all SM-thread writes before set_value() (including SetCurrentState)
// are visible on the main thread after get() returns — no extra atomics needed
// for reading GetCurrentState() after the future resolves.
// ---------------------------------------------------------------------------

// Basic async dispatch: three events are posted asynchronously; we wait for
// the third OnTransition before checking the final state.
static void TestHSMAsyncBasicDispatch()
{
    Thread smThread("StressHSMThread_Basic");
    smThread.CreateThread();

    StressHSM sm;
    sm.SetThread(smThread);

    atomic<int> transCount{0};
    promise<void> done;
    auto conn = sm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) {
                if (transCount.fetch_add(1) + 1 == 3)
                    done.set_value();
            })));

    sm.Activate();    // IDLE → CHILD
    sm.Deactivate();  // CHILD → IDLE (via propagation)
    sm.Activate();    // IDLE → CHILD  (3rd transition)

    done.get_future().get();  // wait until 3rd OnTransition fires on SM thread

    ASSERT_TRUE(sm.GetCurrentState() == StressHSM::ST_CHILD);

    smThread.ExitThread();
}

// OnTransition fires exactly once per event in async mode.
static void TestHSMAsyncTransitionCount()
{
    Thread smThread("StressHSMThread_Count");
    smThread.CreateThread();

    StressHSM sm;
    sm.SetThread(smThread);

    atomic<int> count{0};
    promise<void> done;
    auto conn = sm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) {
                if (count.fetch_add(1) + 1 == 3)
                    done.set_value();
            })));

    sm.Activate();
    sm.Deactivate();
    sm.Activate();

    done.get_future().get();

    ASSERT_TRUE(count == 3);

    smThread.ExitThread();
}

// Entry/exit counts remain correct when events are dispatched asynchronously.
// Activate enters the PARENT/CHILD hierarchy; Deactivate exits it.
static void TestHSMAsyncEntryExitCounts()
{
    Thread smThread("StressHSMThread_EntryExit");
    smThread.CreateThread();

    StressHSM sm;
    sm.SetThread(smThread);

    atomic<int> entryParent{0}, exitParent{0}, transCount{0};
    promise<void> done;

    auto entryConn = sm.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == StressHSM::ST_PARENT) entryParent++; })));
    auto exitConn = sm.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [&](uint8_t s) { if (s == StressHSM::ST_PARENT) exitParent++; })));
    auto transConn = sm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) {
                if (transCount.fetch_add(1) + 1 == 3)
                    done.set_value();
            })));

    sm.Activate();    // IDLE → CHILD: EntryParent fires
    sm.Deactivate();  // CHILD → IDLE: ExitParent fires
    sm.Activate();    // IDLE → CHILD: EntryParent fires again

    done.get_future().get();

    ASSERT_TRUE(entryParent == 2);  // entered PARENT hierarchy twice
    ASSERT_TRUE(exitParent  == 1);  // exited PARENT hierarchy once

    smThread.ExitThread();
}

// N producer threads post Activate/Deactivate concurrently. The SM thread
// serialises them. Completing without a crash and landing in a valid state
// verifies queue integrity under concurrent producers. Weak assertions match
// the flat TestConcurrentEventDispatch pattern — exact counts are not
// deterministic when MSG_EXIT_THREAD races with pending dispatch messages.
static void TestHSMAsyncConcurrentEventDispatch()
{
    const int N_THREADS    = 8;
    const int N_ITERATIONS = 200;

    Thread smThread("StressHSMThread_Concurrent");
    smThread.CreateThread();

    StressHSM sm;
    sm.SetThread(smThread);

    atomic<int> transitionCount{0};
    auto conn = sm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t, uint8_t) { transitionCount.fetch_add(1, memory_order_relaxed); })));

    vector<std::thread> producers;
    producers.reserve(N_THREADS);
    for (int i = 0; i < N_THREADS; ++i)
    {
        producers.emplace_back([&sm, N_ITERATIONS, i]() {
            for (int j = 0; j < N_ITERATIONS; ++j)
            {
                if ((i + j) % 2 == 0)
                    sm.Activate();
                else
                    sm.Deactivate();
            }
        });
    }

    for (auto& t : producers)
        t.join();

    smThread.ExitThread();

    ASSERT_TRUE(transitionCount > 0);
    ASSERT_TRUE(sm.GetCurrentState() < sm.GetMaxStates());
}

// Async AlarmPanel: arm → trigger → disarm sequence posted asynchronously.
// Events are enqueued in order on the SM thread, so the sequence is
// deterministic. We wait for the final Disarm transition before asserting.
static void TestHSMAsyncAlarmSequence()
{
    Thread smThread("AlarmPanelSMThread");
    smThread.CreateThread();

    AlarmPanel alarm;
    alarm.SetThread(smThread);

    promise<void> done;
    auto conn = alarm.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [&](uint8_t from, uint8_t to) {
                // Disarm from ALARMING is the last step in the sequence.
                if (from == AL_ALARMING && to == AL_DISARMED)
                    done.set_value();
            })));

    alarm.ArmAway();
    auto t = xmake_shared<TriggerData>(); t->zone = 7;
    alarm.Trigger(t);  // ARMED_AWAY propagates → ARMED → ALARMING
    alarm.Disarm();    // ALARMING → DISARMED

    done.get_future().get();

    ASSERT_TRUE(alarm.GetCurrentState() == AL_DISARMED);

    smThread.ExitThread();
}

// ---------------------------------------------------------------------------
// RunStateMachineHSMTests
// ---------------------------------------------------------------------------
void RunStateMachineHSMTests()
{
    cout << "\n=== StateMachineHSM Unit Tests ===" << endl;

    // Structural tests (TestHSM)
    TestHSMInitialState();                        cout << "  PASS TestHSMInitialState" << endl;
    TestHSMEntryOrderParentFirst();               cout << "  PASS TestHSMEntryOrderParentFirst" << endl;
    TestHSMSiblingTransitionSkipsParentExitEntry(); cout << "  PASS TestHSMSiblingTransitionSkipsParentExitEntry" << endl;
    TestHSMExitOrderChildBeforeParent();          cout << "  PASS TestHSMExitOrderChildBeforeParent" << endl;
    TestHSMRoundTrip();                           cout << "  PASS TestHSMRoundTrip" << endl;
    TestHSMPropagateToParent();                   cout << "  PASS TestHSMPropagateToParent" << endl;
    TestHSMGuardBlocks();                         cout << "  PASS TestHSMGuardBlocks" << endl;
    TestHSMGuardAllows();                         cout << "  PASS TestHSMGuardAllows" << endl;
    TestHSMSelfTransition();                      cout << "  PASS TestHSMSelfTransition" << endl;
    TestHSMOnEntrySignalOrder();                  cout << "  PASS TestHSMOnEntrySignalOrder" << endl;
    TestHSMOnExitSignalOrder();                   cout << "  PASS TestHSMOnExitSignalOrder" << endl;
    TestHSMOnTransitionSignal();                  cout << "  PASS TestHSMOnTransitionSignal" << endl;

    // Behavioral tests (AlarmPanel)
    TestAlarmInitialState();                      cout << "  PASS TestAlarmInitialState" << endl;
    TestAlarmArmHome();                           cout << "  PASS TestAlarmArmHome" << endl;
    TestAlarmToggleDoesNotRepeatParentEntryExit(); cout << "  PASS TestAlarmToggleDoesNotRepeatParentEntryExit" << endl;
    TestAlarmDisarmFromArmedHomePropagatesToArmed(); cout << "  PASS TestAlarmDisarmFromArmedHomePropagatesToArmed" << endl;
    TestAlarmDisarmFromArmedAway();               cout << "  PASS TestAlarmDisarmFromArmedAway" << endl;
    TestAlarmTriggerPropagatesFromChild();        cout << "  PASS TestAlarmTriggerPropagatesFromChild" << endl;
    TestAlarmTriggerFromArmedAway();              cout << "  PASS TestAlarmTriggerFromArmedAway" << endl;
    TestAlarmDisarmFromAlarming();                cout << "  PASS TestAlarmDisarmFromAlarming" << endl;
    TestAlarmEventsIgnoredWhenDisarmed();         cout << "  PASS TestAlarmEventsIgnoredWhenDisarmed" << endl;
    TestAlarmTriggerIgnoredInAlarming();          cout << "  PASS TestAlarmTriggerIgnoredInAlarming" << endl;
    TestAlarmRearm();                             cout << "  PASS TestAlarmRearm" << endl;
    TestAlarmFullScenario();                      cout << "  PASS TestAlarmFullScenario" << endl;

    // Async tests
    TestHSMAsyncBasicDispatch();                  cout << "  PASS TestHSMAsyncBasicDispatch" << endl;
    TestHSMAsyncTransitionCount();                cout << "  PASS TestHSMAsyncTransitionCount" << endl;
    TestHSMAsyncEntryExitCounts();                cout << "  PASS TestHSMAsyncEntryExitCounts" << endl;
    TestHSMAsyncConcurrentEventDispatch();        cout << "  PASS TestHSMAsyncConcurrentEventDispatch" << endl;
    TestHSMAsyncAlarmSequence();                  cout << "  PASS TestHSMAsyncAlarmSequence" << endl;

    cout << "All StateMachineHSM tests passed." << endl;
}
