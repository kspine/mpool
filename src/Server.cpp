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
#include <sstream>
#include <list>
#include <queue>
#include <vector>
#include <map>
#include <exception>
#include <fstream>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/version.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <fcntl.h>
#include <jsoncpp/json/json.h>
#include <my_global.h>
#include <mysql.h>
#include "include/version.h"
#include "include/DBPool.h"
#include "include/Client.h"
#include "include/Manager.h"
#include "include/Server.h"
#include "include/ServerException.h"

namespace MPool {

Server::Server() {
	this->support_protocol_versions.push_back(MPOOL_PROTOCOL_VERSION);
	this->jsonReader = new Json::Reader(Json::Features::strictMode());
	this->jsonWriter = new Json::FastWriter();
	// Default value;
	this->max_connections = 2000;
	this->port = 3840;
	this->workers = 4;
	this->clients = new std::map<int, MPool::Client*>();
	this->gc_tid = 0;
	this->manager = NULL;
	this->db_pool = NULL;
	this->socket_fd = 0;
	this->running = false;
	this->gc_running = false;
	this->epoll_fd = 0;
	this->pool_size = 4;
	pthread_mutex_init(&this->client_mutex, NULL);
}

Server::~Server() {
	this->doCleanWorks();
	pthread_mutex_destroy(&this->client_mutex);
	delete this->jsonWriter;
	delete this->jsonReader;
	delete this->clients;
}

bool Server::isSocketVal(int fd) {
	return true;
	//return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

void Server::socketMessage(int s, const char *status, const char *code,
		const char *msg, const char *data) {
	if (!s) {
		return;
	}
	Json::Value root;
	root["protocol_version"] = MPOOL_PROTOCOL_VERSION;
	root["status"] = status;
	root["code"] = code;
	root["message"] = msg;
	root["data"] = data;

	std::string str = this->jsonWriter->write(root);
	//Append data length;
	std::stringstream ss;
	ss.width(16);
	ss << str.length();
	ss.fill('0');
	ss << str;
	str = ss.str();
#ifdef DEBUG
	std::cout<<str<<std::endl;
#endif
	if (this->isSocketVal(s)) {
		ssize_t sent = 0;
		while (sent < (ssize_t) str.length()) {
			ssize_t psent = write(s, str.substr(sent).c_str(),
					str.substr(sent).length());
			if (-1 == psent) {
				if (EAGAIN == errno) {
					//Sleep a while and try again;
					psent = 0;
					usleep(50);
				} else {
					std::cout << "(Server) Send Socket Data Error: "
							<< strerror(errno) << std::endl;
					close(s);
					break;
				}
			}
			sent += psent;
		}
	}
}

void Server::clientMessage(Client *client, const char *status,
		const char *code, const char *msg, const char *data) {
	if (!client) {
		return;
	}
	this->socketMessage(client->getSocket(), status, code, msg, data);
}

void Server::doCleanWorks() {
	if (!this->clients->empty()) {
#ifdef DEBUG
		std::cout<<"Cleaning clients"<<std::endl;
#endif
		for (std::map<int, MPool::Client*>::iterator it =
				this->clients->begin(); it != this->clients->end(); it++) {
			Client *client = it->second;
			this->db_pool->freeDB(client->getDBConnection());
			delete client;
			this->clients->erase(it);
		}
	}
#ifdef DEBUG
	std::cout<<"Stop the manager"<<std::endl;
#endif
	this->manager->stop();
#ifdef DEBUG
	std::cout<<"Free the manager"<<std::endl;
#endif
	delete this->manager;
#ifdef DEBUG
	std::cout<<"Cleaning DB Connection Pool"<<std::endl;
#endif
	delete this->db_pool;
}

void Server::init(const char *config_file, const char *user_list_file) {
	if (!config_file || !user_list_file) {
		throw ServerException(ServerException::WRONG_PARAM);
	}
	this->config_file = config_file;
	this->user_list_file = user_list_file;
	this->reload();
#ifdef DEBUG
	std::cout<<"Server has been initialized"<<std::endl;
#endif
}

void Server::reload() {
	this->readConfigFile(this->config_file.c_str());
	this->readUserListFile(this->user_list_file.c_str());
	// Initialize DB Connection Pool;
	this->db_pool = new DBPool();
	if (!this->db_pool) {
		throw ServerException(ServerException::DB_CONNECTION_FAIL);
	}
#ifdef DEBUG
	std::cout<<"Starting DB Connection Pool"<<std::endl;
#endif
	if (!this->db_pool->start(this->config["mysql_host"],
			this->config["mysql_user"], this->config["mysql_pass"],
			this->config["mysql_db"], atoi(this->config["mysql_port"].c_str()))) {
		this->doCleanWorks();
		throw ServerException(ServerException::DB_CONNECTION_FAIL);
	}
#ifdef DEBUG
	std::cout<<"Set DB Pool Size:"<<this->pool_size<<std::endl;
#endif
	this->db_pool->setMinAlives(this->pool_size);
#ifdef DEBUG
	std::cout<<"Initializing manager"<<std::endl;
#endif
	this->manager = new Manager(this->workers);
}

void Server::readConfigFile(const char *config_file) {
	this->config.clear();
	std::fstream fs;
	fs.open(config_file, std::ios_base::in);
	if (!fs.is_open()) {
		throw ServerException(ServerException::OPEN_CONFIG_FILE_FAIL);
	}
	Json::Value root;
	if (!this->jsonReader->parse(fs, root, false)) {
#ifdef DEBUG
		std::cout<<"Configuration error, wrong JSON data"<<std::endl;
#endif
		fs.close();
		throw ServerException(ServerException::PARSE_CONFIG_FILE_FAIL);
	}
	std::stringstream ss;
	ss << this->max_connections;
	this->config["max_connections"]
			= root.isMember("max_connections") ? root["max_connections"].asString()
					: ss.str();
	this->max_connections = atol(this->config["max_connections"].c_str());
	if (!root.isMember("mysql") || !root["mysql"].isObject()) {
#ifdef DEBUG
		std::cout<<"MySQL configuration error"<<std::endl;
#endif
		fs.close();
		throw ServerException(ServerException::PARSE_CONFIG_FILE_FAIL);
	}
	Json::Value mysql_json = root["mysql"];
	this->config["mysql_host"]
			= mysql_json.isMember("host") ? mysql_json["host"].asString()
					: "localhost";
	this->config["mysql_user"]
			= mysql_json.isMember("user") ? mysql_json["user"].asString()
					: "root";
	this->config["mysql_pass"]
			= mysql_json.isMember("pass") ? mysql_json["pass"].asString() : "";
	this->config["mysql_db"]
			= mysql_json.isMember("db") ? mysql_json["db"].asString() : "mysql";
	this->config["mysql_port"]
			= mysql_json.isMember("port") ? mysql_json["port"].asString()
					: "3306";
	ss.str("");
	ss << this->port;
	this->config["port"] = root.isMember("port") ? root["port"].asString()
			: ss.str();
	this->port = atol(this->config["port"].c_str());
	ss.str("");
	ss << this->workers;
	this->config["workers"]
			= root.isMember("workers") ? root["workers"].asString() : ss.str();
	this->workers = atoi(this->config["workers"].c_str());
	ss.str("");
	ss << this->pool_size;
	this->config["pool_size"]
			= mysql_json.isMember("pool_size") ? mysql_json["pool_size"].asString()
					: ss.str();
	this->pool_size = atoi(this->config["pool_size"].c_str());
	fs.close();

}
void Server::readUserListFile(const char *user_list_file) {
	this->user_list.clear();
	std::fstream fs;
	fs.open(user_list_file, std::ios_base::in);
	if (!fs.is_open()) {
		throw ServerException(ServerException::OPEN_ULIST_FILE_FAIL);
	}
	Json::Value root;
	if (!this->jsonReader->parse(fs, root, false)) {
		fs.close();
		throw ServerException(ServerException::PARSE_ULIST_FILE_FAIL);
	}
	if (!root.isArray()) {
		fs.close();
		throw ServerException(ServerException::PARSE_ULIST_FILE_FAIL);
	}
	for (unsigned int i = 0; i < root.size(); i++) {
		Json::Value v = root[i];
		if (v.isObject()) {
			this->user_list[v["user"].asString()] = v["pass"].asString();
		}
	}
	fs.close();
}

bool Server::setNoBlock(int fd) {
	if (!fd) {
		return false;
	}
	int flags = fcntl(fd, F_GETFL, 0);
	if (-1 == flags) {
		return false;
	}
	flags |= O_NONBLOCK;
	int s = fcntl(fd, F_SETFL, flags);
	if (-1 == s) {
		return false;
	}
	return true;
}

bool Server::setReuseaddr(int fd) {
	if (!fd) {
		return false;
	}
	int opt = 1;
	if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt))) {
		return false;
	}
	return true;
}

