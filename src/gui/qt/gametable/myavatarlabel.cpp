/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2011 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/
#include "myavatarlabel.h"

#include "gametableimpl.h"
#include "gametablestylereader.h"
#include "session.h"
#include "playerinterface.h"
#include "game.h"
#include "mymessagedialogimpl.h"
#include "chattools.h"

using namespace std;

MyAvatarLabel::MyAvatarLabel(QGroupBox* parent)
	: QLabel(parent), voteRunning(FALSE), transparent(FALSE)
{

	myContextMenu = new QMenu;
	action_EditTip = new QAction(QIcon(":/gfx/user_properties.png"), tr("Add/Edit/Remove tooltip"), myContextMenu);
	myContextMenu->addAction(action_EditTip);
	action_VoteForKick = new QAction(QIcon(":/gfx/list_remove_user.png"), tr("Start vote to kick this player"), myContextMenu);
	myContextMenu->addAction(action_VoteForKick);
	action_IgnorePlayer = new QAction(QIcon(":/gfx/im-ban-user.png"), tr("Ignore Player"), myContextMenu);
	myContextMenu->addAction(action_IgnorePlayer);
	action_ReportBadAvatar = new QAction(QIcon(":/gfx/emblem-important.png"), tr("Report inappropriate avatar"), myContextMenu);
	myContextMenu->addAction(action_ReportBadAvatar);


	connect( action_VoteForKick, SIGNAL ( triggered() ), this, SLOT ( sendTriggerVoteOnKickSignal() ) );
	connect( action_IgnorePlayer, SIGNAL ( triggered() ), this, SLOT ( putPlayerOnIgnoreList() ) );
	connect( action_ReportBadAvatar, SIGNAL ( triggered() ), this, SLOT ( reportBadAvatar() ) );
	connect( action_EditTip, SIGNAL( triggered() ), this, SLOT ( startEditTip() ) );
}


MyAvatarLabel::~MyAvatarLabel()
{
}

void MyAvatarLabel::contextMenuEvent ( QContextMenuEvent *event )
{

	assert(myW->getSession()->getCurrentGame());
	if (myW->getSession()->isNetworkClientRunning()) {

		boost::shared_ptr<PlayerInterface> humanPlayer = myW->getSession()->getCurrentGame()->getSeatsList()->front();
		//only active players are allowed to start a vote
		if(humanPlayer->getMyActiveStatus()) {

			boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
			PlayerListConstIterator it_c;
			int activePlayerCounter=0;
			PlayerList seatList = currentGame->getSeatsList();
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
				if((*it_c)->getMyActiveStatus()) activePlayerCounter++;
			}
			GameInfo info(myW->getSession()->getClientGameInfo(myW->getSession()->getClientCurrentGameId()));

			if(activePlayerCounter > 2 && !voteRunning && info.data.gameType != GAME_TYPE_RANKING && !myW->getGuestMode()) {
				setVoteOnKickContextMenuEnabled(TRUE);
			} else {
				setVoteOnKickContextMenuEnabled(FALSE);
			}

			action_IgnorePlayer->setEnabled(true);
			action_EditTip->setEnabled(true);
			int j=0;
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

				if(myId == j) {
					if(j == 0) {
						action_EditTip->setDisabled(true);
					}
					if(myW->myStartWindow->getSession()->getGameType() == Session::GAME_TYPE_NETWORK) {
						action_EditTip->setDisabled(true);
					}

					if(myW->getSession()->getClientPlayerInfo((*it_c)->getMyUniqueID()).isGuest) {
						action_IgnorePlayer->setDisabled(true);
						action_EditTip->setDisabled(true);
					}

					if(myW->getSession()->getGameType() == Session::GAME_TYPE_INTERNET && !((*it_c)->getMyAvatar().empty()) ) {
						action_ReportBadAvatar->setVisible(TRUE);
					} else {
						action_ReportBadAvatar->setVisible(FALSE);
					}
				}
				j++;
			}

			int i=0;
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

				//also inactive player which stays on table can be voted to kick
				if(myContextMenuEnabled && myId != 0 && myId == i && (*it_c)->getMyType() != PLAYER_TYPE_COMPUTER && ( (*it_c)->getMyActiveStatus() || (*it_c)->getMyStayOnTableStatus() ) )
					showContextMenu(event->globalPos());

				i++;
			}
		}
	}
}

