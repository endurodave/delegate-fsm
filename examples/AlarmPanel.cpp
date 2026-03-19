#include "AlarmPanel.h"
#include <iostream>

using namespace std;

AlarmPanel::AlarmPanel() :
    StateMachineHSM(ST_MAX_STATES, ST_DISARMED)
{
}

// ---------------------------------------------------------------------------
// External events
// ---------------------------------------------------------------------------

void AlarmPanel::ArmHome()
{
    BEGIN_TRANSITION_MAP(AlarmPanel, ArmHome)           // - Current State -
        TRANSITION_MAP_ENTRY(ST_ARMED_HOME)             // ST_DISARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED_HOME
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED_AWAY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ALARMING
    END_TRANSITION_MAP_HSM(nullptr)
}

void AlarmPanel::ArmAway()
{
    BEGIN_TRANSITION_MAP(AlarmPanel, ArmAway)           // - Current State -
        TRANSITION_MAP_ENTRY(ST_ARMED_AWAY)             // ST_DISARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED_HOME
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED_AWAY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ALARMING
    END_TRANSITION_MAP_HSM(nullptr)
}

void AlarmPanel::Disarm()
{
    BEGIN_TRANSITION_MAP(AlarmPanel, Disarm)            // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_DISARMED
        TRANSITION_MAP_ENTRY(ST_DISARMED)               // ST_ARMED  <-- parent handles it
        TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT)       // ST_ARMED_HOME  --> ST_ARMED
        TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT)       // ST_ARMED_AWAY  --> ST_ARMED
        TRANSITION_MAP_ENTRY(ST_DISARMED)               // ST_ALARMING
    END_TRANSITION_MAP_HSM(nullptr)
}

void AlarmPanel::Toggle()
{
    BEGIN_TRANSITION_MAP(AlarmPanel, Toggle)            // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_DISARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ARMED
        TRANSITION_MAP_ENTRY(ST_ARMED_AWAY)             // ST_ARMED_HOME
        TRANSITION_MAP_ENTRY(ST_ARMED_HOME)             // ST_ARMED_AWAY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ALARMING
    END_TRANSITION_MAP_HSM(nullptr)
}

void AlarmPanel::Trigger(std::shared_ptr<const TriggerData> data)
{
    BEGIN_TRANSITION_MAP(AlarmPanel, Trigger, data)     // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_DISARMED
        TRANSITION_MAP_ENTRY(ST_ALARMING)               // ST_ARMED  <-- parent handles it
        TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT)       // ST_ARMED_HOME  --> ST_ARMED
        TRANSITION_MAP_ENTRY(PROPAGATE_TO_PARENT)       // ST_ARMED_AWAY  --> ST_ARMED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)             // ST_ALARMING (already sounding)
    END_TRANSITION_MAP_HSM(data)
}

// ---------------------------------------------------------------------------
// State actions
// ---------------------------------------------------------------------------

STATE_DEFINE(AlarmPanel, Disarmed, NoEventData)
{
    cout << "AlarmPanel: system disarmed" << endl;
}

// ST_ARMED is a composite parent — its state action only runs if explicitly
// targeted, which does not happen in normal operation.
STATE_DEFINE(AlarmPanel, Armed, NoEventData)
{
    cout << "AlarmPanel: armed (composite parent state)" << endl;
}

STATE_DEFINE(AlarmPanel, ArmedHome, NoEventData)
{
    cout << "AlarmPanel: armed HOME - perimeter sensors active" << endl;
}

STATE_DEFINE(AlarmPanel, ArmedAway, NoEventData)
{
    cout << "AlarmPanel: armed AWAY - all sensors active" << endl;
}

STATE_DEFINE(AlarmPanel, Alarming, TriggerData)
{
    cout << "AlarmPanel: *** ALARM *** zone " << data->zone
         << " - contacting emergency services" << endl;
}

// ---------------------------------------------------------------------------
// Entry / exit actions
// ---------------------------------------------------------------------------

// Fires when entering the ARMED composite state from any path — whether
// going to ARMED_HOME or ARMED_AWAY. The single place to activate hardware.
ENTRY_DEFINE(AlarmPanel, EntryArmed, NoEventData)
{
    cout << "AlarmPanel: [entry ARMED] activating sensors" << endl;
}

// Fires when leaving the ARMED composite state from any path — whether
// Disarm or Trigger triggered the exit.
EXIT_DEFINE(AlarmPanel, ExitArmed)
{
    cout << "AlarmPanel: [exit ARMED] deactivating sensors" << endl;
}

ENTRY_DEFINE(AlarmPanel, EntryArmedHome, NoEventData)
{
    cout << "AlarmPanel: [entry ARMED_HOME] interior motion disabled" << endl;
}

ENTRY_DEFINE(AlarmPanel, EntryArmedAway, NoEventData)
{
    cout << "AlarmPanel: [entry ARMED_AWAY] full coverage enabled" << endl;
}
