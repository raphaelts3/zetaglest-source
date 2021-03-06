// ==============================================================
//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#include "network_interface.h"

#include <exception>
#include <cassert>

#include "data_types.h"
#include "conversion.h"
#include "platform_util.h"
#include <fstream>
#include "util.h"
#include "network_protocol.h"
#include "leak_dumper.h"

using namespace Shared::Platform;
using namespace Shared::Util;
using namespace std;

namespace Glest {
	namespace Game {

		// =====================================================
		//	class NetworkInterface
		// =====================================================

		const int NetworkInterface::readyWaitTimeout = 99000;	// 99 seconds to 0 looks good on the screen

		bool NetworkInterface::allowGameDataSynchCheck = false;
		bool NetworkInterface::allowDownloadDataSynch = false;
		DisplayMessageFunction NetworkInterface::pCB_DisplayMessage = NULL;

		Vec4f MarkedCell::static_system_marker_color = MAGENTA;

		NetworkInterface::NetworkInterface() {
			networkAccessMutex = new Mutex(CODE_AT_LINE);

			networkGameDataSynchCheckOkMap = false;
			networkGameDataSynchCheckOkTile = false;
			networkGameDataSynchCheckOkTech = false;
			receivedDataSynchCheck = false;

			networkPlayerFactionCRCMutex = new Mutex(CODE_AT_LINE);
			for (unsigned int index = 0; index < (unsigned int) GameConstants::maxPlayers; ++index) {
				networkPlayerFactionCRC[index] = 0;
			}
		}

		void NetworkInterface::init() {
			networkAccessMutex = NULL;

			networkGameDataSynchCheckOkMap = false;
			networkGameDataSynchCheckOkTile = false;
			networkGameDataSynchCheckOkTech = false;
			receivedDataSynchCheck = false;

			gameSettings = GameSettings();

			networkPlayerFactionCRCMutex = NULL;
			for (unsigned int index = 0; index < (unsigned int) GameConstants::maxPlayers; ++index) {
				networkPlayerFactionCRC[index] = 0;
			}
		}

		NetworkInterface::~NetworkInterface() {
			delete networkAccessMutex;
			networkAccessMutex = NULL;

			delete networkPlayerFactionCRCMutex;
			networkPlayerFactionCRCMutex = NULL;
		}

		uint32 NetworkInterface::getNetworkPlayerFactionCRC(int index) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkPlayerFactionCRCMutex, mutexOwnerId);