void MyAvatarLabel::showContextMenu(const QPoint &pos)
{

	myContextMenu->popup(pos);
}

void MyAvatarLabel::setPlayerRating(QString playerInfo)
{
	int found=0;
	QStringList playerInfoList=playerInfo.split("\"", QString::KeepEmptyParts, Qt::CaseSensitive), tipInfo;
	boost::shared_ptr<Game> currentGame = myW->myStartWindow->getSession()->getCurrentGame();
	PlayerListConstIterator it_c;
	PlayerList seatsList = currentGame->getSeatsList();
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	std::list<std::string> result;
	std::string separator="(!#$%)";
	std::list<std::string>::iterator iterator;
	for(iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		tipInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", QString::KeepEmptyParts, Qt::CaseSensitive);
		if(tipInfo.at(0)==playerInfoList.at(0)) {
			result.push_back(tipInfo.at(0).toUtf8().constData()+separator+tipInfo.at(1).toUtf8().constData()+separator+playerInfoList.at(1).toUtf8().constData()+separator);
			found=1;
		} else {
			result.push_back(tipInfo.at(0).toUtf8().constData()+separator+tipInfo.at(1).toUtf8().constData()+separator+tipInfo.at(2).toUtf8().constData()+separator);
		}
	}
	if(found==0) {
		result.push_back(playerInfoList.at(0).toUtf8().constData()+separator+separator+playerInfoList.at(1).toUtf8().constData()+separator);
	}
	myW->getMyConfig()->writeConfigStringList("PlayerTooltips", result);
	myW->getMyConfig()->writeBuffer();
	refreshStars();
}


void MyAvatarLabel::startChangePlayerTip(QString playerName)
{
	if(myW->tabWidget_Left->widget(2) == myW->tab_Kick) {
		myW->tabWidget_Left->insertTab(3, myW->tab_editTip, playerName);
		myW->tabWidget_Left->setCurrentIndex(3);
	} else {
		myW->tabWidget_Left->insertTab(2, myW->tab_editTip, playerName);
		myW->tabWidget_Left->setCurrentIndex(2);
	}
	myW->textEdit_tipInput->setPlainText(getPlayerTip(playerName));
}

void MyAvatarLabel::refreshStars()
{
	QString fontSize("12");
	QString fontFamily(myW->getMyGameTableStyle()->getFont1String());

#ifdef _WIN32
	fontSize = "10";
#else
#ifdef __APPLE__
	fontSize = "7";
#else
	fontSize = "12";
#endif
#endif

	boost::shared_ptr<Game> curGame = myW->myStartWindow->getSession()->getCurrentGame();
	PlayerListConstIterator it_c;
	int seatPlace;
	PlayerList seatsList = curGame->getSeatsList();
	for (seatPlace=0,it_c=seatsList->begin(); it_c!=seatsList->end(); ++it_c, seatPlace++) {
		for(int i=1; i<=5; i++)myW->playerStarsArray[i][seatPlace]->setText("");
		if(myW->myStartWindow->getSession()->getGameType() == Session::GAME_TYPE_INTERNET && !myW->getSession()->getClientPlayerInfo((*it_c)->getMyUniqueID()).isGuest && (*it_c)->getMyType() != PLAYER_TYPE_COMPUTER) {
			if((*it_c)->getMyStayOnTableStatus() == TRUE && (*it_c)->getMyName()!="" && seatPlace!=0) {
				int playerStars=getPlayerRating(QString::fromUtf8((*it_c)->getMyName().c_str()));
				for(int i=1; i<=5; i++) {
					myW->playerStarsArray[i][seatPlace]->setText("<a style='color: #"+myW->getMyGameTableStyle()->getRatingStarsColor()+"; "+fontFamily+" font-size: "+fontSize+"px; text-decoration: none;' href='"+QString::fromUtf8((*it_c)->getMyName().c_str())+"\""+QString::number(i)+"'>&#9734;</a>");
				}
				for(int i=1; i<=playerStars; i++) {
					myW->playerStarsArray[i][seatPlace]->setText("<a style='color: #"+myW->getMyGameTableStyle()->getRatingStarsColor()+"; "+fontFamily+" font-size: "+fontSize+"px; text-decoration: none;' href='"+QString::fromUtf8((*it_c)->getMyName().c_str())+"\""+QString::number(i)+"'>&#9733;</a>");
				}
			}
		}
	}
}