bool Server::setNoReuseaddr(int fd) {
	if (!fd) {
		return false;
	}
	int opt = 0;
	if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt))) {
		return false;
	}
	return true;
}

void Server::run() {
	//Create socket;
	this->socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	int opt = 1;
	setsockopt(this->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt));
	int port = this->port;
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	if (bind(this->socket_fd, (struct sockaddr*) &sin, sizeof(struct sockaddr))
			== -1) {
		throw ServerException(ServerException::SOCKET_PORT_INUSE);
	}
	/* Step 2: Listen; */
	if (listen(this->socket_fd, MPOOL_EPOLL_LISTEN) == -1) {
		throw ServerException(ServerException::SOCKET_LISTEN_FAIL);
	}
	if (!this->setNoBlock(this->socket_fd)) {
		throw ServerException(ServerException::SOCKET_NOBLOCK_FAIL);
	}
	struct epoll_event ev;
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = this->socket_fd;
	this->epoll_fd = epoll_create(MPOOL_EPOLL_LISTEN);
	struct epoll_event events[MPOOL_EPOLL_LISTEN];
	if (this->epoll_fd == -1) {
		throw ServerException(ServerException::EPOLL_CREATE_FAIL);
	}
	if (-1 == epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->socket_fd, &ev)) {
		throw ServerException(ServerException::EPOLL_CTL_FAIL);
	}
	openlog(MPOOL_LOG_IDENT, LOG_CONS | LOG_PID, LOG_USER);
	syslog(LOG_INFO, "Server Started");
