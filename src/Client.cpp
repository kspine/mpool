/*
 * Client.cpp
 *
 *  Created on: 2017-2-16
 *      Author: test
 */

#include <string>
#include <queue>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <jsoncpp/json/json.h>
#include <my_global.h>
#include <mysql.h>
#include "include/version.h"
#include "include/DBPool.h"
#include "include/Client.h"

namespace MPool {

Client::Client(std::string username) {
	time_t now = time(0);
	this->connect_time = now;
	this->last_hb_time = now;
	this->username = username;
	this->token = "";
	pthread_mutex_init(&this->work_mutex, NULL);
	pthread_mutex_init(&this->sql_mutex, NULL);
	this->jsonReader = new Json::Reader(Json::Features::strictMode());
	this->jsonWriter = new Json::FastWriter();
	this->failed_queries = 0;
	this->success_queries = 0;
	this->queries = 0;
	this->db_con = NULL;
	this->socket = 0;
	this->works = 0;
}

int Client::getSocket() {
	return this->socket;
}

Client::~Client() {
	pthread_mutex_destroy(&this->work_mutex);
	pthread_mutex_destroy(&this->sql_mutex);
	delete this->jsonWriter;
	delete this->jsonReader;
}

void Client::setSocket(int s) {
	this->socket = s;
}

time_t Client::getConnectTime() {
	return this->connect_time;
}

void Client::lastActive() {
	time_t now = time(0);
	this->last_hb_time = now;
}

time_t Client::getLastHbTime() {
	return this->last_hb_time;
}

std::string Client::getToken() {
	return this->token;
}

std::string Client::generateToken() {
	this->token = "";
	srandom(time(0));
	for (int i = 0; i < 64; i++) {
		char c = 0;
		unsigned char seed = random() % 4;
		unsigned char n = 0;
		switch (seed) {
		case 0:
			//Number;
			n = random() % 10;
			c = 0x30 + n;
			break;
		case 1:
			//Upper case;
			n = random() % 26;
			c = 0x41 + n;
			break;
		case 2:
			//Lower case;
			n = random() % 26;
			c = 0x61 + n;
			break;
		case 3:
			c = '=';
			break;
		}
		this->token += c;
	}
	this->token = token;
	return this->token;
}

std::string Client::getUsername() {
	return this->username;
}

void Client::setDBConnection(DB *db_con) {
	this->db_con = db_con;
}

DB* Client::getDBConnection() {
	return this->db_con;
}

void Client::pushSQL(std::string sql) {
	this->lastActive();
	pthread_mutex_lock(&this->sql_mutex);
	this->sqls.push(sql);
	this->works++;
#ifdef DEBUG
	std::cout<<"Pushed SQL:"<<sql<<", works:"<<this->works<<std::endl;
#endif
	pthread_mutex_unlock(&this->sql_mutex);
}

void Client::doWork() {
	if (this->sqls.empty()) {
		return;
	}
#ifdef DEBUG
	std::cout<<"Starting work"<<std::endl;
#endif
	//pthread_mutex_lock(&this->work_mutex);
	pthread_mutex_lock(&this->sql_mutex);
	std::string sql = this->sqls.front();
	this->sqls.pop();
	pthread_mutex_unlock(&this->sql_mutex);
	Json::Value root;
	Json::Value data;
	Json::Value row;
	std::string status = "SUCCESS";
	std::string code = "T001";
	std::string message = "";
	bool hasResult = false;
	this->queries++;
	// Left trim;
	sql.erase(0, sql.find_first_not_of(" \n\r\t"));
	if (sql.empty()) {
		pthread_mutex_unlock(&this->work_mutex);
		this->works--;
		return;
	}
	DBResult *result = this->db_con->query(sql);
	if (result) {
		hasResult = true;
		for (DBDataSet::iterator it = result->data.begin();
				it != result->data.end(); it++) {
			if (!row.empty()) {
				row.clear();
			}
			DBDataRow dRow = *it;
			for (DBDataRow::iterator rit = dRow.begin(); rit != dRow.end();
					rit++) {
				row[rit->first] = rit->second;
#ifdef DEBUG
				//std::cout<<"(Client)Query Result, "<<rit->first<<"->"<<rit->second<<std::endl;
#endif
			}
			data.append(row);
		}
		this->db_con->freeResult(result);
		this->success_queries++;
	} else {
		if (0 != this->db_con->getErrno()) {
			// Log error;
			code = "F001";
			message = this->db_con->getError();
			this->failed_queries++;
#ifdef DEBUG
			std::cout<<"(Client)Query error:"<<this->db_con->getError()<<std::endl;
#endif
		} else {
			this->success_queries++;
		}
	}
	// Send result to client;
	root["protocol_version"] = MPOOL_PROTOCOL_VERSION;
	root["status"] = status;
	root["code"] = code;
	root["message"] = message;
	if (!hasResult) {
		root["data"] = "";
	} else {
		root["data"] = data;
	}
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
	ssize_t sent = 0;
	while (sent < (ssize_t) str.length()) {
		ssize_t psent = write(this->socket, str.substr(sent).c_str(),
				str.substr(sent).length());
		if (-1 == psent) {
			if ( EAGAIN == errno) {
				//Sleep a while and try again;
				psent = 0;
				usleep(50);
			} else {
				std::cout << "(Client) Send Data Error: " << strerror(errno)
						<< std::endl;
				close(this->socket);
				break;
			}
		}
		sent += psent;
	}
#ifdef DEBUG
	std::cout<<"(Client) "<<sent<<"/"<<str.length()<<" has been sent"<<std::endl;
#endif
	this->works--;
	//pthread_mutex_unlock(&this->work_mutex);
#ifdef DEBUG
	std::cout<<"(Client)Work done"<<std::endl;
	std::cout<<"[Client]Pending Works:"<<this->works<<std::endl;
	std::cout<<"[Client]Queries:"<<this->queries<<std::endl;
	std::cout<<"[Client]Success:"<<this->success_queries<<std::endl;
	std::cout<<"[Client]Fail:"<<this->failed_queries<<std::endl;
#endif
}

void Client::wait() {
	pthread_mutex_lock(&this->work_mutex);
	pthread_mutex_unlock(&this->work_mutex);
}

bool Client::isTimeout() {
	time_t now = time(0);
	if (now - this->last_hb_time > MPOOL_CLIENT_TIMEOUT) {
		this->setTimeout();
		return true;
	}
	return false;
}

void Client::setTimeout() {
	if (!this->isBusy()) {
		pthread_mutex_lock(&this->sql_mutex);
		while (!this->sqls.empty()) {
			this->sqls.pop();
		}
		pthread_mutex_unlock(&this->sql_mutex);
		this->last_hb_time = 0;
	}
}

bool Client::isBusy() {
	if (0 == pthread_mutex_trylock(&this->work_mutex)) {
		pthread_mutex_unlock(&this->work_mutex);
		return false;
	}
	return true;
}

unsigned long Client::getWorks() {
	return this->works;
}

}