void MyAvatarLabel::refreshTooltips()
{
	boost::shared_ptr<Game> currentGame = myW->myStartWindow->getSession()->getCurrentGame();
	PlayerListConstIterator it_c;
	int seatPlace;
	PlayerList seatsList = currentGame->getSeatsList();
	for (seatPlace=0,it_c=seatsList->begin(); it_c!=seatsList->end(); ++it_c, seatPlace++) {
		if((*it_c)->getMyStayOnTableStatus() == TRUE || (*it_c)->getMyActiveStatus()) {
			bool computerPlayer = false;
			if((*it_c)->getMyType() == PLAYER_TYPE_COMPUTER) {
				computerPlayer = true;
			}
			if(!computerPlayer && getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str()))!="" && seatPlace!=0) {
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setText(QString("<a style='text-decoration: none; color: #"+myW->getMyGameTableStyle()->getPlayerInfoHintTextColor()+"; font-size: 14px; font-weight: bold; font-family:serif;' href=\'")+QString::fromUtf8((*it_c)->getMyName().c_str())+"\'>i</a>");
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip( getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str())) );
				myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip( getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str())) );
			} else {
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setText("");
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip("");
				myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip("");
			}
		} else {
			myW->playerTipLabelArray[(*it_c)->getMyID()]->setText("");
			myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip("");
			myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip("");

		}


	}
	refreshStars();
}

int MyAvatarLabel::getPlayerRating(QString playerName)
{
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	std::list<std::string>::iterator iterator;
	QStringList playerInfo;
	QString result="0";
	for(iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		playerInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", QString::KeepEmptyParts, Qt::CaseSensitive);
		if(playerInfo.at(0)==playerName) {
			result=playerInfo.at(2);
			break;
		}
	}
	return result.toInt();
}


QString MyAvatarLabel::getPlayerTip(QString playerName)
{
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	std::list<std::string>::iterator iterator;
	QStringList playerInfo;
	for(iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		playerInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", QString::KeepEmptyParts, Qt::CaseSensitive);
		if(playerInfo.at(0)==playerName)return playerInfo.at(1);
	}
	return QString("");
}


void MyAvatarLabel::setPlayerTip()
{
	int found=0;
	//std::string rating="1";
	std::string separator="(!#$%)";
	std::string tip = std::string((const char*)myW->textEdit_tipInput->toPlainText().toUtf8());
	std::string playerName;
	if(myW->tabWidget_Left->widget(2) == myW->tab_editTip)playerName = myW->tabWidget_Left->tabText(2).toUtf8().constData();
	if(myW->tabWidget_Left->widget(3) == myW->tab_editTip)playerName = myW->tabWidget_Left->tabText(3).toUtf8().constData();
	QStringList playerInfo;
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	std::list<std::string> result;
	std::list<std::string>::iterator iterator;
	for(iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		playerInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", QString::KeepEmptyParts, Qt::CaseSensitive);
		if(QString::fromUtf8(playerName.c_str())==playerInfo.at(0)) {
			result.push_back(playerName+separator+QString::fromUtf8(tip.c_str()).toUtf8().constData()+separator+playerInfo.at(2).toStdString()+separator);
			found=1;
		} else {
			result.push_back(playerInfo.at(0).toUtf8().constData()+separator+playerInfo.at(1).toUtf8().constData()+separator+playerInfo.at(2).toStdString()+separator);
		}
	}
	if(found==0) {
		result.push_back(playerName.c_str()+separator+QString::fromUtf8(tip.c_str()).toUtf8().constData()+separator+QString("0").toStdString()+separator);
	}
	myW->getMyConfig()->writeConfigStringList("PlayerTooltips", result);
	myW->getMyConfig()->writeBuffer();

	if(myW->tabWidget_Left->widget(3) == myW->tab_editTip) {
		myW->tabWidget_Left->removeTab(3);
	} else if(myW->tabWidget_Left->widget(2) == myW->tab_editTip) {
		myW->tabWidget_Left->removeTab(2);
	}
	refreshTooltips();
}


void MyAvatarLabel::sendTriggerVoteOnKickSignal()
{
	myW->triggerVoteOnKick(myId);
}

void MyAvatarLabel::setEnabledContextMenu(bool b)
{
	myContextMenuEnabled = b;
}

