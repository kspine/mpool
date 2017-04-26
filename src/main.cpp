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
#include <string>
#include <queue>
#include <list>
#include <map>
#include <exception>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#include <my_global.h>
#include <mysql.h>
#include "include/ServerException.h"
#include "include/DBPool.h"
#include "include/Client.h"
#include "include/Manager.h"
#include "include/Server.h"

MPool::Server *__server;

void show_help() {
	printf("Usage: mpool [options]\n");
	printf("--help  Show this usage.\n");
	printf("--config_file Path of configuration \n");
	printf("--user_list_file Path of user list \n");
	printf("--pid_file  Process id file, default /var/run/mpool.pid \n");
}

void signal_exit(int signal_no) {
	__server->stop();
}

int main(int argc, char **argv) {
	const char *short_options = "P:c:u:h";
	struct option long_options[] = { { "help", 0, NULL, 'h' }, { "pid_file", 1,
	NULL, 'P' }, { "config_file", 1, NULL, 'c' }, { "user_list_file", 1, NULL,
			'u' }, { 0, 0, 0, 0 } };
	char c;
	std::string pid_file = "/var/run/mpool.pid";
	std::string config_file = "/etc/mpool.conf.d/config.json";
	std::string user_list_file = "/etc/mpool.conf.d/user_list.json";
	while ((c = getopt_long(argc, argv, short_options, long_options, NULL))
			!= -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'u':
			user_list_file = optarg;
			break;
		case 'P':
			pid_file = optarg;
			break;
		case 'h':
			show_help();
			return 0;
		}
	}
	std::fstream fpid;
	fpid.open(pid_file.c_str(), std::ios_base::out);
	if (!fpid.is_open()) {
		return -1;
	}
	fpid << getpid();
	fpid.close();
	MPool::Server server;
	signal(SIGTERM, signal_exit);
	signal(SIGINT, signal_exit);
	signal(SIGQUIT, signal_exit);
	signal(SIGPIPE, SIG_IGN);
	try {
		__server = &server;
		server.init(config_file.c_str(), user_list_file.c_str());
		server.run();
	} catch (MPool::ServerException& e) {
		std::cout << e.what() << std::endl;
	} catch (Json::Exception &e) {
		std::cout << e.what() << std::endl;
	} catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	} catch (...) {
		std::cout << "Unknown Exception" << std::endl;
	}
	return 0;
}
