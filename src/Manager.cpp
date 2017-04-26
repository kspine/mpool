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
#include <iostream>
#include <sstream>
#include <list>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <jsoncpp/json/json.h>
#include <my_global.h>
#include <mysql.h>
#include "include/version.h"
#include "include/DBPool.h"
#include "include/Client.h"
#include "include/Manager.h"
#include "include/ServerException.h"

namespace MPool {

Worker::Worker() {
#ifdef DEBUG
	std::cout<<"[Worker] Constructor Start"<<std::endl;
#endif
	this->status = 'N';
	this->client = 0;
	this->tid = 0;
#ifdef DEBUG
	std::cout<<"[Worker] Constructor end"<<std::endl;
#endif
	this->last_run_time = 0;
	pthread_mutex_init(&this->mutex, NULL);
}

Worker::~Worker() {
	pthread_mutex_destroy(&this->mutex);
}

void Worker::setTid(pthread_t tid) {
	this->tid = tid;
}

void Worker::stop() {
	this->status = 'N';
	if (0 == pthread_kill(this->tid, 0)) {
		pthread_kill(this->tid, SIGKILL);
	}
	this->tid = 0;
}

void Worker::run() {
#ifdef DEBUG
	std::cout<<"[Worker]Started"<<std::endl;
#endif
	while (this->status != 'N') {
		this->last_run_time = time(0);
		if (this->status == 'B' && this->client) {
#ifdef DEBUG
			std::cout<<"[Worker]Starting a work"<<std::endl;
#endif
			this->client->doWork();
			// Clean works;
			pthread_mutex_lock(&this->mutex);
			this->client = 0;
			this->status = 'I';
			pthread_mutex_unlock(&this->mutex);
#ifdef DEBUG
			std::cout<<"[Worker]done a work"<<std::endl;
#endif
		} else {
			usleep(50);
		}
	}
#ifdef DEBUG
	std::cout<<"[Worker]Stopped"<<std::endl;
#endif
}

void Worker::setClient(Client *c) {
	pthread_mutex_lock(&this->mutex);
	this->status = 'B';
	this->client = c;
	pthread_mutex_unlock(&this->mutex);
}

void Worker::start() {
	if (this->tid) {
		if (0 == pthread_kill(this->tid, 0)) {
			// Kill the thread if still alive;
#ifdef DEBUG
			std::cout<<"[Worker] Killing running thread "<<this->tid<<std::endl;
#endif
			pthread_kill(this->tid, SIGKILL);
		}
		this->tid = 0;
	}
#ifdef DEBUG
	std::cout<<"[Worker] Creating new thread"<<std::endl;
#endif
	if (pthread_create(&this->tid, 0, MPool::Worker::threadStart, this) != 0) {
#ifdef DEBUG
		std::cout<<"[Worker] Cannot create new thread"<<std::endl;
#endif
		throw ServerException();
	}
#ifdef DEBUG
	std::cout<<"[Worker] Switching to the new thread"<<std::endl;
#endif
	pthread_detach(this->tid);
#ifdef DEBUG
	std::cout<<"[Worker] Set idle status"<<std::endl;
#endif
	this->status = 'I';
#ifdef DEBUG
	std::cout<<"[Worker] started"<<std::endl;
#endif
}

char Worker::getStatus() {
	return this->status;
}

time_t Worker::getLastRunTime() {
	return this->last_run_time;
}

Manager::Manager(unsigned int workers) {
	this->nWorkers = workers == 0 ? 4 : workers;
	this->tid = 0;
	this->workers = new Worker*[workers]();
	pthread_mutex_init(&this->mutex, NULL);
	// Start workers;
#ifdef DEBUG
	std::cout<<"Starting workers"<<std::endl;
#endif
	for (unsigned i = 0; i < this->nWorkers; i++) {
		this->workers[i] = new Worker();
#ifdef DEBUG
		std::cout<<"Starting worker: "<<i<<std::endl;
#endif
		this->workers[i]->start();
#ifdef DEBUG
		std::cout<<"worker "<<i<<" has been started"<<std::endl;
#endif
	}
#ifdef DEBUG
	std::cout<<"Workers has been started"<<std::endl;
#endif
	this->running = false;
}

Manager::~Manager() {
	// Stop workers;
	pthread_mutex_destroy(&this->mutex);
}

void Manager::setTid(pthread_t tid) {
	this->tid = tid;
}

bool Manager::push(Client *client) {
	pthread_mutex_lock(&this->mutex);
	this->pending.push(client);
	pthread_mutex_unlock(&this->mutex);
	return true;
}

void Manager::stop() {
#ifdef DEBUG
	std::cout<<"Cleaning the pending quires"<<std::endl;
#endif
	while (!this->pending.empty()) {
		this->pending.pop();
	}
	this->running = false;
#ifdef DEBUG
	std::cout<<"Stopping the workers"<<std::endl;
#endif
	for (unsigned i = 0; i < this->nWorkers; i++) {
		this->workers[i]->stop();
		delete this->workers[i];
	}
#ifdef DEBUG
	std::cout<<"Free workers"<<std::endl;
#endif
	delete[] this->workers;
	if (0 == pthread_kill(this->tid, 0)) {
		// Kill the thread if still alive;
		pthread_kill(this->tid, SIGKILL);
	}
	this->tid = 0;
}

void Manager::run() {
	while (this->running) {
		if (!this->pending.empty()) {
#ifdef DEBUG
			std::cout<<"[Manager]Pending clients: "<<this->pending.size()<<std::endl;
#endif
			for (unsigned int i = 0; i < this->nWorkers; i++) {
				time_t now = time(0);
				if (now
						- this->workers[i]->getLastRunTime() > MPOOL_CLIENT_TIMEOUT) {
					// Zombie worker, restart;
					this->workers[i]->stop();
					this->workers[i]->start();
				}
				if (this->workers[i]->getStatus() == 'I') {
					pthread_mutex_lock(&this->mutex);
					if (0 == pthread_kill(this->tid, 0)) {
#ifdef DEBUG
						std::cout<<"[Manager]Worker "<<i<<" is alive, push task"<<std::endl;
#endif
					} else {
#ifdef DEBUG
						std::cout<<"[Manager]Worker "<<i<<" is zombie, restart it"<<std::endl;
#endif
						this->workers[i]->stop();
						this->workers[i]->start();
					}
					Client *c = this->pending.front();
					this->pending.pop();
					if (!c->isBusy()) {
#ifdef DEBUG
						std::cout<<"[Manager]Client is free, associate it with Worker "<<i<<std::endl;
#endif
						this->workers[i]->setClient(c);
					} else {
#ifdef DEBUG
						std::cout<<"[Manager]Client is busy, push it to the end of pending queue"<<std::endl;
#endif
						this->pending.push(c);
					}
#ifdef DEBUG
					std::cout<<"[Manager]Done a assignment"<<std::endl;
#endif
					pthread_mutex_unlock(&this->mutex);
					break;
				}
			}
		} else {
			usleep(50);
		}
	}
}

void Manager::start() {
	if (this->tid) {
		if (0 == pthread_kill(this->tid, 0)) {
			// Kill the thread if still alive;
			pthread_kill(this->tid, SIGKILL);
		}
		this->tid = 0;
	}
	if (pthread_create(&this->tid, 0, MPool::Manager::threadStart, this) != 0) {
		throw ServerException();
	}
	pthread_detach(this->tid);
	this->running = true;
}

}
