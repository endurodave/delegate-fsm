![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/StateMachine/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/StateMachine/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/StateMachine/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/StateMachine/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/StateMachine/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/StateMachine/actions/workflows/cmake_windows.yml)

# State Machine Design in C++

A compact, table-driven C++ finite state machine (FSM) with asynchronous active-object and signal-slot event notification. Runs on embedded and PC targets. 

# Table of Contents

- [State Machine Design in C++](#state-machine-design-in-c)
- [Table of Contents](#table-of-contents)
- [Preface](#preface)
  - [Related repositories](#related-repositories)
- [Getting Started](#getting-started)
- [Introduction](#introduction)
  - [Background](#background)
  - [Why use a state machine?](#why-use-a-state-machine)
- [State machine design](#state-machine-design)
  - [Internal and external events](#internal-and-external-events)
  - [Event data](#event-data)
  - [State transitions](#state-transitions)
- [StateMachine class](#statemachine-class)
- [Motor example](#motor-example)
  - [State functions](#state-functions)
  - [State map](#state-map)
  - [Transition map](#transition-map)
- [State engine](#state-engine)
- [Generating events](#generating-events)
- [Signals](#signals)
- [Asynchronous active-object mode](#asynchronous-active-object-mode)
- [State machine inheritance](#state-machine-inheritance)
- [State function inheritance](#state-function-inheritance)
- [Multithread safety](#multithread-safety)
- [Fixed-block allocator](#fixed-block-allocator)
- [Comparison with other libraries](#comparison-with-other-libraries)
    - [Boost.MSM](#boostmsm)
    - [Boost.Statechart](#booststatechart)
    - [SML (kgrzybek)](#sml-kgrzybek)
    - [TinyFSM](#tinyfsm)
    - [QP/C++](#qpc)
    - [Summary](#summary)


# Preface

Originally published on CodeProject at: [**State Machine Design in C++**](https://www.codeproject.com/Articles/1087619/State-Machine-Design-in-Cplusplus)

Based on original design published in C\C++ Users Journal (Dr. Dobb's) at: [**State Machine Design in C++**](http://www.drdobbs.com/cpp/state-machine-design-in-c/184401236)

## Related repositories

The following repositories offer various implementations of finite state machines, ranging from compact single-threaded versions to advanced asynchronous frameworks utilizing the Signal-Slot pattern.

| Project | Description |
| :--- | :--- |
| [**State Machine Design in C**](https://github.com/endurodave/C_StateMachine) | A compact C language finite state machine (FSM) implementation. |
| [**DelegateMQ**](https://github.com/endurodave/DelegateMQ) | The asynchronous delegate library used by this project for active-object dispatch, type-safe multicast delegates, and pub/sub signals. |


# Getting Started

[CMake](https://cmake.org/) is used to create the project build files on any Windows or Linux machine. The state machine source code works on any C++ compiler on any platform.

```bash
# Build
cmake -B build .

# Run
./build/StateMachineApp          # Linux
build\Debug\StateMachineApp.exe  # Windows
```

`StateMachineApp` runs all example state machines and unit tests on launch and outputs results to stdout.

# Introduction

In 2000, I wrote an article entitled "*State Machine Design in C++*" for C/C++ Users Journal (R.I.P.). Interestingly, that old article is still available and (at the time of writing this article) the #1 hit on Google when searching for C++ state machine. The article was written over 25 years ago, but I continue to use the basic idea on numerous projects. It's compact, easy to understand and, in most cases, has just enough features to accomplish what I need.

This implementation updates the classic design with modern DelegateMQ features. It uses a compact table-driven core that makes the original design so practical for embedded and PC targets alike, while adding publisher/subscriber signals and an optional asynchronous active-object mode.

This state machine has the following features:

1. **Memory efficient** – uses static transition and state maps to minimize RAM/heap usage.
2. **Transition tables** – transition tables precisely control state transition behavior.
3. **Events** – events are public member functions that trigger state transitions.
4. **State action** – every state action is a separate member function.
5. **Guards/entry/exit actions** – optionally a state machine can use guard conditions and separate entry/exit action functions for each state.
6. **Signals** – `OnTransition`, `OnEntry`, `OnExit`, and `OnCannotHappen` pub/sub signals allow external observers to react to state changes.
7. **Async active-object mode** – calling `SetThread()` enables active-object dispatch; `ExternalEvent()` marshals to the SM thread and returns immediately.
8. **State machine inheritance** – supports inheriting states from a base state machine class.
9. **Type safe** – compile-time checks via templates and macros catch signature mismatches.

## Background

A common design technique in the repertoire of most programmers is the venerable finite state machine (FSM). Designers use this programming construct to break complex problems into manageable states and state transitions. There are innumerable ways to implement a state machine.

A switch statement provides one of the easiest to implement and most common version of a state machine. Here, each case within the switch statement becomes a state, implemented something like:

```cpp
switch (currentState) {
   case ST_IDLE:
       // do something in the idle state
       break;
    case ST_STOP:
       // do something in the stop state
       break;
    // etc...
}
```

This method is certainly appropriate for solving many different design problems. When employed on an event driven, multithreaded project, however, state machines of this form can be quite limiting.

The first problem revolves around controlling what state transitions are valid and which ones are invalid. There is no way to enforce the state transition rules. Any transition is allowed at any time, which is not particularly desirable. For most designs, only a few transition patterns are valid. Ideally, the software design should enforce these predefined state sequences and prevent the unwanted transitions. Another problem arises when trying to send data to a specific state. Since the entire state machine is located within a single function, sending additional data to any given state proves difficult. And lastly these designs are rarely suitable for use in a multithreaded system. The designer must ensure the state machine is called from a single thread of control.

## Why use a state machine?

Implementing code using a state machine is an extremely handy design technique for solving complex engineering problems. State machines break down the design into a series of steps, or what are called states in state-machine lingo. Each state performs some narrowly defined task. Events, on the other hand, are the stimuli, which cause the state machine to move, or transition, between states.

To take a simple example, which I will use throughout this article, let's say we are designing motor-control software. We want to start and stop the motor, as well as change the motor's speed. Simple enough. The motor control events to be exposed to the client software will be as follows:

1. **Set Speed** – sets the motor going at a specific speed.
2. **Halt** – stops the motor.

These events provide the ability to start the motor at whatever speed desired, which also implies changing the speed of an already moving motor. Or we can stop the motor altogether. To the motor-control class, these two events, or functions, are considered external events. To a client using our code, however, these are just plain functions within a class.

These events are not state machine states. The steps required to handle these two events are different. In this case the states are:

1. **Idle** — the motor is not spinning but is at rest.
2. **Start** — starts the motor from a dead stop.
3. **Change Speed** — adjust the speed of an already moving motor.
4. **Stop** — stop a moving motor.

As can be seen, breaking the motor control into discreet states, as opposed to having one monolithic function, we can more easily manage the rules of how to operate the motor.

Every state machine has the concept of a "current state." This is the state the state machine currently occupies. At any given moment in time, the state machine can be in only a single state. Every instance of a particular state machine class can set the initial state during construction. That initial state, however, does not execute during object creation. Only an event sent to the state machine causes a state function to execute.

To graphically illustrate the states and events, we use a state diagram. Figure 1 below shows the state transitions for the motor control class. A box denotes a state and a connecting arrow indicates the event transitions. Arrows with the event name listed are external events, whereas unadorned lines are considered internal events.

![](Motor.png)

**Figure 1: Motor state diagram**

In short, using a state machine captures and enforces complex interactions, which might otherwise be difficult to convey and implement.

# State machine design

## Internal and external events

As I mentioned earlier, an event is the stimulus that causes a state machine to transition between states. Events can be broken out into two categories: external and internal. The external event, at its most basic level, is a function call into a state-machine object. Any thread or task within a system can generate an external event. In synchronous mode (the default), the external event function call causes state execution on the caller's thread of control. In asynchronous active-object mode (see below), `ExternalEvent()` marshals the call to the designated SM thread and returns immediately. An internal event, on the other hand, is self-generated by the state machine itself during state execution and always runs synchronously on the SM thread.

Once the external event starts the state machine executing, it cannot be interrupted by another external event until the external event and all internal events have completed execution. In active-object mode this is guaranteed structurally — all events are serialized through the SM thread's message queue.

## Event data

When an event is generated, it can optionally attach event data to be used by the state function during execution. All event data structures must inherit from the `EventData` base class:

```cpp
class EventData {
public:
    virtual ~EventData() = default;
};
using NoEventData = EventData;
```

The state machine takes ownership of the `EventData` pointer and deletes it after the transition is processed. (Note: if the fixed-block allocator is enabled, `operator new` and `delete` will use the pool allocator). All event data passed to `ExternalEvent()` must be created on the heap using `operator new`.

## State transitions

When an external event is generated, a lookup is performed to determine the state transition course of action. There are three possible outcomes to an event: new state, event ignored, or cannot happen. A new state causes a transition to a new state where it is allowed to execute. For an ignored event, no state executes. The last possibility, cannot happen, is reserved for situations where the event is not valid given the current state of the state machine. If this occurs, the `OnCannotHappen` signal fires and the software faults.

# StateMachine class

Inherit from `StateMachine` to create a new state machine. Use macros to declare and define states.

```cpp
class StateMachine {
public:
    enum { EVENT_IGNORED = 0xFE, CANNOT_HAPPEN = 0xFF };

    StateMachine(uint8_t maxStates, uint8_t initialState = 0);
    virtual ~StateMachine() = default;

    uint8_t GetCurrentState() const;
    uint8_t GetMaxStates() const;

    // Enable active-object async mode. Call before the first ExternalEvent.
    void SetThread(dmq::IThread& thread);

    // Signals — fire on the SM thread in async mode.
    dmq::Signal<void(uint8_t fromState, uint8_t toState)> OnTransition;
    dmq::Signal<void(uint8_t state)> OnEntry;
    dmq::Signal<void(uint8_t state)> OnExit;
    dmq::Signal<void(uint8_t state)> OnCannotHappen;

protected:
    void ExternalEvent(uint8_t newState, const EventData* pData = nullptr);
    void InternalEvent(uint8_t newState, const EventData* pData = nullptr);
};
```

# Motor example

## State functions

State functions implement each state — one state function per state-machine state. All state functions follow the signature `void ST_Name(const DataType*)`.

```cpp
STATE_DEFINE(Motor, Start, MotorData)
{
    cout << "Motor::ST_Start : Speed is " << data->speed << endl;
    m_currentSpeed = data->speed;
}
```

## State map

The state map links state enums to state functions, guards, entry, and exit actions.

```cpp
BEGIN_STATE_MAP_EX
    STATE_MAP_ENTRY_ALL_EX(&Start, &GuardStart, &EntryStart, &ExitStart)
END_STATE_MAP_EX
```

## Transition map

A transition map is defined in each external event function:

```cpp
void Motor::Halt()
{
    BEGIN_TRANSITION_MAP
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_STOP)        // ST_START
        ...
    END_TRANSITION_MAP(nullptr)
}
```

# State engine

The state engine executes state functions based upon events generated. `StateEngine()` runs the following sequence in a loop until no more internal events are pending:

1. Look up the `StateMapRow` for the new state.
2. If a guard condition exists, evaluate it. If it returns `false`, the transition is vetoed.
3. If transitioning to a different state, call the current state's exit action, fire `OnExit`, update `m_currentState`, call the new state's entry action, and fire `OnEntry`.
4. Call the state action function.
5. Fire `OnTransition(fromState, toState)`. 
6. Check whether the action generated an internal event. If so, repeat from step 1.

# Generating events

External events are generated by creating event data on the heap, populating its fields, and calling the external event function.

```cpp
auto data = new MotorData();
data->speed = 100;
motor.SetSpeed(data);
```

# Signals

`StateMachine` provides signals to observe its behavior:

- **OnTransition(fromState, toState)** — fires after every completed state action.
- **OnEntry(state)** — fires when entering a new state.
- **OnExit(state)** — fires when exiting a state.
- **OnCannotHappen(state)** — fires when a `CANNOT_HAPPEN` transition is triggered.

```cpp
motor.OnTransition.Connect(MakeDelegate(
    [](uint8_t from, uint8_t to) {
        cout << "Transitioned from " << (int)from << " to " << (int)to << endl;
    }));
```

# Asynchronous active-object mode

Call `SetThread()` to enable async dispatch. All `ExternalEvent()` calls will then marshal to the specified thread.

```cpp
Thread smThread("MotorSMThread");
smThread.CreateThread();

motor.SetThread(smThread);
motor.SetSpeed(new MotorData(100)); // returns immediately
```

# State machine inheritance

Derived classes can inherit and extend the state machine of a base class. `PARENT_TRANSITION` macro handles events in the base class that should be routed to a derived state.

```cpp
void SelfTest::Cancel()
{
    PARENT_TRANSITION(ST_FAILED)
    ...
}
```

# State function inheritance

State functions can be overridden in the derived class. The derived class may call the base implementation if desired.

```cpp
void CentrifugeTest::ST_Idle(const NoEventData* data)
{
    cout << "CentrifugeTest::ST_Idle" << endl;
    SelfTest::ST_Idle(data);   // call base class Idle
}
```

# Multithread safety

Structural thread safety is provided via active-object dispatch; no explicit locks needed inside the state machine when `SetThread()` is used.

# Fixed-block allocator

This project includes an optional fixed-block pool allocator, `xallocator`. When `DMQ_ALLOCATOR` is enabled, `xmake_shared` allocates memory from pre-sized pools.

# Comparison with other libraries

The table below compares this implementation against several widely used C++ state machine libraries across the features most relevant to embedded and multithreaded applications.

| Feature | This implementation | Boost.MSM | Boost.Statechart | SML (kgrzybek) | TinyFSM | QP/C++ |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Compact binary footprint | ✓ | — | — | ✓ | ✓ | — |
| No external dependencies | — | — | — | ✓ | ✓ | — |
| Embedded-friendly | ✓ | — | — | ✓ | ✓ | ✓ |
| Runtime state registration | ✓ | — | — | — | — | — |
| Typed event data per state | ✓ | ✓ | ✓ | ✓ | — | ✓ |
| Guard conditions | ✓ | ✓ | ✓ | ✓ | — | ✓ |
| Entry / exit actions | ✓ | ✓ | ✓ | ✓ | — | ✓ |
| State machine inheritance | ✓ | — | ✓ | — | — | ✓ |
| Built-in async active-object | ✓ | — | — | — | — | ✓ |
| Pub/sub signals (OnTransition etc.) | ✓ | — | — | — | — | — |
| `shared_ptr` event data (async safe) | — | — | — | — | — | — |
| Validate() at startup | — | — | — | — | — | — |
| C++17 standard only | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| Hierarchical SM (HSM) | — | ✓ | ✓ | — | — | ✓ |
| Compile-time transition checking | partial | ✓ | — | ✓ | ✓ | — |

### Boost.MSM

Boost.MSM is one of the most feature-complete C++ SM libraries available. Transitions are defined entirely at compile time as a table of `Row<>` type entries, and the optimizer can reduce the dispatch overhead to near zero. The tradeoff is significant complexity: template metaprogramming drives the entire design, compile times are long, and error messages are notoriously difficult to interpret. It requires the full Boost installation and is impractical on most embedded targets.

### Boost.Statechart

Boost.Statechart provides a runtime Hierarchical State Machine that closely follows the UML semantics including orthogonal regions and history states. The runtime flexibility comes at a cost: each state is a separate heap-allocated object with virtual dispatch at every transition. The Boost dependency and per-state heap allocation make it unsuitable for constrained embedded systems.

### SML (kgrzybek)

SML is a modern, header-only C++14 library that defines transitions using a concise DSL. It has no heap allocation, no RTTI requirement, and compiles to very tight code. The main limitation is that the entire state machine structure — states, events, guards, and transitions — must be expressed as a compile-time type list. This makes runtime introspection, signals, and dynamic observer attachment impossible without significant extra work. There is also no built-in threading support.

### TinyFSM

TinyFSM is designed specifically for small embedded targets. The entire library is a single header with no dependencies, and dispatch is done through `static` member functions — effectively one state machine instance per type. This makes it very simple and very fast, but it means you cannot have two independent instances of the same state machine class. There is no support for per-transition typed event data, guards, or entry/exit actions in the base design.

### QP/C++

QP/C++ is a full embedded RTOS framework built around hierarchical active objects. It is battle-tested in safety-critical systems and supports a rich UML-compliant HSM with orthogonal regions, deferred events, and publish/subscribe. The active-object model provides the same structural thread safety as `SetThread()` here. The tradeoff is that QP is an entire framework, not a library you drop into an existing project — it brings its own event pool, time-event management, and RTOS abstraction layer. Commercial use may require a license.

### Summary

This implementation occupies the practical middle ground. It is compact and embedded-friendly like TinyFSM and SML, supports guards/entry/exit/inheritance like Boost.MSM and QP/C++, and uniquely adds a built-in async active-object mode and pub/sub signals that none of the others provide out of the box. Runtime delegate registration keeps the syntax straightforward — state functions are plain member functions with no special macros or template parameter packs — while the typed delegate adapters still catch signature errors at compile time. For projects that need a capable FSM without adopting a full framework or wrestling with heavy template metaprogramming, this design hits a practical sweet spot.
