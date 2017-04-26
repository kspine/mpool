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

#ifndef MANAGER_H_
#define MANAGER_H_

namespace MPool {

class Worker {
protected:
	pthread_t tid;
	/// Running status: I - idle, N - stopped, B - busy
	char status;
	Client *client;
	pthread_mutex_t mutex;
	time_t last_run_time;
public:
	Worker();
	~Worker();
	void start();
	void run();
	void stop();
	pthread_t getTid();
	void setTid(pthread_t tid);
	char getStatus();
	void setClient(Client *c);
	time_t getLastRunTime();
	static void* threadStart(void *t) {
		if (!t) {
			return NULL;
		}
		Worker *m = (Worker*) t;
		//m->setTid(pthread_self());
		m->run();
		return NULL;
	}
};

class Manager {
public:
	Manager(unsigned int workers = 4);
	virtual ~Manager();
	/**
	 * @brief Push client into pending queue;
	 * @param client, the query client;
	 * @return true on sucess, false on fail;
	 * */
	bool push(Client *client);
	/**
	 * @brief start processing thread;
	 * */
	void start();
	/**
	 * @brief blocked running;
	 * */
	void run();
	/**
	 * @brief stop running;
	 * */
	void stop();
	pthread_t getTid();
	void setTid(pthread_t tid);
	/**
	 * @brief blocked running, used for pthread;
	 * @param t, the this pointer;
	 * */
	static void* threadStart(void *t) {
		if (!t) {
			return NULL;
		}
		Manager *m = (Manager*) t;
		//m->setTid(pthread_self());
		m->run();
		return NULL;
	}
protected:
	pthread_t tid;
	unsigned nWorkers;
	Worker **workers;
	std::queue<Client*> pending; /// Pending process queries;
	bool running;
	pthread_mutex_t mutex;
};

}

#endif /* MANAGER_H_ */
