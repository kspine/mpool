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

#ifndef SERVER_H_
#define SERVER_H_

namespace MPool {
/**
 * @brief mPool Server
 * #Configurations:
 * max_connections
 * database
 * */
class Server {
public:
	Server();
	virtual ~Server();
	/**
	 * @brief initialize the server
	 * @param config_file: server configurations
	 * @param user_list_file: user list
	 * */
	void
	init(const char *config_file = NULL, const char *user_list_file = NULL);
	/// Reload configurations when received signal
	void reload();
	/**
	 * @brief Main loop, this method will block the program
	 * @note Throw Exception when failed
	 * */
	void run();
	void stop();
	void gc(); /// Garbage Collection;
	void startGc();
	static void* threadStart(void *t) {
		if (!t) {
			return NULL;
		}
		Server *m = (Server*) t;
		m->gc();
		return NULL;
	}
protected:
	std::list<std::string> support_protocol_versions; /// Support protocol versions
	std::map<int, MPool::Client*> *clients; /// Connected clients, use TCP socket fd as keys
	std::map<std::string, std::string> config; /// Server configurations
	std::map<std::string, std::string> user_list; // User list, username & password
	std::string config_file; /// Path of configuration file
	std::string user_list_file; /// Path of user list file
	Manager *manager; /// Process manager;
	DBPool *db_pool; /// DB Connection Pool;
	bool running; /// Running status;
	bool gc_running;
	int socket_fd;
	Json::Reader *jsonReader;
	Json::FastWriter *jsonWriter;
	unsigned long max_connections;
	unsigned int pool_size;
	unsigned int workers;
	int port;
	pthread_t gc_tid;
	pthread_mutex_t client_mutex;
	int epoll_fd;
protected:
	bool setNoBlock(int fd);
	void readConfigFile(const char *config_file = NULL);
	void readUserListFile(const char *user_list_file = NULL);
	void doCleanWorks();
	void socketMessage(int s, const char *status, const char *code,
			const char *msg, const char *data = "");
	void clientMessage(Client *client, const char *status, const char *code,
			const char *msg, const char *data = "");
	bool isSocketVal(int fd);
	bool setReuseaddr(int fd);
	bool setNoReuseaddr(int fd);
	bool clientQueryAction(Client *client, Json::Value root);
	bool clientExitAction(Client *client, Json::Value root);
	bool clientServerStatusAction(Client *client, Json::Value root);
	void closeSocket(int fd);
	void goToGc(Client *client);
	void normalEnd(Client *client);
};

}

#endif /* SERVER_H_ */
