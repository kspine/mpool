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

#ifndef SERVEREXCEPTION_H_
#define SERVEREXCEPTION_H_

#include <exception>

namespace MPool {

class ServerException: public std::exception {
public:
	ServerException(int error_no = 0);
	const char* what();
public:
	/**
	 * @brief Error Codes
	 * */
	const static int UNKNOWN_ERROR = 0x00;
	const static int WRONG_PARAM = 0x01;
	const static int INVALID_CONFIG = 0x02;
	const static int DB_CONNECTION_FAIL = 0x03;
	const static int OPEN_CONFIG_FILE_FAIL = 0x04;
	const static int PARSE_CONFIG_FILE_FAIL = 0x05;
	const static int OPEN_ULIST_FILE_FAIL = 0x06;
	const static int PARSE_ULIST_FILE_FAIL = 0x07;
	const static int SOCKET_PORT_INUSE = 0x08;
	const static int SOCKET_LISTEN_FAIL = 0x09;
	const static int SOCKET_NOBLOCK_FAIL = 0x0a;
	const static int EPOLL_CREATE_FAIL = 0x0b;
	const static int EPOLL_CTL_FAIL = 0x0c;
	const static int DBPOLL_GETCON_FAIL = 0x0d;
protected:
	int error_no;
};

}

#endif /* SERVEREXCEPTION_H_ */
