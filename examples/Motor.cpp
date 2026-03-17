#include "Motor.h"
#include <iostream>

using namespace std;

Motor::Motor() :
    StateMachine(ST_MAX_STATES),
    m_currentSpeed(0)
{
}

void Motor::SetSpeed(std::shared_ptr<const MotorData> data)
{
    BEGIN_TRANSITION_MAP(Motor, SetSpeed, data)     // - Current State -
        TRANSITION_MAP_ENTRY(ST_START)              // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_STOP
        TRANSITION_MAP_ENTRY(ST_CHANGE_SPEED)       // ST_START
        TRANSITION_MAP_ENTRY(ST_CHANGE_SPEED)       // ST_CHANGE_SPEED
    END_TRANSITION_MAP(data)
}

void Motor::Halt()
{
    BEGIN_TRANSITION_MAP(Motor, Halt)               // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_IDLE
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_STOP
        TRANSITION_MAP_ENTRY(ST_STOP)               // ST_START
        TRANSITION_MAP_ENTRY(ST_STOP)               // ST_CHANGE_SPEED
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(Motor, Idle, NoEventData)
{
    cout << "Motor::ST_Idle" << endl;
}

STATE_DEFINE(Motor, Stop, NoEventData)
{
    cout << "Motor::ST_Stop" << endl;
    m_currentSpeed = 0;
    InternalEvent(ST_IDLE);
}

STATE_DEFINE(Motor, Start, MotorData)
{
    cout << "Motor::ST_Start : Speed is " << data->speed << endl;
    m_currentSpeed = data->speed;
}

STATE_DEFINE(Motor, ChangeSpeed, MotorData)
{
    cout << "Motor::ST_ChangeSpeed : Speed is " << data->speed << endl;
    m_currentSpeed = data->speed;
}