			return networkPlayerFactionCRC[index];
		}
		void NetworkInterface::setNetworkPlayerFactionCRC(int index, uint32 crc) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkPlayerFactionCRCMutex, mutexOwnerId);

			networkPlayerFactionCRC[index] = crc;
		}

		void NetworkInterface::addChatInfo(const ChatMsgInfo &msg) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			chatTextList.push_back(msg);
		}

		void NetworkInterface::addMarkedCell(const MarkedCell &msg) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			markedCellList.push_back(msg);
		}
		void NetworkInterface::addUnMarkedCell(const UnMarkedCell &msg) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			unmarkedCellList.push_back(msg);
		}

		void NetworkInterface::sendMessage(NetworkMessage* networkMessage) {
			Socket* socket = getSocket(false);

			networkMessage->send(socket);
		}

		NetworkMessageType NetworkInterface::getNextMessageType(int waitMilliseconds) {
			Socket* socket = getSocket(false);
			int8 messageType = nmtInvalid;

			/*
				if(socket != NULL &&
					((waitMilliseconds <= 0 && socket->hasDataToRead() == true) ||
					 (waitMilliseconds > 0 && socket->hasDataToReadWithWait(waitMilliseconds) == true))) {
					//peek message type
					int dataSize = socket->getDataToRead();
					if(dataSize >= (int)sizeof(messageType)) {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] socket->getDataToRead() dataSize = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,dataSize);

						int iPeek = socket->peek(&messageType, sizeof(messageType));

						if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] socket->getDataToRead() iPeek = %d, messageType = %d [size = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,iPeek,messageType,sizeof(messageType));
					}
					else {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] PEEK WARNING, socket->getDataToRead() messageType = %d [size = %d], dataSize = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,messageType,sizeof(messageType),dataSize);
					}

					//sanity check new message type
					if(messageType < 0 || messageType >= nmtCount) {
						if(getConnectHasHandshaked() == true) {
							throw megaglest_runtime_error("Invalid message type: " + intToStr(messageType));
						}
						else {
							if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] Invalid message type = %d (no packet handshake yet so ignored)\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,messageType);
						}
					}
				}

				return static_cast<NetworkMessageType>(messageType);
			*/


			// According to here: https://support.microsoft.com/en-us/kb/192599
			// its a terrible sin to use MSG_PEEK so lets try an alternative

			/*
			int bytesReceived = socket->receive(&messageType, sizeof(messageType), true);
			if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] socket->getDataToRead() iPeek = %d, messageType = %d [size = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,bytesReceived,messageType,sizeof(messageType));

			return static_cast<NetworkMessageType>(messageType);
			*/


			if (socket != NULL &&
				((waitMilliseconds <= 0 && socket->hasDataToRead() == true) ||
				(waitMilliseconds > 0 && socket->hasDataToReadWithWait(waitMilliseconds) == true))) {
				//peek message type
				int dataSize = socket->getDataToRead();
				if (dataSize >= (int)sizeof(messageType)) {
					//if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] socket->getDataToRead() dataSize = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,dataSize);
					if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] before recv\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__);

					//			int iPeek = socket->peek(&messageType, sizeof(messageType));
					//			if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] socket->getDataToRead() iPeek = %d, messageType = %d [size = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,iPeek,messageType,sizeof(messageType));
					//			if(iPeek > 0) {
					int bytesReceived = socket->receive(&messageType, sizeof(messageType), true);
					if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] socket->getDataToRead() iPeek = %d, messageType = %d [size = %d]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, bytesReceived, messageType, sizeof(messageType));
					//}
				}
				//else {
				//	if(SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork,"In [%s::%s Line: %d] PEEK WARNING, socket->getDataToRead() messageType = %d [size = %d], dataSize = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,messageType,sizeof(messageType),dataSize);
				//}

				if (socket->isSocketValid() == false) {
					return nmtInvalid;
				}
				//sanity check new message type
				if (messageType < 0 || messageType >= nmtCount) {
					if (getConnectHasHandshaked() == true) {
						printf("%s%d\n", "Invalid message type: ", messageType);
					} else {
						if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] Invalid message type = %d (no packet handshake yet so ignored)\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, messageType);
					}
				}
			}

			return static_cast<NetworkMessageType>(messageType);

		}

		bool NetworkInterface::receiveMessage(NetworkMessage* networkMessage) {
			if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__);

			Socket* socket = getSocket(false);

			return networkMessage->receive(socket);
		}

		bool NetworkInterface::receiveMessage(NetworkMessage* networkMessage, NetworkMessageType type) {
			if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__);

			Socket* socket = getSocket(false);

			return networkMessage->receive(socket, type);
		}

		bool NetworkInterface::isConnected() {
			bool result = (getSocket() != NULL && getSocket()->isConnected());
			return result;
		}

		void NetworkInterface::setLastPingInfo(const NetworkMessagePing &ping) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			this->lastPingInfo = ping;
		}

		void NetworkInterface::setLastPingInfoToNow() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			this->lastPingInfo.setPingReceivedLocalTime(time(NULL));
		}

		NetworkMessagePing NetworkInterface::getLastPingInfo() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			return lastPingInfo;
		}
		double NetworkInterface::getLastPingLag() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			return difftime((long int) time(NULL), lastPingInfo.getPingReceivedLocalTime());
		}

		void NetworkInterface::DisplayErrorMessage(string sErr, bool closeSocket) {
			if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] sErr [%s]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, sErr.c_str());
			//SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] sErr [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,sErr.c_str());
			SystemFlags::OutputDebug(SystemFlags::debugSystem, "In [%s::%s Line: %d] sErr [%s]\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, sErr.c_str());

			if (closeSocket == true && getSocket() != NULL) {
				close();
			}

			if (pCB_DisplayMessage != NULL) {
				pCB_DisplayMessage(sErr.c_str(), false);
			} else {
				throw megaglest_runtime_error(sErr);
			}
		}

		std::vector<ChatMsgInfo> NetworkInterface::getChatTextList(bool clearList) {
			std::vector<ChatMsgInfo> result;

			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (chatTextList.empty() == false) {
				result = chatTextList;

				if (clearList == true) {
					chatTextList.clear();
				}
			}
			return result;
		}

		void NetworkInterface::clearChatInfo() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (chatTextList.empty() == false) {
				if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] chatTextList.size() = %d\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, chatTextList.size());
				chatTextList.clear();
			}
		}

		std::vector<MarkedCell> NetworkInterface::getMarkedCellList(bool clearList) {
			std::vector<MarkedCell> result;

			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (markedCellList.empty() == false) {
				result = markedCellList;

				if (clearList == true) {
					markedCellList.clear();
				}
			}
			return result;
		}

		void NetworkInterface::clearMarkedCellList() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (markedCellList.empty() == false) {
				if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] markedCellList.size() = %d\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, markedCellList.size());
				markedCellList.clear();
			}
		}

		std::vector<UnMarkedCell> NetworkInterface::getUnMarkedCellList(bool clearList) {
			std::vector<UnMarkedCell> result;

			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (unmarkedCellList.empty() == false) {
				result = unmarkedCellList;

				if (clearList == true) {
					unmarkedCellList.clear();
				}
			}
			return result;
		}

		void NetworkInterface::clearUnMarkedCellList() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (unmarkedCellList.empty() == false) {
				if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] unmarkedCellList.size() = %d\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, unmarkedCellList.size());
				unmarkedCellList.clear();
			}
		}

		std::vector<MarkedCell> NetworkInterface::getHighlightedCellList(bool clearList) {
			std::vector<MarkedCell> result;

			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (highlightedCellList.empty() == false) {
				result = highlightedCellList;

				if (clearList == true) {
					highlightedCellList.clear();
				}
			}
			return result;
		}

		void NetworkInterface::clearHighlightedCellList() {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			if (highlightedCellList.empty() == false) {
				if (SystemFlags::getSystemSettingType(SystemFlags::debugNetwork).enabled) SystemFlags::OutputDebug(SystemFlags::debugNetwork, "In [%s::%s Line: %d] markedCellList.size() = %d\n", extractFileFromDirectoryPath(__FILE__).c_str(), __FUNCTION__, __LINE__, markedCellList.size());
				highlightedCellList.clear();
			}
		}

		void NetworkInterface::setHighlightedCell(const MarkedCell &msg) {
			static string mutexOwnerId = string(__FILE__) + string("_") + intToStr(__LINE__);
			MutexSafeWrapper safeMutex(networkAccessMutex, mutexOwnerId);

			for (int idx = 0; idx < (int) highlightedCellList.size(); idx++) {
				MarkedCell mc = highlightedCellList[idx];
				if (mc.getFactionIndex() == msg.getFactionIndex()) {
					highlightedCellList.erase(highlightedCellList.begin() + idx);
					break;
				}
			}
			highlightedCellList.push_back(msg);
		}

		float NetworkInterface::getThreadedPingMS(std::string host) {
			float result = -1;

			if (getSocket() != NULL) {
				result = getSocket()->getThreadedPingMS(host);
			}
			return result;
		}

		// =====================================================
		//	class GameNetworkInterface
		// =====================================================

		GameNetworkInterface::GameNetworkInterface() {
			quit = false;
		}

		void GameNetworkInterface::requestCommand(const NetworkCommand *networkCommand, bool insertAtStart) {
			assert(networkCommand != NULL);
			Mutex *mutex = getServerSynchAccessor();

			if (insertAtStart == false) {
				MutexSafeWrapper safeMutex(mutex, string(__FILE__) + "_" + intToStr(__LINE__));
				requestedCommands.push_back(*networkCommand);
			} else {
				MutexSafeWrapper safeMutex(mutex, string(__FILE__) + "_" + intToStr(__LINE__));
				requestedCommands.insert(requestedCommands.begin(), *networkCommand);
			}
		}

		// =====================================================
		//	class FileTransferSocketThread
		// =====================================================

		const int32 SEND_FILE = 0x20;
		const int32 ACK = 0x47;

		FileTransferSocketThread::FileTransferSocketThread(FileTransferInfo fileInfo) : info(fileInfo) {
			this->info.serverPort += 100;
		}

		void FileTransferSocketThread::execute() {
			if (info.hostType == eServer) {
				ServerSocket serverSocket;
				serverSocket.bind(this->info.serverPort);
				serverSocket.listen(1);
				Socket *clientSocket = serverSocket.accept();

				char data[513] = "";
				memset(data, 0, 256);

				clientSocket->receive(data, 256, true);
				if (*data == SEND_FILE) {
					FileInfo file;

					memcpy(&file, data + 1, sizeof(file));

					*data = ACK;
					clientSocket->send(data, 256);

					Checksum checksum;
					checksum.addFile(file.fileName);
					file.filecrc = checksum.getSum();

					ifstream infile(file.fileName.c_str(), ios::in | ios::binary | ios::ate);
					if (infile.is_open() == true) {
						file.filesize = infile.tellg();
						infile.seekg(0, ios::beg);

						memset(data, 0, 256);
						*data = SEND_FILE;
						memcpy(data + 1, &file, sizeof(file));

						clientSocket->send(data, 256);
						clientSocket->receive(data, 256, true);
						if (*data != ACK) {
							//transfer error
						}

						int remain = file.filesize % 512;
						int packs = (file.filesize - remain) / 512;

						while (packs--) {
							infile.read(data, 512);
							//if(!ReadFile(file,data,512,&read,NULL))
							//    ; //read error
							//if(written!=pack)
							//    ; //read error
							clientSocket->send(data, 512);
							clientSocket->receive(data, 256, true);
							if (*data != ACK) {
								//transfer error
							}
						}

						infile.read(data, remain);
						//if(!ReadFile(file,data,remain,&read,NULL))
						//   ; //read error
						//if(written!=pack)
						//   ; //read error

						clientSocket->send(data, remain);
						clientSocket->receive(data, 256, true);
						if (*data != ACK) {
							//transfer error
						}

						infile.close();
					}
				}

				delete clientSocket;
			} else {
				Ip ip(this->info.serverIP);
				ClientSocket clientSocket;
				clientSocket.connect(this->info.serverIP, this->info.serverPort);

				if (clientSocket.isConnected() == true) {
					FileInfo file;
					file.fileName = this->info.fileName;
					//file.filesize =
					//file.filecrc  = this->info.

					string path = extractDirectoryPathFromFile(file.fileName);
					createDirectoryPaths(path);
					ofstream outFile(file.fileName.c_str(), ios_base::binary | ios_base::out);
					if (outFile.is_open() == true) {
						char data[513] = "";
						memset(data, 0, 256);
						*data = SEND_FILE;
						memcpy(data + 1, &file, sizeof(file));

						clientSocket.send(data, 256);
						clientSocket.receive(data, 256, true);
						if (*data != ACK) {
							//transfer error
						}

						clientSocket.receive(data, 256, true);
						if (*data == SEND_FILE) {
							memcpy(&file, data + 1, sizeof(file));
							*data = ACK;
							clientSocket.send(data, 256);

							int remain = file.filesize % 512;
							int packs = (file.filesize - remain) / 512;

							while (packs--) {
								clientSocket.receive(data, 512, true);

								outFile.write(data, 512);
								if (outFile.bad()) {
									//int ii = 0;
								}
								//if(!WriteFile(file,data,512,&written,NULL))
								//   ; //write error
								//if(written != pack)
								//   ; //write error
								*data = ACK;
								clientSocket.send(data, 256);
							}
							clientSocket.receive(data, remain, true);

							outFile.write(data, remain);
							if (outFile.bad()) {
								//int ii = 0;
							}

							//if(!WriteFile(file,data,remain,&written,NULL))
							//    ; //write error
							//if(written!=pack)
							//    ; //write error
							*data = ACK;
							clientSocket.send(data, 256);

							Checksum checksum;
							checksum.addFile(file.fileName);
							uint32 crc = checksum.getSum();
							if (file.filecrc != crc) {
								//int ii = 0;
							}

							//if(calc_crc(file)!=info.crc)
							//   ; //transfeer error
						}

						outFile.close();
					}
				}
			}
		}


	}
}//end namespace
