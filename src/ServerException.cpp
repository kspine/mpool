/*
 * Copyright (C) Lei.Peng, All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 */
#include "include/ServerException.h"

namespace MPool {

ServerException::ServerException(int error_no) {
	this->error_no = error_no;
}

const char* ServerException::what() {
	switch (this->error_no) {
	case ServerException::UNKNOWN_ERROR:
		return "Unknown Error";
	case ServerException::WRONG_PARAM:
		return "Wrong Parameters";
	case ServerException::INVALID_CONFIG:
		return "Invalid Configuration";
	case ServerException::DB_CONNECTION_FAIL:
		return "Fail to connect to database";
	case ServerException::OPEN_CONFIG_FILE_FAIL:
		return "Fail to open the configuration file";
	case ServerException::PARSE_CONFIG_FILE_FAIL:
		return "Cannot parse the configuration file";
	case ServerException::OPEN_ULIST_FILE_FAIL:
		return "Fail to open the user list file";
	case ServerException::PARSE_ULIST_FILE_FAIL:
		return "Cannot parse the user list file";
	case ServerException::SOCKET_PORT_INUSE:
		return "Bind failed, please ensure the listening port is not used by other process";
	case ServerException::SOCKET_LISTEN_FAIL:
		return "Fail to listen the socket";
	case ServerException::SOCKET_NOBLOCK_FAIL:
		return "Cannot set the socket to non-block status";
	case ServerException::EPOLL_CREATE_FAIL:
		return "Fail to call epoll_create";
	case ServerException::EPOLL_CTL_FAIL:
		return "Fail to call epoll_ctl";
	case ServerException::DBPOLL_GETCON_FAIL:
		return "Fail to get connection from the pool";
	default:
		return "Unknown Error";
	}
}

}
