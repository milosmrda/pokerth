/***************************************************************************
 *   Copyright (C) 2006 by FThauer FHammer   *
 *   f.thauer@web.de   *
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
#ifndef LOCALHAND_H
#define LOCALHAND_H

#include <enginefactory.h>
#include <guiinterface.h>
#include <boardinterface.h>
#include <playerinterface.h>
#include <handinterface.h>
#include <berointerface.h>

#include <vector>

class Log;

class LocalHand : public HandInterface
{
public:
	LocalHand(boost::shared_ptr<EngineFactory> f, GuiInterface*, boost::shared_ptr<BoardInterface>, Log*, PlayerList, PlayerList, PlayerList, int, int, unsigned, int, int);
	~LocalHand();

	void start();

	PlayerList getSeatsList() const {
		return seatsList;
	}
	PlayerList getActivePlayerList() const {
		return activePlayerList;
	}
	PlayerList getRunningPlayerList() const {
		return runningPlayerList;
	}

	boost::shared_ptr<BoardInterface> getBoard() const {
		return myBoard;
	}
	boost::shared_ptr<BeRoInterface> getPreflop() const {
		return myBeRo[GAME_STATE_PREFLOP];
	}
	boost::shared_ptr<BeRoInterface> getFlop() const {
		return myBeRo[GAME_STATE_FLOP];
	}
	boost::shared_ptr<BeRoInterface> getTurn() const {
		return myBeRo[GAME_STATE_TURN];
	}
	boost::shared_ptr<BeRoInterface> getRiver() const {
		return myBeRo[GAME_STATE_RIVER];
	}
	GuiInterface* getGuiInterface() const {
		return myGui;
	}
	boost::shared_ptr<BeRoInterface> getCurrentBeRo() const {
		return myBeRo[currentRound];
	}

	Log* getLog() const {
		return myLog;
	}

	void setMyID(int theValue) {
		myID = theValue;
	}
	int getMyID() const {
		return myID;
	}

	void setStartQuantityPlayers(int theValue) {
		startQuantityPlayers = theValue;
	}
	int getStartQuantityPlayers() const {
		return startQuantityPlayers;
	}

	void setCurrentRound(int theValue) {
		currentRound = theValue;
	}
	int getCurrentRound() const {
		return currentRound;
	}

	void setDealerPosition(int theValue) {
		dealerPosition = theValue;
	}
	int getDealerPosition() const {
		return dealerPosition;
	}

	void setSmallBlind(int theValue) {
		smallBlind = theValue;
	}
	int getSmallBlind() const {
		return smallBlind;
	}

	void setAllInCondition(bool theValue) {
		allInCondition = theValue;
	}
	bool getAllInCondition() const {
		return allInCondition;
	}

	void setStartCash(int theValue)	{
		startCash = theValue;
	}
	int getStartCash() const {
		return startCash;
	}

	void setBettingRoundsPlayed(int theValue) {
		bettingRoundsPlayed = theValue;
	}
	int getBettingRoundsPlayed() const {
		return bettingRoundsPlayed;
	}

	void setLastPlayersTurn(int theValue) {
		lastPlayersTurn = theValue;
	}
	int getLastPlayersTurn() const {
		return lastPlayersTurn;
	}

	void setLastActionPlayer ( unsigned theValue );
	unsigned getLastActionPlayer() const {
		return lastActionPlayer;
	}

	void setCardsShown(bool theValue) {
		cardsShown = theValue;
	}
	bool getCardsShown() const {
		return cardsShown;
	}

	void assignButtons();
	void setBlinds();

	void switchRounds();

protected:
	PlayerListIterator getSeatIt(unsigned) const;
	PlayerListIterator getActivePlayerIt(unsigned) const;
	PlayerListIterator getRunningPlayerIt(unsigned) const;

private:

	boost::shared_ptr<EngineFactory> myFactory;
	GuiInterface *myGui;
	boost::shared_ptr<BoardInterface> myBoard;
	Log *myLog;

	PlayerList seatsList; // all player
	PlayerList activePlayerList; // all player who are not out
	PlayerList runningPlayerList; // all player who are not folded, not all in and not out

	std::vector<boost::shared_ptr<BeRoInterface> > myBeRo;

	int myID;
	int startQuantityPlayers;
	unsigned dealerPosition;
	unsigned smallBlindPosition;
	unsigned bigBlindPosition;
	int currentRound; //0 = preflop, 1 = flop, 2 = turn, 3 = river
	int smallBlind;
	int startCash;

	int lastPlayersTurn;
	unsigned lastActionPlayer;

	bool allInCondition;
	bool cardsShown;

	// hier steht bis zu welcher bettingRound der human player gespielt hat: 0 - nur Preflop, 1 - bis Flop, ...
	int bettingRoundsPlayed;
};

#endif