void MyAvatarLabel::setVoteOnKickContextMenuEnabled(bool b)
{
	action_VoteForKick->setEnabled(b);
}

void MyAvatarLabel::setPixmap ( const QPixmap &pix, const bool trans)
{

	myPixmap = pix.scaled(50,50, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	transparent = trans;
	update();

}

void MyAvatarLabel::setPixmapAndCountry ( const QPixmap &pix,QString countryString, int seatPlace, const bool trans)
{

	QPixmap resultAvatar(pix.scaled(50,50, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPainter painter(&resultAvatar);
	int showCountryFlags = myW->getMyConfig()->readConfigInt("ShowCountryFlagInAvatar");
	if(showCountryFlags && !countryString.isEmpty()) {
		if(seatPlace<=2||seatPlace>=8) {
			painter.drawPixmap(resultAvatar.width()-(QPixmap(countryString)).width(),resultAvatar.height()-(QPixmap(countryString)).height(),QPixmap(countryString));
		} else {
			painter.drawPixmap(resultAvatar.width()-(QPixmap(countryString)).width(),0,QPixmap(countryString));
		}
	}
	painter.end();
	myPixmap = resultAvatar;
	transparent = trans;
	update();

}

void MyAvatarLabel::paintEvent(QPaintEvent*)
{

	QPainter painter(this);
	if(transparent)
		painter.setOpacity(0.4);
	else
		painter.setOpacity(1.0);

	painter.drawPixmap(0,0,myPixmap);
}

bool MyAvatarLabel::playerIsOnIgnoreList(QString playerName)
{

	list<std::string> playerIgnoreList = myW->getMyConfig()->readConfigStringList("PlayerIgnoreList");
	list<std::string>::iterator it1;
	for(it1= playerIgnoreList.begin(); it1 != playerIgnoreList.end(); ++it1) {

		if(playerName == QString::fromUtf8(it1->c_str())) {
			return true;
		}
	}
	return false;
}


void MyAvatarLabel::putPlayerOnIgnoreList()
{

	QStringList list;
	PlayerListConstIterator it_c;
	PlayerList seatList = myW->getSession()->getCurrentGame()->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
		list << QString::fromUtf8((*it_c)->getMyName().c_str());
	}

	if(!playerIsOnIgnoreList(list.at(myId))) {

		myMessageDialogImpl dialog(myW->getMyConfig(), this);
		if(dialog.exec(4, tr("You will no longer receive chat messages or game invitations from this user.<br>Do you really want to put player <b>%1</b> on ignore list?").arg(list.at(myId)), tr("PokerTH - Question"), QPixmap(":/gfx/im-ban-user_64.png"), QDialogButtonBox::Yes|QDialogButtonBox::No, false ) == QDialog::Accepted) {

			std::list<std::string> playerIgnoreList = myW->getMyConfig()->readConfigStringList("PlayerIgnoreList");
			playerIgnoreList.push_back(list.at(myId).toUtf8().constData());
			myW->getMyConfig()->writeConfigStringList("PlayerIgnoreList", playerIgnoreList);
			myW->getMyConfig()->writeBuffer();

			myW->getMyChat()->refreshIgnoreList();
		}
	}
}

void MyAvatarLabel::reportBadAvatar()
{

	boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
	int j=0;
	PlayerListConstIterator it_c;
	PlayerList seatList = currentGame->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

		if(myId == j) {

			QString avatar = QString::fromUtf8((*it_c)->getMyAvatar().c_str());
			if(!avatar.isEmpty()) {

				QString nick = QString::fromUtf8((*it_c)->getMyName().c_str());
				int ret = QMessageBox::question(this, tr("PokerTH - Question"),
												tr("Are you sure you want to report the avatar of \"%1\" as inappropriate?").arg(nick), QMessageBox::Yes | QMessageBox::No);

				if(ret == QMessageBox::Yes) {
					QFileInfo fi(avatar);
					myW->getSession()->reportBadAvatar((*it_c)->getMyUniqueID(), fi.baseName().toStdString());
				}
			}
			break;
		}
		j++;
	}
}


void MyAvatarLabel::startEditTip()
{
	boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
	int j=0;
	PlayerListConstIterator it_c;
	PlayerList seatList = currentGame->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
		if(myId == j) {
			QString nick = QString::fromUtf8((*it_c)->getMyName().c_str());
			startChangePlayerTip(nick);
			break;
		}
		j++;
	}
}
