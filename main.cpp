// main.cpp
// Example driver exercising the delegate-based FSM framework.
// Demonstrates synchronous, asynchronous active-object, and polling patterns.
// @see https://github.com/endurodave/StateMachine
// David Lafreniere

#include "examples/Motor.h"
#include "examples/Player.h"
#include "examples/CentrifugeTest.h"
#include "examples/TcpConnection.h"
#include "unit-tests/StateMachineTests.h"
#include "delegate-mq/DelegateMQ.h"
#include "delegate-mq/predef/util/Fault.h"
#include "delegate-mq/predef/util/Timer.h"
#include <iostream>
#include <atomic>
#include <future>
#include <thread>
#include <chrono>

using namespace std;
using namespace dmq;

std::atomic<bool> processTimerExit = false;

void ProcessTimers()
{
    while (!processTimerExit.load())
    {
        // Process all delegate-based timers
        Timer::ProcessTimers();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int main()
{
    std::thread timerThread(ProcessTimers);
    // -----------------------------------------------------------------------
    // Synchronous Motor — ExternalEvent runs on the caller's thread.
    // SetSpeed/Halt block until the state action completes before returning.
    // -----------------------------------------------------------------------
    cout << "=== Synchronous Motor ===" << endl;

    Motor syncMotor;

    printf("Size of StateMapRowEx: %zu bytes\n", sizeof(StateMapRowEx));
    printf("Size of Motor: %zu bytes\n", sizeof(Motor));

    // OnTransition fires after every completed state action.
    // fromState == toState indicates a self-transition.
    auto syncConn = syncMotor.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [](uint8_t from, uint8_t to) {
                cout << "  [transition " << (int)from << " -> " << (int)to << "]" << endl;
            })));

    // OnEntry/OnExit fire on every actual state change — not on self-transitions.
    // OnExit fires before the new state's entry action; OnEntry before its action.
    auto entryConn = syncMotor.OnEntry.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [](uint8_t state) {
                cout << "  [entry " << (int)state << "]" << endl;
            })));

    auto exitConn = syncMotor.OnExit.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [](uint8_t state) {
                cout << "  [exit " << (int)state << "]" << endl;
            })));

    // OnCannotHappen fires before FaultHandler (which calls abort) when a
    // CANNOT_HAPPEN transition is taken. Use this to flush logs, record the
    // offending state, or trigger a controlled shutdown before the process dies.
    auto faultConn = syncMotor.OnCannotHappen.Connect(
        MakeDelegate(std::function<void(uint8_t)>(
            [](uint8_t state) {
                cerr << "  [CANNOT_HAPPEN from state " << (int)state << "]" << endl;
            })));

    auto d1 = xmake_shared<MotorData>(); d1->speed = 100;
    syncMotor.SetSpeed(d1);   // blocks — state executes before returning

    auto d2 = xmake_shared<MotorData>(); d2->speed = 200;
    syncMotor.SetSpeed(d2);

    syncMotor.Halt();
    syncMotor.Halt();   // ignored — motor is already idle

    // -----------------------------------------------------------------------
    // Asynchronous Motor — active-object pattern.
    // SetSpeed/Halt post to the SM thread and return immediately.
    // All state logic, guards, entry/exit, and signals fire on that thread.
    // No mutex needed inside the SM — structural thread safety via dispatch.
    // -----------------------------------------------------------------------
    cout << "\n=== Asynchronous Motor (Active Object) ===" << endl;

    Thread smThread("MotorSMThread");
    smThread.CreateThread();

    Motor asyncMotor;
    asyncMotor.SetThread(smThread);

    auto asyncConn = asyncMotor.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [](uint8_t from, uint8_t to) {
                // Fires on smThread — output may interleave with main thread cout
                cout << "  [async transition " << (int)from << " -> " << (int)to << "]" << endl;
            })));

    auto a1 = xmake_shared<MotorData>(); a1->speed = 100;
    cout << "Posting SetSpeed(100)..." << endl;
    asyncMotor.SetSpeed(a1);   // returns immediately; SM thread processes asynchronously

    auto a2 = xmake_shared<MotorData>(); a2->speed = 200;
    cout << "Posting SetSpeed(200)..." << endl;
    asyncMotor.SetSpeed(a2);

    cout << "Posting Halt()..." << endl;
    asyncMotor.Halt();

    cout << "Posting Halt() again (will be ignored)..." << endl;
    asyncMotor.Halt();

    // Drain the queue and stop the SM thread before asyncMotor goes out of scope.
    // Without this the SM thread may still be processing events while the Motor
    // destructor runs, causing a use-after-free.
    smThread.ExitThread();

    // -----------------------------------------------------------------------
    // Player and CentrifugeTest (synchronous)
    // -----------------------------------------------------------------------
    cout << "\n=== Player ===" << endl;

    Player player;
    player.OpenClose();
    player.OpenClose();
    player.Play();
    player.Pause();
    player.EndPause();
    player.Stop();
    player.Play();
    player.Play();
    player.OpenClose();

    cout << "\n=== CentrifugeTest ===" << endl;

    CentrifugeTest test;

    std::promise<void> testDone;
    auto testDoneConn = test.OnComplete.Connect(
        MakeDelegate(std::function<void()>([&testDone]() {
            testDone.set_value();
        })));

    test.Cancel();
    test.Start();
    testDone.get_future().get();   // blocks until OnComplete fires on SM thread

    // -----------------------------------------------------------------------
    // TcpConnection (asynchronous)
    // -----------------------------------------------------------------------
    cout << "\n=== TcpConnection (Active Object) ===" << endl;

    TcpConnection tcp;
    
    auto tcpConn = tcp.OnTransition.Connect(
        MakeDelegate(std::function<void(uint8_t, uint8_t)>(
            [](uint8_t from, uint8_t to) {
                cout << "  [TCP transition " << (int)from << " -> " << (int)to << "]" << endl;
            })));

    cout << "Initiating Active Open..." << endl;
    tcp.ActiveOpen();

    // Simulate network packets
    auto synAck = xmake_shared<TcpData>(); synAck->syn = true; synAck->ack = true;
    cout << "Receiving SYN+ACK packet..." << endl;
    tcp.HandlePacket(synAck);

    cout << "Initiating Close..." << endl;
    tcp.Close();

    auto fin = xmake_shared<TcpData>(); fin->fin = true;
    cout << "Receiving FIN packet..." << endl;
    tcp.HandlePacket(fin);

    auto ack = xmake_shared<TcpData>(); ack->ack = true;
    cout << "Receiving ACK packet..." << endl;
    tcp.HandlePacket(ack);

    // Give some time for async transitions to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    processTimerExit = true;
    timerThread.join();

    RunStateMachineTests();

    return 0;
}
