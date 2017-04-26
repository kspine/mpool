/*
 * DBPool.cpp
 *
 *  Created on: 2017-3-22
 *      Author: test
 */
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <queue>
#include <vector>
#include <map>
#include <exception>
#include <pthread.h>
#include <my_global.h>
#include <mysql.h>
#include "include/version.h"
#include "include/DBPool.h"

namespace MPool {
DB::DB(unsigned long id, MYSQL *conn) {
	this->real_conn = conn;
	this->db_errno = 0;
	this->db_error = "";
	this->affected_rows = 0;
	this->id = id;
	pthread_mutex_init(&this->mutex, NULL);
}
DB::~DB() {
	mysql_close(this->real_conn);
	pthread_mutex_destroy(&this->mutex);
}
unsigned long DB::getId() {
	return this->id;
}
void DB::setId(unsigned long id) {
	this->id = id;
}
unsigned int DB::getErrno() {
	return this->db_errno;
}
std::string DB::getError() {
	return this->db_error;
}
DBResult* DB::query(std::string sql) {
	if (!this->real_conn || sql.empty()) {
#ifdef DEBUG
		std::cout<<"(DB)Empty query, exit"<<std::endl;
#endif
		return NULL;
	}
	pthread_mutex_lock(&this->mutex);
	this->db_errno = 0;
	this->db_error = "";
	if (0 != mysql_ping(this->real_conn)) {
#ifdef DEBUG
		std::cout<<"(DB)Lost connection, exit"<<std::endl;
#endif
		this->db_errno = mysql_errno(this->real_conn);
		this->db_error = mysql_error(this->real_conn);
		pthread_mutex_unlock(&this->mutex);
		return NULL;
	}
	int qz = mysql_query(this->real_conn, sql.c_str());
	if (0 != qz) {
		this->db_errno = mysql_errno(this->real_conn);
		this->db_error = mysql_error(this->real_conn);
		pthread_mutex_unlock(&this->mutex);
		return NULL;
	}
	this->affected_rows = mysql_affected_rows(this->real_conn);
	MYSQL_RES *res;
	MYSQL_ROW row;
	res = mysql_store_result(this->real_conn);
	if (!res) {
		this->db_errno = mysql_errno(this->real_conn);
		this->db_error = mysql_error(this->real_conn);
		pthread_mutex_unlock(&this->mutex);
		return NULL;
	}
	DBResult *result = new DBResult();
	if (!result) {
		//Not enough memory to allocate;
		pthread_mutex_unlock(&this->mutex);
		return NULL;
	}
	//my_ulonglong rows = mysql_num_rows(res);
	unsigned int fields = mysql_num_fields(res);
	while ((row = mysql_fetch_row(res))) {
		std::map<std::string, std::string> data_row;
		for (unsigned int i = 0; i < fields; i++) {
			MYSQL_FIELD *field = mysql_fetch_field_direct(res, i);
			std::string name = field->name;
			std::string value = row[i] ? row[i] : "";
			data_row[name] = value;
#ifdef DEBUG
			//std::cout<<"(DB)Query Result, "<<name<<"->"<<value<<std::endl;
#endif
		}
		result->data.push_back(data_row);
	}
	mysql_free_result(res);
	pthread_mutex_unlock(&this->mutex);
	return result;
}
void DB::freeResult(DBResult *result) {
	if (!result) {
		return;
	}
	delete result;
}
DBPool::DBPool() {
	this->host = "";
	this->user = "";
	this->pass = "";
	this->database = "";
	this->port = 0;
	this->min_alives = 4;
	this->current_id = 0;
	pthread_mutex_init(&this->mutex, NULL);
	if (-1 == mysql_library_init(0, NULL, NULL)) {
		//Throw Exception;
	}
}
DBPool::~DBPool() {
	this->doCleanWorks();
	mysql_thread_end();
	mysql_library_end();
	pthread_mutex_destroy(&this->mutex);
}
void DBPool::doCleanWorks() {
	if (!this->idle.empty()) {
		for (std::vector<MPool::DB*>::iterator it = this->idle.begin();
				it != this->idle.end(); it++) {
			delete *it;
			this->idle.erase(it);
		}
	}
	if (!this->busy.empty()) {
		for (std::vector<MPool::DB*>::iterator it = this->busy.begin();
				it != this->busy.end(); it++) {
			delete *it;
			this->busy.erase(it);
		}

	}
}
DB* DBPool::allocDB() {
	if (!this->idle.empty()) {
		pthread_mutex_lock(&this->mutex);
		DB *db = this->idle.front();
		this->idle.erase(this->idle.begin());
		this->busy.push_back(db);
		pthread_mutex_unlock(&this->mutex);
		return db;
	} else {
		DB *db = this->newDB();
		if (!db) {
			return NULL;
		}
		pthread_mutex_lock(&this->mutex);
		this->busy.push_back(db);
		pthread_mutex_unlock(&this->mutex);
		return db;
	}
	return NULL;
}
void DBPool::freeDB(DB *db) {
	if (!db) {
		return;
	}
#ifdef DEBUG
	std::cout<<"(DB Pool)Starting free DB:"<<db->getId()<<std::endl;
#endif
	if (!this->busy.empty()) {
		for (std::vector<MPool::DB*>::iterator it = this->busy.begin();
				it != this->busy.end(); it++) {
			if (db->getId() == (*it)->getId()) {
				pthread_mutex_lock(&this->mutex);
#ifdef DEBUG
				std::cout<<"(DB Pool)Removing DB from busy pool:"<<db->getId()<<std::endl;
#endif
				this->busy.erase(it);
#ifdef DEBUG
				std::cout<<"(DB Pool)Done"<<std::endl;
#endif
				pthread_mutex_unlock(&this->mutex);
				break;
			}
		}
	}
	if (this->idle.size() < this->min_alives) {
		pthread_mutex_lock(&this->mutex);
#ifdef DEBUG
		std::cout<<"(DB Pool)Put it into idle pool:"<<db->getId()<<std::endl;
#endif
		this->idle.push_back(db);
#ifdef DEBUG
		std::cout<<"(DB Pool)Done"<<std::endl;
#endif
		pthread_mutex_unlock(&this->mutex);
	} else {
#ifdef DEBUG
		std::cout<<"(DB Pool)Active alive is greater than "<<this->min_alives<<", free it:"<<db->getId()<<std::endl;
#endif
		delete db;
#ifdef DEBUG
		std::cout<<"(DB Pool)Done"<<std::endl;
#endif
	}
}
DB* DBPool::newDB() {
	MYSQL *conn = mysql_init(NULL);
	if (!conn) {
		return NULL;
	}
	if (!mysql_real_connect(conn, this->host.c_str(), this->user.c_str(),
			this->pass.c_str(), this->database.c_str(), this->port,
			NULL, 0)) {
		mysql_close(conn);
		return NULL;
	}
	/* Set Connection Timeout; */
	int con_timeout = 5;
	mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, (const char*) &con_timeout);
	/* Enable auto reconnect; */
	my_bool enable = TRUE;
	mysql_options(conn, MYSQL_OPT_RECONNECT, &enable);
	/* Set character set to UTF-8; */
	if (0 != mysql_set_character_set(conn, "utf8")) {
		mysql_close(conn);
		return NULL;
	}
	DB *db = new DB(0, conn);
	if (!db) {
		mysql_close(conn);
		return NULL;
	}
	db->setId((unsigned long) db);
	return db;
}
bool DBPool::start(std::string host, std::string user, std::string pass,
		std::string database, unsigned int port) {
	this->host = host;
	this->user = user;
	this->pass = pass;
	this->database = database;
	this->port = port;
	for (unsigned int i = 0; i < this->min_alives; i++) {
		DB *db = this->newDB();
		if (!db) {
			this->doCleanWorks();
			return false;
		}
		this->idle.push_back(db);
	}
	return true;
}
void DBPool::setMinAlives(unsigned int ma) {
	this->min_alives = ma;
}
unsigned int DBPool::getMinAlives() {
	return this->min_alives;
}
}