#ifdef DEBUG
	std::cout<<"Starting the manager"<<std::endl;
#endif
	this->manager->start();
#ifdef DEBUG
	std::cout<<"Manager started"<<std::endl;
#endif
#ifdef DEBUG
	std::cout<<"Starting GC thread"<<std::endl;
#endif
	this->startGc();
#ifdef DEBUG
	std::cout<<"GC thread started"<<std::endl;
#endif
	this->running = true;
	while (this->running) {
		int nfds = epoll_wait(this->epoll_fd, events, MPOOL_EPOLL_LISTEN, 200);
		if (nfds == -1) {
#ifdef DEBUG
			std::cout<<"(Server)epoll wait error:"<<strerror(errno)
			<< std::endl;
#endif
			continue;
		}
		int n = 0;
		for (n = 0; n < nfds; n++) {
			Client *client = NULL;
			std::map<int, MPool::Client*>::iterator it = this->clients->find(
					events[n].data.fd);
			if (it != this->clients->end()) {
				client = it->second;
			}
			if (events[n].data.fd == this->socket_fd) {
				//New connection;
				struct sockaddr_in new_sin;
				socklen_t new_sin_len = sizeof(struct sockaddr);
#ifdef DEBUG
				std::cout<<"(Server)New connection, try to accept"<<std::endl;
#endif
				while (1) {
					int new_socket = accept(this->socket_fd,
							(struct sockaddr*) &new_sin, &new_sin_len);
					if (-1 == new_socket) {
						// All connections have been established;
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							break;
						} else {
							// Write log;
							break;
						}
					}
					if (!this->setNoBlock(new_socket)) {
						throw ServerException(
								ServerException::SOCKET_NOBLOCK_FAIL);
					}
#ifdef DEBUG
					std::cout<<"(Server)New connection is established, FD:"<<new_socket<<std::endl;
#endif
					memset(&ev, '\0', sizeof(struct epoll_event));
					ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
					ev.data.fd = new_socket;
					if (-1 == epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD,
							new_socket, &ev)) {
						std::cout << "(Server)epoll add error:" << strerror(
								errno) << std::endl;
					}
				}
			} else {
#ifdef DEBUG
				std::cout<<"(Server)#Event: EPOLLIN="<<(events[n].events & EPOLLIN)<<" ,FD:"<< events[n].data.fd<<std::endl;
				std::cout<<"(Server)#Event: EPOLLOUT="<<(events[n].events & EPOLLOUT)<<" ,FD:"<< events[n].data.fd<<std::endl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
				std::cout<<"(Server)#Event: EPOLLRDHUP="<<(events[n].events & EPOLLRDHUP)<<" ,FD:"<< events[n].data.fd<<std::endl;
#endif
				std::cout<<"(Server)#Event: EPOLLPRI="<<(events[n].events & EPOLLPRI)<<" ,FD:"<< events[n].data.fd<<std::endl;
				std::cout<<"(Server)#Event: EPOLLERR="<<(events[n].events & EPOLLERR)<<" ,FD:"<< events[n].data.fd<<std::endl;
				std::cout<<"(Server)#Event: EPOLLHUP="<<(events[n].events & EPOLLHUP)<<" ,FD:"<< events[n].data.fd<<std::endl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,2)
				std::cout<<"(Server)#Event: EPOLLONESHOT="<<(events[n].events & EPOLLONESHOT)<<" ,FD:"<< events[n].data.fd<<std::endl;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
				std::cout<<"(Server)#Event: EPOLLWAKEUP="<<(events[n].events & EPOLLWAKEUP)<<" ,FD:"<< events[n].data.fd<<std::endl;
