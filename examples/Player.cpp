#include "Player.h"
#include <iostream>

using namespace std;

Player::Player() :
    StateMachine(ST_MAX_STATES)
{
}

void Player::OpenClose()
{
    BEGIN_TRANSITION_MAP(Player, OpenClose)     // - Current State -
        TRANSITION_MAP_ENTRY(ST_OPEN)           // ST_EMPTY
        TRANSITION_MAP_ENTRY(ST_EMPTY)          // ST_OPEN
        TRANSITION_MAP_ENTRY(ST_OPEN)           // ST_STOPPED
        TRANSITION_MAP_ENTRY(ST_OPEN)           // ST_PAUSED
        TRANSITION_MAP_ENTRY(ST_OPEN)           // ST_PLAYING
    END_TRANSITION_MAP(nullptr)
}

void Player::Play()
{
    BEGIN_TRANSITION_MAP(Player, Play)          // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_EMPTY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_OPEN
        TRANSITION_MAP_ENTRY(ST_PLAYING)        // ST_STOPPED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_PAUSED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_PLAYING
    END_TRANSITION_MAP(nullptr)
}

void Player::Stop()
{
    BEGIN_TRANSITION_MAP(Player, Stop)          // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_EMPTY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_OPEN
        TRANSITION_MAP_ENTRY(ST_STOPPED)        // ST_STOPPED
        TRANSITION_MAP_ENTRY(ST_STOPPED)        // ST_PAUSED
        TRANSITION_MAP_ENTRY(ST_STOPPED)        // ST_PLAYING
    END_TRANSITION_MAP(nullptr)
}

void Player::Pause()
{
    BEGIN_TRANSITION_MAP(Player, Pause)         // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_EMPTY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_OPEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_STOPPED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_PAUSED
        TRANSITION_MAP_ENTRY(ST_PAUSED)         // ST_PLAYING
    END_TRANSITION_MAP(nullptr)
}

void Player::EndPause()
{
    BEGIN_TRANSITION_MAP(Player, EndPause)      // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_EMPTY
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_OPEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_STOPPED
        TRANSITION_MAP_ENTRY(ST_PLAYING)        // ST_PAUSED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)     // ST_PLAYING
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(Player, Empty, NoEventData)
{
    static bool CD_DetectedToggle = false;
    CD_DetectedToggle = !CD_DetectedToggle;

    cout << "Player::ST_Empty" << endl;
    if (CD_DetectedToggle)
        InternalEvent(ST_STOPPED);
}

STATE_DEFINE(Player, Open, NoEventData)
{
    cout << "Player::ST_Open" << endl;
}

STATE_DEFINE(Player, Stopped, NoEventData)
{
    cout << "Player::ST_Stopped" << endl;
}

STATE_DEFINE(Player, Paused, NoEventData)
{
    cout << "Player::ST_Paused" << endl;
}

STATE_DEFINE(Player, Playing, NoEventData)
{
    cout << "Player::ST_Playing" << endl;
}
