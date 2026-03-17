#ifndef _TCP_CONNECTION_H
#define _TCP_CONNECTION_H

#include "state-machine/StateMachine.h"
#include <string>

/// @brief Event data for TCP transitions. 
struct TcpData : public EventData
{
    bool syn = false;
    bool ack = false;
    bool fin = false;
    bool rst = false;
};

/// @brief TcpConnection implements a subset of the RFC 793 TCP state machine.
/// Demonstrates complex transition maps, guards, and async active-object mode.
class TcpConnection : public StateMachine
{
public:
    TcpConnection();
    ~TcpConnection();

    // External events
    void ActiveOpen();
    void PassiveOpen();
    void Send();
    void Close();
    void HandlePacket(std::shared_ptr<const TcpData> data);

private:
    Thread m_threadObj;

    enum States
    {
        ST_CLOSED,
        ST_LISTEN,
        ST_SYN_SENT,
        ST_SYN_RCVD,
        ST_ESTABLISHED,
        ST_FIN_WAIT_1,
        ST_FIN_WAIT_2,
        ST_CLOSE_WAIT,
        ST_CLOSING,
        ST_LAST_ACK,
        ST_TIME_WAIT,
        ST_MAX_STATES
    };

    // State functions
    STATE_DECLARE(TcpConnection, Closed, NoEventData)
    STATE_DECLARE(TcpConnection, Listen, NoEventData)
    STATE_DECLARE(TcpConnection, SynSent, NoEventData)
    STATE_DECLARE(TcpConnection, SynRcvd, TcpData)
    STATE_DECLARE(TcpConnection, Established, NoEventData)
    STATE_DECLARE(TcpConnection, FinWait1, NoEventData)
    STATE_DECLARE(TcpConnection, FinWait2, NoEventData)
    STATE_DECLARE(TcpConnection, CloseWait, NoEventData)
    STATE_DECLARE(TcpConnection, Closing, NoEventData)
    STATE_DECLARE(TcpConnection, LastAck, NoEventData)
    STATE_DECLARE(TcpConnection, TimeWait, NoEventData)

    // Guard
    GUARD_DECLARE(TcpConnection, IsPortAvailable, NoEventData)

    // State map
    BEGIN_STATE_MAP_EX
        STATE_MAP_ENTRY_EX(&Closed)
        STATE_MAP_ENTRY_EX(&Listen)
        STATE_MAP_ENTRY_EX(&SynSent)
        STATE_MAP_ENTRY_EX(&SynRcvd)
        STATE_MAP_ENTRY_EX(&Established)
        STATE_MAP_ENTRY_EX(&FinWait1)
        STATE_MAP_ENTRY_EX(&FinWait2)
        STATE_MAP_ENTRY_EX(&CloseWait)
        STATE_MAP_ENTRY_EX(&Closing)
        STATE_MAP_ENTRY_EX(&LastAck)
        STATE_MAP_ENTRY_EX(&TimeWait)
    END_STATE_MAP_EX
};

#endif
