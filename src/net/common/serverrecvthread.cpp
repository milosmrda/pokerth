/***************************************************************************
 *   Copyright (C) 2007 by Lothar May                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <net/serverrecvthread.h>
#include <net/serverexception.h>
#include <net/serverrecvstate.h>
#include <net/senderthread.h>
#include <net/sendercallback.h>
#include <net/receiverhelper.h>
#include <net/socket_msg.h>
#include <game.h>
#include <tools.h>
#include <localenginefactory.h>

#include <boost/lambda/lambda.hpp>

#define SERVER_CLOSE_SESSION_DELAY_SEC	10

using namespace std;


class ServerSenderCallback : public SenderCallback
{
public:
	ServerSenderCallback(ServerRecvThread &server) : m_server(server) {}
	virtual ~ServerSenderCallback() {}

	virtual void SignalNetError(SOCKET sock, int errorID, int osErrorID)
	{
		// We just ignore send errors for now, on server side.
		// A send error should trigger a read error or a read
		// returning 0 afterwards, and we will handle this error.
	}

private:
	ServerRecvThread &m_server;
};


ServerRecvThread::ServerRecvThread(GuiInterface &gui, ConfigFile *playerConfig)
: m_curGameId(1), m_gui(gui), m_playerConfig(playerConfig)
{
	m_senderCallback.reset(new ServerSenderCallback(*this));
	m_sender.reset(new SenderThread(GetSenderCallback()));
	m_receiver.reset(new ReceiverHelper);
}

ServerRecvThread::~ServerRecvThread()
{
	CleanupConnectQueue();
	CleanupSessionMap();
}

void
ServerRecvThread::Init(const string &pwd, const GameData &gameData)
{
	m_password = pwd;
	m_gameData = gameData;
}

void
ServerRecvThread::AddConnection(boost::shared_ptr<ConnectData> data)
{
	boost::mutex::scoped_lock lock(m_connectQueueMutex);
	m_connectQueue.push_back(data);
}

void
ServerRecvThread::AddNotification(unsigned message, const string &param)
{
	boost::mutex::scoped_lock lock(m_notificationQueueMutex);
	m_notificationQueue.push_back(Notification(message, param));
}

void
ServerRecvThread::Main()
{
	SetState(SERVER_INITIAL_STATE::Instance());
	GetSender().Run();

	try
	{
		while (!ShouldTerminate())
		{
			{
				// Handle one incoming connection at a time.
				boost::shared_ptr<ConnectData> tmpData;
				{
					boost::mutex::scoped_lock lock(m_connectQueueMutex);
					if (!m_connectQueue.empty())
					{
						tmpData = m_connectQueue.front();
						m_connectQueue.pop_front();
					}
				}
				if (tmpData.get())
					GetState().HandleNewConnection(*this, tmpData);
			}
			// Process current state.
			GetState().Process(*this);
			// Process thread-safe notifications.
			NotificationLoop();
			// Close sessions.
			CloseSessionLoop();
		}
	} catch (const NetException &e)
	{
		GetCallback().SignalNetServerError(e.GetErrorId(), e.GetOsErrorCode());
	}
	GetSender().SignalTermination();
	GetSender().Join(SENDER_THREAD_TERMINATE_TIMEOUT);

	CleanupConnectQueue();
	CleanupSessionMap();
}

void
ServerRecvThread::NotificationLoop()
{
	boost::mutex::scoped_lock lock(m_notificationQueueMutex);
	// Process all notifications.
	while (!m_notificationQueue.empty())
	{
		Notification notification = m_notificationQueue.front();
		m_notificationQueue.pop_front();

		switch(notification.message)
		{
			case NOTIFY_GAME_START:
				InternalStartGame();
				break;
			case NOTIFY_KICK_PLAYER:
				InternalKickPlayer(notification.param);
				break;
		}
	}
}

void
ServerRecvThread::CloseSessionLoop()
{
	CloseSessionList::iterator i = m_closeSessionList.begin();
	CloseSessionList::iterator end = m_closeSessionList.end();

	while (i != end)
	{
		CloseSessionList::iterator cur = i++;

		if (cur->first.elapsed().seconds() >= SERVER_CLOSE_SESSION_DELAY_SEC)
			m_closeSessionList.erase(cur);
	}
}

SOCKET
ServerRecvThread::Select()
{
	SOCKET retSock = INVALID_SOCKET;

	SOCKET maxSock = INVALID_SOCKET;
	fd_set rdset;
	FD_ZERO(&rdset);

	{
		boost::mutex::scoped_lock lock(m_sessionMapMutex);
		SocketSessionMap::iterator i = m_sessionMap.begin();
		SocketSessionMap::iterator end = m_sessionMap.end();

		while (i != end)
		{
			SOCKET tmpSock = i->first;
			FD_SET(tmpSock, &rdset);
			if (tmpSock > maxSock || maxSock == INVALID_SOCKET)
				maxSock = tmpSock;
			++i;
		}
	}

	if (maxSock == INVALID_SOCKET)
	{
		Msleep(RECV_TIMEOUT_MSEC); // just sleep if there is no session
	}
	else
	{
		// wait for data
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = RECV_TIMEOUT_MSEC * 1000;
		int selectResult = select(maxSock + 1, &rdset, NULL, NULL, &timeout);
		if (!IS_VALID_SELECT(selectResult))
		{
			throw ServerException(ERR_SOCK_SELECT_FAILED, SOCKET_ERRNO());
		}
		if (selectResult > 0) // one (or more) of the sockets is readable
		{
			// Check which socket is readable, return the first.
			boost::mutex::scoped_lock lock(m_sessionMapMutex);
			SocketSessionMap::iterator i = m_sessionMap.begin();
			SocketSessionMap::iterator end = m_sessionMap.end();

			while (i != end)
			{
				SOCKET tmpSock = i->first;
				if (FD_ISSET(tmpSock, &rdset))
				{
					retSock = tmpSock;
					break;
				}
				++i;
			}
		}
	}
	return retSock;
}

void
ServerRecvThread::CleanupConnectQueue()
{
	boost::mutex::scoped_lock lock(m_connectQueueMutex);

	// Sockets will be closed automatically.
	m_connectQueue.clear();
}

void
ServerRecvThread::CleanupSessionMap()
{
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	// Sockets will be closed automatically.
	m_sessionMap.clear();
}

void
ServerRecvThread::InternalStartGame()
{
	SetState(SERVER_START_GAME_STATE::Instance());

	// Kick all players which are not fully connected.
	RemoveNotEstablishedSessions();

	// Initialize the game.
	GuiInterface &gui = GetGui();
	PlayerDataList playerData = GetPlayerDataList();

	// Create EngineFactory
	boost::shared_ptr<EngineFactory> factory(new LocalEngineFactory(m_playerConfig)); // LocalEngine erstellen

	// Set start data.
	StartData startData;
	startData.numberOfPlayers = playerData.size();

	int tmpDealerPos = 0;
	Tools::getRandNumber(0, startData.numberOfPlayers-1, 1, &tmpDealerPos, 0);
	// The Player Id is not continuous. Therefore, the start dealer position
	// needs to be converted to a player Id, and cannot be directly generated
	// as player Id.
	PlayerDataList::const_iterator player_i = playerData.begin();
	PlayerDataList::const_iterator player_end = playerData.end();

	bool randDealerFound = false;
	while (player_i != player_end)
	{
		if ((*player_i)->GetNumber() == tmpDealerPos)
		{
			// Get ID of the dealer.
			startData.startDealerPlayerId = static_cast<unsigned>((*player_i)->GetUniqueId());
			randDealerFound = true;
			break;
		}
		++player_i;
	}
	assert(randDealerFound); // TODO: Throw exception.

	SetStartData(startData);

	m_game.reset(new Game(&gui, factory, playerData, GetGameData(), GetStartData(), m_curGameId++));
}

void
ServerRecvThread::InternalKickPlayer(const string playerName)
{
	if (!playerName.empty())
	{
		SessionWrapper tmpSession = GetSession(playerName);

		SessionError(tmpSession, ERR_NET_PLAYER_KICKED);
	}
}

SessionWrapper
ServerRecvThread::GetSession(SOCKET sock) const
{
	SessionWrapper tmpSession;
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::const_iterator pos = m_sessionMap.find(sock);
	if (pos != m_sessionMap.end())
	{
		tmpSession = pos->second;
	}
	return tmpSession;
}

SessionWrapper
ServerRecvThread::GetSession(const string playerName) const
{
	SessionWrapper tmpSession;
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::const_iterator session_i = m_sessionMap.begin();
	SocketSessionMap::const_iterator session_end = m_sessionMap.end();

	while (session_i != session_end)
	{
		// Check all players which are fully connected.
		if (session_i->second.sessionData->GetState() == SessionData::Established)
		{
			boost::shared_ptr<PlayerData> tmpPlayer(session_i->second.playerData);
			assert(tmpPlayer.get());
			if (tmpPlayer->GetName() == playerName)
			{
				tmpSession = session_i->second;
				break;
			}
		}

		++session_i;
	}
	return tmpSession;
}

void
ServerRecvThread::AddSession(boost::shared_ptr<SessionData> sessionData)
{
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::iterator pos = m_sessionMap.lower_bound(sessionData->GetSocket());

	// If pos points to a pair whose key is equivalent to the socket, this handle
	// already exists within the list.
	if (pos != m_sessionMap.end() && sessionData->GetSocket() == pos->first)
	{
		throw ServerException(ERR_SOCK_CONN_EXISTS, 0);
	}
	m_sessionMap.insert(pos, SocketSessionMap::value_type(sessionData->GetSocket(), SessionWrapper(sessionData, boost::shared_ptr<PlayerData>())));
}

void
ServerRecvThread::SessionError(SessionWrapper session, int errorCode)
{
	assert(session.sessionData.get());
	SendError(session.sessionData->GetSocket(), errorCode);
	CloseSessionDelayed(session);
}

void
ServerRecvThread::RejectNewConnection(boost::shared_ptr<ConnectData> connData)
{
	// Create a generic session with Id 0.
	boost::shared_ptr<SessionData> sessionData(new SessionData(connData->ReleaseSocket(), 0));
	// Gracefully close this session.
	SessionError(SessionWrapper(sessionData, boost::shared_ptr<PlayerData>()), ERR_NET_GAME_ALREADY_RUNNING);
}

void
ServerRecvThread::CloseSessionDelayed(SessionWrapper session)
{
	{
		boost::mutex::scoped_lock lock(m_sessionMapMutex);

		m_sessionMap.erase(session.sessionData->GetSocket());
	}

	boost::shared_ptr<PlayerData> tmpPlayerData = session.playerData;
	if (tmpPlayerData.get() && !tmpPlayerData->GetName().empty())
	{
		// Set player inactive.
		if (m_game.get())
		{
			PlayerInterface *player = GetGame().getPlayerByUniqueId(tmpPlayerData->GetUniqueId());
			if (player)
			{
				player->setMyAction(PLAYER_ACTION_FOLD);
				player->setMyCash(0);
				player->setMyActiveStatus(false);
			}
		}

		// Send "Player Left" to clients.
		boost::shared_ptr<NetPacket> thisPlayerLeft(new NetPacketPlayerLeft);
		NetPacketPlayerLeft::Data thisPlayerLeftData;
		thisPlayerLeftData.playerId = tmpPlayerData->GetUniqueId();
		static_cast<NetPacketPlayerLeft *>(thisPlayerLeft.get())->SetData(thisPlayerLeftData);
		SendToAllPlayers(thisPlayerLeft);

		GetCallback().SignalNetServerPlayerLeft(tmpPlayerData->GetName());
	}

	boost::microsec_timer closeTimer;
	closeTimer.start();
	CloseSessionList::value_type closeSessionData(closeTimer, session.sessionData);
	m_closeSessionList.push_back(closeSessionData);
}

void
ServerRecvThread::RemoveNotEstablishedSessions()
{
	SessionList removeList;

	SocketSessionMap::iterator session_i = m_sessionMap.begin();
	SocketSessionMap::iterator session_end = m_sessionMap.end();

	while (session_i != session_end)
	{
		// Remove all players which are not fully connected.
		assert(session_i->second.sessionData.get());
		if (session_i->second.sessionData->GetState() != SessionData::Established)
		{
			// Do not mess with the map within this loop.
			// Just store what needs to be removed.
			removeList.push_back(session_i->second);
		}
		++session_i;
	}

	SessionList::iterator remove_i = removeList.begin();
	SessionList::iterator remove_end = removeList.end();

	while (remove_i != remove_end)
	{
		// Inform the players that we are starting without them.
		// Gracefully remove them from the server.
		SessionError(*remove_i, ERR_NET_GAME_ALREADY_RUNNING);
		++remove_i;
	}
}

size_t
ServerRecvThread::GetCurNumberOfPlayers() const
{
	PlayerDataList playerList = GetPlayerDataList();
	return playerList.size();
}

bool
ServerRecvThread::IsPlayerConnected(const string &playerName) const
{
	bool retVal = false;

	SessionWrapper tmpSession = GetSession(playerName);

	if (tmpSession.sessionData.get() && tmpSession.playerData.get())
		retVal = true;

	return retVal;
}

void
ServerRecvThread::SetSessionPlayerData(boost::shared_ptr<SessionData> sessionData, boost::shared_ptr<PlayerData> playerData)
{
	assert(playerData.get());
	assert(!playerData->GetName().empty());
	assert(sessionData.get());

	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::iterator pos = m_sessionMap.find(sessionData->GetSocket());
	if (pos != m_sessionMap.end())
	{
		pos->second.playerData = playerData;

		// Signal joining player to GUI.
		GetCallback().SignalNetServerPlayerJoined(playerData->GetName());
	}
}

PlayerDataList
ServerRecvThread::GetPlayerDataList() const
{
	PlayerDataList playerList;
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::const_iterator session_i = m_sessionMap.begin();
	SocketSessionMap::const_iterator session_end = m_sessionMap.end();

	while (session_i != session_end)
	{
		// Get all players which are fully connected.
		if (session_i->second.sessionData->GetState() == SessionData::Established)
		{
			boost::shared_ptr<PlayerData> tmpPlayer(session_i->second.playerData);
			assert(tmpPlayer.get());
			assert(!tmpPlayer->GetName().empty());
			playerList.push_back(tmpPlayer);
		}
		++session_i;
	}
	// Sort the list by player number.
	playerList.sort(*boost::lambda::_1 < *boost::lambda::_2);
	return playerList;
}

int
ServerRecvThread::GetNextPlayerNumber() const
{
	int playerNumber = 0;

	PlayerDataList playerList = GetPlayerDataList();
	PlayerDataList::const_iterator player_i = playerList.begin();
	PlayerDataList::const_iterator player_end = playerList.end();

	// Assume the player list is sorted by player number.
	while (player_i != player_end)
	{
		if ((*player_i)->GetNumber() == playerNumber)
			playerNumber++;
		else
			break;
		++player_i;
	}

	return playerNumber;
}

void
ServerRecvThread::SendError(SOCKET s, int errorCode)
{
	boost::shared_ptr<NetPacket> packet(new NetPacketError);
	NetPacketError::Data errorData;
	errorData.errorCode = errorCode;
	static_cast<NetPacketError *>(packet.get())->SetData(errorData);
	GetSender().Send(s, packet);
}

void
ServerRecvThread::SendToAllPlayers(boost::shared_ptr<NetPacket> packet)
{
	// This function needs to be thread safe.
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::iterator i = m_sessionMap.begin();
	SocketSessionMap::iterator end = m_sessionMap.end();

	while (i != end)
	{
		assert(i->second.sessionData.get());

		// Send each fully connected client a copy of the packet.
		if (i->second.sessionData->GetState() == SessionData::Established)
			GetSender().Send(i->first, boost::shared_ptr<NetPacket>(packet->Clone()));
		++i;
	}
}

void
ServerRecvThread::SendToAllButOnePlayers(boost::shared_ptr<NetPacket> packet, SOCKET except)
{
	// This function needs to be thread safe.
	boost::mutex::scoped_lock lock(m_sessionMapMutex);

	SocketSessionMap::iterator i = m_sessionMap.begin();
	SocketSessionMap::iterator end = m_sessionMap.end();

	while (i != end)
	{
		// Send each fully connected client but one a copy of the packet.
		if (i->second.sessionData->GetState() == SessionData::Established)
			if (i->first != except)
				GetSender().Send(i->first, boost::shared_ptr<NetPacket>(packet->Clone()));
		++i;
	}
}

ServerCallback &
ServerRecvThread::GetCallback()
{
	return m_gui;
}

ServerRecvState &
ServerRecvThread::GetState()
{
	assert(m_curState);
	return *m_curState;
}

void
ServerRecvThread::SetState(ServerRecvState &newState)
{
	newState.Init();
	m_curState = &newState;
}

SenderThread &
ServerRecvThread::GetSender()
{
	assert(m_sender.get());
	return *m_sender;
}

ReceiverHelper &
ServerRecvThread::GetReceiver()
{
	assert(m_receiver.get());
	return *m_receiver;
}

Game &
ServerRecvThread::GetGame()
{
	assert(m_game.get());
	return *m_game;
}

const GameData &
ServerRecvThread::GetGameData() const
{
	return m_gameData;
}

const StartData &
ServerRecvThread::GetStartData() const
{
	return m_startData;
}

void
ServerRecvThread::SetStartData(const StartData &startData)
{
	m_startData = startData;
}

bool
ServerRecvThread::CheckPassword(const string &password) const
{
	return (password == m_password);
}

ServerSenderCallback &
ServerRecvThread::GetSenderCallback()
{
	assert(m_senderCallback.get());
	return *m_senderCallback;
}

GuiInterface &
ServerRecvThread::GetGui()
{
	return m_gui;
}

