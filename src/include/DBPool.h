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

#ifndef DBPOOL_H_
#define DBPOOL_H_
namespace MPool {
typedef std::map<std::string, std::string> DBDataRow;
typedef std::vector<DBDataRow> DBDataSet;
class DBResult {
public:
	DBDataSet data;
};
class DB {
	/// DB Connection;
protected:
	MYSQL *real_conn;
	unsigned int db_errno;
	std::string db_error;
	unsigned long long affected_rows;
	pthread_mutex_t mutex;
	unsigned long id;
public:
	DB(unsigned long id, MYSQL *conn);
	~DB();
	unsigned int getErrno();
	std::string getError();
	unsigned long long getAffectedRows();
	DBResult* query(std::string sql);
	void freeResult(DBResult *result);
	unsigned long getId();
	void setId(unsigned long);
};
class DBPool {
protected:
	std::vector<MPool::DB*> idle;
	std::vector<MPool::DB*> busy;
	std::string host;
	std::string user;
	std::string pass;
	std::string database;
	unsigned int port;
	unsigned int min_alives;
	pthread_mutex_t mutex;
	unsigned long current_id;
public:
	DBPool();
	~DBPool();
	bool start(std::string host, std::string user, std::string pass,
			std::string database, unsigned int port);
	void setMinAlives(unsigned int ma);
	unsigned int getMinAlives();
	DB* allocDB();
	void freeDB(DB *db);
protected:
	DB* newDB();
	void doCleanWorks();
};
}
#endif
