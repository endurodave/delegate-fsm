#ifndef _ALARM_PANEL_H
#define _ALARM_PANEL_H

#include "state-machine/StateMachineHSM.h"

/// @brief Event data for a sensor trigger — carries the zone that fired.
class TriggerData : public EventData
{
public:
    int zone = 0;
};

/// @brief AlarmPanel demonstrates StateMachineHSM with a two-level hierarchy.
///
/// Hierarchy:
///   DISARMED        (top-level)
///   ARMED           (top-level, composite parent)
///     ARMED_HOME    (child of ARMED — perimeter sensors only)
///     ARMED_AWAY    (child of ARMED — all sensors active)
///   ALARMING        (top-level)
///
/// The HSM benefit is visible in the transition maps: ARMED_HOME and ARMED_AWAY
/// both use PROPAGATE_TO_PARENT for Disarm and Trigger. The ARMED parent handles
/// those events once. Entry/exit on ARMED activates/deactivates hardware for
/// both child states without duplicating that logic in each child.
class AlarmPanel : public StateMachineHSM
{
public:
    AlarmPanel();

    // External events
    void ArmHome();
    void ArmAway();
    void Disarm();
    void Toggle();
    void Trigger(std::shared_ptr<const TriggerData> data);

private:
    enum States
    {
        ST_DISARMED,
        ST_ARMED,
        ST_ARMED_HOME,
        ST_ARMED_AWAY,
        ST_ALARMING,
        ST_MAX_STATES
    };

    // State actions
    STATE_DECLARE(AlarmPanel, Disarmed,  NoEventData)
    STATE_DECLARE(AlarmPanel, Armed,     NoEventData)
    STATE_DECLARE(AlarmPanel, ArmedHome, NoEventData)
    STATE_DECLARE(AlarmPanel, ArmedAway, NoEventData)
    STATE_DECLARE(AlarmPanel, Alarming,  TriggerData)

    // Entry/exit on the ARMED composite state — hardware activation point.
    ENTRY_DECLARE(AlarmPanel, EntryArmed,     NoEventData)
    EXIT_DECLARE (AlarmPanel, ExitArmed)

    // Entry on leaf states — announce current mode.
    ENTRY_DECLARE(AlarmPanel, EntryArmedHome, NoEventData)
    ENTRY_DECLARE(AlarmPanel, EntryArmedAway, NoEventData)

    // HSM state map — parent column is what makes this an HSM.
    //                             State        Guard    Entry             Exit         Parent
    BEGIN_STATE_MAP_HSM
        STATE_MAP_ENTRY_ALL_HSM(&Disarmed,  nullptr, nullptr,          nullptr,       NO_PARENT)
        STATE_MAP_ENTRY_ALL_HSM(&Armed,     nullptr, &EntryArmed,      &ExitArmed,    NO_PARENT)
        STATE_MAP_ENTRY_ALL_HSM(&ArmedHome, nullptr, &EntryArmedHome,  nullptr,       ST_ARMED)
        STATE_MAP_ENTRY_ALL_HSM(&ArmedAway, nullptr, &EntryArmedAway,  nullptr,       ST_ARMED)
        STATE_MAP_ENTRY_ALL_HSM(&Alarming,  nullptr, nullptr,          nullptr,       NO_PARENT)
    END_STATE_MAP_HSM
};

#endif