#endif
#endif
				bool normal_end = false;
				bool error_end = false;
				if ((events[n].events & EPOLLERR) && !(events[n].events
						& EPOLLIN)) {
					std::cout << "(Server)epoll wait event error, FD:"
							<< events[n].data.fd << ", Error:" << strerror(
							errno) << std::endl;
					if (EAGAIN != errno && ENOTSUP != errno) {
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
					} else {
						std::cout
								<< "(Server)EAGAIN/ENOTSUP for epoll wait, sleep a while & try again, FD:"
								<< events[n].data.fd << std::endl;
					}
					continue;
				}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
				if ((events[n].events & EPOLLRDHUP) && !(events[n].events
						& EPOLLIN)) {
					// Connection closed by client;
#ifdef DEBUG
					std::cout<<"(Server)Connection closed by client: "<<events[n].data.fd<<std::endl;
#endif
					if (!client) {
						// Garbage Connection;
						this->closeSocket(events[n].data.fd);
					} else {
						if (!client->isBusy() && client->getWorks() <= 0) {
							this->normalEnd(client);
						}
					}
					continue;
				}
#endif
				if (events[n].events & EPOLLIN) {
					// Read all data first;
					std::string buffer = "";
					buffer.clear();
#ifdef DEBUG
					std::cout<<"(Server)Start reading, FD:"<<events[n].data.fd<<std::endl;
#endif
					bool client_close = false;
					while (!normal_end) {
						char buf[257];
						memset(buf, 0, sizeof(buf));
						int rv = read(events[n].data.fd, buf, 256);
						switch (rv) {
						case 0:
							// Connection closed by client;
							normal_end = true;
							if (events[n].events & EPOLLRDHUP) {
								client_close = true;
							}
							break;
						case -1:
							if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
								// All data has been read;
								normal_end = true;
							} else {
								std::cout << "(Server)socket read error:"
										<< strerror(errno) << std::endl;
								normal_end = true;
								error_end = true;
							}
							break;
						default:
							buffer += buf;
							break;
						}

					}
