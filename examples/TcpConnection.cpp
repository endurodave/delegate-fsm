#include "TcpConnection.h"
#include <iostream>

using namespace std;

TcpConnection::TcpConnection() :
    StateMachine(ST_MAX_STATES),
    m_threadObj("TcpSMThread")
{
    m_threadObj.CreateThread();
    SetThread(m_threadObj);
}

TcpConnection::~TcpConnection()
{
    m_threadObj.ExitThread();
}

void TcpConnection::ActiveOpen()
{
    BEGIN_TRANSITION_MAP(TcpConnection, ActiveOpen) // - Current State -
        TRANSITION_MAP_ENTRY(ST_SYN_SENT)           // ST_CLOSED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_LISTEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_SENT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_RCVD
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_ESTABLISHED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_1
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_2
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSE_WAIT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_LAST_ACK
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_TIME_WAIT
    END_TRANSITION_MAP(nullptr)
}

void TcpConnection::PassiveOpen()
{
    BEGIN_TRANSITION_MAP(TcpConnection, PassiveOpen) // - Current State -
        TRANSITION_MAP_ENTRY(ST_LISTEN)             // ST_CLOSED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_LISTEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_SENT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_RCVD
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_ESTABLISHED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_1
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_2
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSE_WAIT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_LAST_ACK
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_TIME_WAIT
    END_TRANSITION_MAP(nullptr)
}

void TcpConnection::Send()
{
    BEGIN_TRANSITION_MAP(TcpConnection, Send)        // - Current State -
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_CLOSED
        TRANSITION_MAP_ENTRY(ST_SYN_SENT)           // ST_LISTEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_SENT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_SYN_RCVD
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_ESTABLISHED
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_FIN_WAIT_1
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_FIN_WAIT_2
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_CLOSE_WAIT
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_CLOSING
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_LAST_ACK
        TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)         // ST_TIME_WAIT
    END_TRANSITION_MAP(nullptr)
}

void TcpConnection::Close()
{
    BEGIN_TRANSITION_MAP(TcpConnection, Close)       // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSED
        TRANSITION_MAP_ENTRY(ST_CLOSED)             // ST_LISTEN
        TRANSITION_MAP_ENTRY(ST_CLOSED)             // ST_SYN_SENT
        TRANSITION_MAP_ENTRY(ST_FIN_WAIT_1)         // ST_SYN_RCVD
        TRANSITION_MAP_ENTRY(ST_FIN_WAIT_1)         // ST_ESTABLISHED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_1
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_FIN_WAIT_2
        TRANSITION_MAP_ENTRY(ST_LAST_ACK)           // ST_CLOSE_WAIT
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_CLOSING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_LAST_ACK
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)         // ST_TIME_WAIT
    END_TRANSITION_MAP(nullptr)
}

void TcpConnection::HandlePacket(std::shared_ptr<const TcpData> data)
{
    if (!IsOnStateMachineThread()) {
        dmq::MakeDelegate(this, &TcpConnection::HandlePacket, *GetThread())(data);
        return;
    }

    uint8_t nextState = EVENT_IGNORED;

    switch (GetCurrentState())
    {
    case ST_LISTEN:
        if (data->syn) nextState = ST_SYN_RCVD;
        break;
    case ST_SYN_SENT:
        if (data->syn && data->ack) nextState = ST_ESTABLISHED;
        else if (data->syn) nextState = ST_SYN_RCVD;
        break;
    case ST_SYN_RCVD:
        if (data->ack) nextState = ST_ESTABLISHED;
        break;
    case ST_ESTABLISHED:
        if (data->fin) nextState = ST_CLOSE_WAIT;
        break;
    case ST_FIN_WAIT_1:
        if (data->fin && data->ack) nextState = ST_TIME_WAIT;
        else if (data->fin) nextState = ST_CLOSING;
        else if (data->ack) nextState = ST_FIN_WAIT_2;
        break;
    case ST_FIN_WAIT_2:
        if (data->fin) nextState = ST_TIME_WAIT;
        break;
    case ST_CLOSING:
        if (data->ack) nextState = ST_TIME_WAIT;
        break;
    case ST_LAST_ACK:
        if (data->ack) nextState = ST_CLOSED;
        break;
    }

    ExternalEvent(nextState, data);
}

STATE_DEFINE(TcpConnection, Closed, NoEventData) { cout << "TcpConnection::ST_Closed" << endl; }
STATE_DEFINE(TcpConnection, Listen, NoEventData) { cout << "TcpConnection::ST_Listen" << endl; }
STATE_DEFINE(TcpConnection, SynSent, NoEventData) { cout << "TcpConnection::ST_SynSent" << endl; }
STATE_DEFINE(TcpConnection, SynRcvd, TcpData) { cout << "TcpConnection::ST_SynRcvd" << endl; }
STATE_DEFINE(TcpConnection, Established, NoEventData) { cout << "TcpConnection::ST_Established" << endl; }
STATE_DEFINE(TcpConnection, FinWait1, NoEventData) { cout << "TcpConnection::ST_FinWait1" << endl; }
STATE_DEFINE(TcpConnection, FinWait2, NoEventData) { cout << "TcpConnection::ST_FinWait2" << endl; }
STATE_DEFINE(TcpConnection, CloseWait, NoEventData) { cout << "TcpConnection::ST_CloseWait" << endl; }
STATE_DEFINE(TcpConnection, Closing, NoEventData) { cout << "TcpConnection::ST_Closing" << endl; }
STATE_DEFINE(TcpConnection, LastAck, NoEventData) { cout << "TcpConnection::ST_LastAck" << endl; }
STATE_DEFINE(TcpConnection, TimeWait, NoEventData) 
{ 
    cout << "TcpConnection::ST_TimeWait" << endl; 
    InternalEvent(ST_CLOSED);
}

GUARD_DEFINE(TcpConnection, IsPortAvailable, NoEventData)
{
    cout << "TcpConnection::GD_IsPortAvailable" << endl;
    return true;
}