#ifdef DEBUG
					std::cout<<"(Server)Read length:"<<buffer.size()<<std::endl;
#endif
					// Package Length, first 16 characters;
					if (!buffer.empty() && buffer.size() < 16) {
						// Wrong package length;
#ifdef DEBUG
						std::cout<<"(Server)Wrong package length, FD:"<<events[n].data.fd<<std::endl;
						std::cout<<buffer<<std::endl;
#endif
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					// Connection closed by client;
					if (buffer.empty() || (error_end && buffer.empty())
							|| (client_close && buffer.empty())) {
#ifdef DEBUG
						std::cout<<"(Server)Connection closed by client, FD:"<<events[n].data.fd<<std::endl;
#endif
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					const std::string len_str = buffer.substr(0, 16);
					ssize_t jsonLength = atol(len_str.c_str());
					std::string jsonBuffer = buffer.substr(16);
					if (!jsonLength || (ssize_t) jsonBuffer.size()
							!= jsonLength) {
						// Wrong package length;
#ifdef DEBUG
						std::cout<<"(Server)Package length is not matched with buffer, FD:"<<events[n].data.fd<<std::endl;
#endif
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					// Right trim;
					jsonBuffer.erase(jsonBuffer.find_last_not_of(" \n\r\t") + 1);
#ifdef DEBUG
					std::cout<<"(Server)JSON:"<<jsonBuffer<<std::endl;
#endif
					if (jsonBuffer.empty()) {
#ifdef DEBUG
						std::cout<<"(Server)Wrong Data, end of socket, FD:"<<events[n].data.fd<<std::endl;
						if(error_end) {
							std::cout<<"(Server)Error End, FD:"<<events[n].data.fd<<std::endl;
							std::cout<<"(Server)Socket Error:"<<strerror(errno)
							<< std::endl;
						}
#endif
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
#ifdef DEBUG
					std::cout<<"(Server)Data received>"<<buffer<<std::endl;
#endif
					Json::Value root;

					bool parsed = this->jsonReader->parse(jsonBuffer, root,
							false);

					if (!parsed || !root.isMember("type") || !root.isMember(
							"protocol_version")) {
#ifdef DEBUG
						std::cout<<"(Server)Fail to parse JSON, Wrong data, drop it"<<std::endl;
#endif
						// Wrong Data;
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					if (root["protocol_version"].asString().compare(
							MPOOL_PROTOCOL_VERSION)) {
#ifdef DEBUG
						std::cout<<"(Server)Wrong protocol, drop it"<<std::endl;
#endif
						// Wrong Protocol Version;
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					if (this->clients->size() >= this->max_connections) {
#ifdef DEBUG
						std::cout<<"(Server)Too many connections"<<std::endl;
#endif
						if (!client) {
							this->closeSocket(events[n].data.fd);
						} else {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					if (!root["type"].asString().compare("query")) {
#ifdef DEBUG
						std::cout<<"(Server)Query Action"<<std::endl;
#endif
						// Normal query;
						if (!client) {
#ifdef DEBUG
							std::cout<<"(Creating new client)"<<std::endl;
#endif
							client = new Client(root["username"].asString());
							if (!client) {
								syslog(LOG_ERR,
										"Fail to collect memory to create client");
								this->closeSocket(events[n].data.fd);
								continue;
							}
							client->setSocket(events[n].data.fd);
							DB *db_con = this->db_pool->allocDB();
							if (!db_con) {
#ifdef DEBUG
								std::cout<<"(Creating new client)Fail to get db connection from pool, FD:"<<events[n].data.fd<<std::endl;
#endif
								syslog(LOG_ERR,
										"Fail to get db connection from pool");
								delete client;
								this->closeSocket(events[n].data.fd);
								continue;
							}
							client->setDBConnection(db_con);
							(*this->clients)[events[n].data.fd] = client;

						}
						if (!this->clientQueryAction(client, root)) {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						if (client_close) {
#ifdef DEBUG
							std::cout<<"(Server)Client closed the connection after send SQL, FD"<<events[n].data.fd<<std::endl;
#endif
							// Wrong Data;
							if (!client) {
								this->closeSocket(events[n].data.fd);
							} else {
								if (!client->isBusy() && client->getWorks()
										<= 0) {
									this->normalEnd(client);
								}
							}
						}
						continue;
					}
					if (!root["type"].asString().compare("status")) {
						// Get server running information;
						if (!client) {
							this->closeSocket(events[n].data.fd);
							continue;
						}
						if (!this->clientServerStatusAction(client, root)) {
							if (!client->isBusy() && client->getWorks() <= 0) {
								this->normalEnd(client);
							}
						}
						continue;
					}
					// Garbage message, go to gc;
					if (!client) {
						this->closeSocket(events[n].data.fd);
					} else {
						if (!client->isBusy() && client->getWorks() <= 0) {
							this->normalEnd(client);
						}
					}
				}
			}
		}
	}
	close(this->epoll_fd);
	syslog(LOG_INFO, "Server end without error");
	closelog();
}

void Server::stop() {
	this->running = false;
	this->gc_running = false;
}

void Server::startGc() {
	if (this->gc_tid) {
		if (0 == pthread_kill(this->gc_tid, 0)) {
			// Kill the thread if still alive;
			pthread_kill(this->gc_tid, SIGKILL);
		}
		this->gc_tid = 0;
	}
	if (pthread_create(&this->gc_tid, 0, MPool::Server::threadStart, this) != 0) {
		throw ServerException();
	}
	pthread_detach(this->gc_tid);
	this->gc_running = true;
}

void Server::gc() {
	//Garbage Collection;
	while (this->gc_running) {
#ifdef DEBUG
		std::cout<<"(GC Loop Start)"<<std::endl;
		if(!this->clients->empty()) {
			std::cout<<"(Server)Clients:"<<this->clients->size()<<std::endl;
		}
#endif
		for (std::map<int, MPool::Client*>::iterator it =
				this->clients->begin(); it != this->clients->end(); it++) {
			Client *client = it->second;
#ifdef DEBUG
			std::cout<<"(Server)Client:"<<client->getSocket()<<std::endl;
			std::cout<<"Is timeout? "<<client->isTimeout()<<std::endl;
			std::cout<<"Is busy? "<<client->isBusy()<<std::endl;
			std::cout<<"Pending works: "<<client->getWorks()<<std::endl;
#endif
			if (client->isTimeout() && !client->isBusy() && client->getWorks()
					<= 0) {
				pthread_mutex_lock(&this->client_mutex);
#ifdef DEBUG
				std::cout<<"(Server)Garbage collection for client:"<<client->getSocket()<<std::endl;
#endif
				if (this->isSocketVal(client->getSocket())) {
					if (-1 == close(client->getSocket())) {
#ifdef DEBUG
						std::cout<<"(Server)Fail to close socket, FD: "<<client->getSocket()<<", Error: "<<strerror(errno)<<std::endl;
#endif
					}
				}
				this->db_pool->freeDB((it->second)->getDBConnection());
				delete it->second;
				this->clients->erase(it);
				pthread_mutex_unlock(&this->client_mutex);
			}
		}
#ifdef DEBUG
		std::cout<<"(GC Loop End)"<<std::endl;
		if(!this->clients->empty()) {
			std::cout<<"(Server)Clients:"<<this->clients->size()<<std::endl;
		}
#endif
		sleep(30);
	}
}

bool Server::clientQueryAction(Client *client, Json::Value root) {
	if (!client) {
#ifdef DEBUG
		std::cout<<"(Server)Call Query Action without Client, Drop it"<<std::endl;
#endif
		return false;
	}
	if (!root.isMember("username") || !root.isMember("password")) {
		return false;
	}
	std::map<std::string, std::string>::iterator it = this->user_list.find(
			root["username"].asString());
	if (it == this->user_list.end()) {
		this->clientMessage(client, "AUTH_FAIL", "F001",
				"Authorization fail, incorrect user or password");
		return false;
	}
	if (it->second.compare(root["password"].asString())) {
		this->clientMessage(client, "AUTH_FAIL", "F001",
				"Authorization fail, incorrect user or password");
		return false;
	}
	if (!root.isMember("sql")) {
		return false;
	}
#ifdef DEBUG
	std::cout<<"(Server)Push SQL into Client"<<std::endl;
#endif
	client->pushSQL(root["sql"].asString());
#ifdef DEBUG
	std::cout<<"(Server)Push Client into pending list"<<std::endl;
#endif
	this->manager->push(client);
	return true;
}
bool Server::clientExitAction(Client *client, Json::Value root) {
	if (!client) {
		return false;
	}
	return true;
}
bool Server::clientServerStatusAction(Client *client, Json::Value root) {
	if (!client) {
		return false;
	}
	if (!root.isMember("username") || !root.isMember("password")) {
		return false;
	}
	std::map<std::string, std::string>::iterator it = this->user_list.find(
			root["username"].asString());
	if (it == this->user_list.end()) {
		this->clientMessage(client, "AUTH_FAIL", "F001",
				"Authorization fail, incorrect user or password");
		return false;
	}
	if (it->second.compare(root["password"].asString())) {
		this->clientMessage(client, "AUTH_FAIL", "F001",
				"Authorization fail, incorrect user or password");
		return false;
	}
	Json::Value data;
	data["server_version"] = MPOOL_SERVER_VERSION;
	data["clients"] = this->clients->size();
	data["workers"] = this->workers;
	std::string str_data = this->jsonWriter->write(data);
	this->clientMessage(client, "SUCCESS", "T001", "Success", str_data.c_str());
	return true;
}

void Server::closeSocket(int fd) {
	if (!fd) {
		return;
	}
	if (-1 == close(fd)) {
#ifdef DEBUG
		std::cout<<"(Server)Fail to close socket, FD: "<<fd<<", Error: "<<strerror(errno)<<std::endl;
#endif
	}
}

void Server::goToGc(Client *client) {
	if (client) {
		// GC it;
		client->setTimeout();
	}
}

void Server::normalEnd(Client *client) {
	if (client) {
		if (!client->isBusy() && client->getWorks() <= 0) {
			if (!this->clients->empty()) {
				pthread_mutex_lock(&this->client_mutex);
				std::map<int, MPool::Client*>::iterator it =
						this->clients->find(client->getSocket());
				if (it == this->clients->end()) {
					pthread_mutex_unlock(&this->client_mutex);
					return;
				}
				this->clients->erase(it);
				if (this->isSocketVal(client->getSocket())) {
					if (-1 == close(client->getSocket())) {
#ifdef DEBUG
						std::cout<<"(Server)Fail to close socket, FD: "<<client->getSocket()<<", Error: "<<strerror(errno)<<std::endl;
#endif
					}
				}
				this->db_pool->freeDB(client->getDBConnection());
				delete client;
				pthread_mutex_unlock(&this->client_mutex);
			}
		}
	}
}
}
