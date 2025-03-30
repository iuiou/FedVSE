/**
	@author:	Yuxiang Zeng
	@email: 	yxzeng@buaa.edu.cn
	@date:		2024.09.28
*/
#ifndef MIDDLEWARE_PG_SILO_CONNECTOR_HPP
// #define MIDDLEWARE_PG_SILO_CONNECTOR_HPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <set>
#include <exception>
#include <signal.h>
#include <unistd.h>

#include <libpq-fe.h> // for PostgreSQL only

#include "../database/VectorDB.hpp"
#include "../index/BaseIndex.hpp"
#include "../utils/DataType.hpp"
#include "../utils/MetricType.hpp"
#include "../utils/BenchLogger.hpp"
#include "BaseSiloConnector.hpp"

#define LOCAL_DEBUG

template <typename Distance = EuclideanSquareDistance>
class PgSiloConnector final : public BaseSiloConnector<Distance> {
public:
    PgSiloConnector(const int silo_id, const std::string& silo_ipaddr) 
                        : BaseSiloConnector<Distance>(silo_id, silo_ipaddr) {
        // BaseSiloConnector will init the logger
        m_conn = NULL;
    }

    PgSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port) 
                        : BaseSiloConnector<Distance>(silo_id, silo_ipaddr, db_name, user_name, password, db_ipaddr, db_port) {
        // BaseSiloConnector will init the logger
        // BaseSiloConnector will connect the database
        m_conn = NULL;
    }

    PgSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port, const std::string& table_name) 
                        : BaseSiloConnector<Distance>(silo_id, silo_ipaddr, db_name, user_name, password, db_ipaddr, db_port) {
        // BaseSiloConnector will init the logger
        // BaseSiloConnector will connect the database
        m_table_name = table_name;
        m_conn = NULL;
    }

    ~PgSiloConnector() override {
        if (m_conn != NULL)
            PQfinish(m_conn);
    }

    void ConnectDB(const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port) override {
        m_conn = PQsetdbLogin(m_db_ipaddr, m_db_port, NULL, NULL, m_db_name, m_user_name, m_password);
        
        if (PQstatus(m_conn) != CONNECTION_OK) {
            PrintLine(__LINE__);
            std::cerr << "Connection to database failed: " << PQerrorMessage(m_conn) << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::cout << "Connect PostgreSQL at silo #(" << m_silo_id <<  ") successfully." << std::endl;    
    }

    // import the data from specific file
    void ImportData(const std::string& file_name) override {

    }

    // construct the index
    void ConstructIndex(const std::string& index_options) override {

    }

    // perform the knn query, where exactness or approximation depends on the index
    std::vector<VectorDataType> KnnQuery(const VectorDataType& query_data, const size_t& query_k) override {
        std::vector<VectorDataType> answer_object;
        return answer_object;
    }

    void KnnQuery(const VectorDataType& query_data, const size_t& query_k, std::vector<VectorDataType>& answer_object, std::vector<VectorDimensionType>& answer_dist) override {

    }

    // Output the current index size
    size_t IndexSize() const override {
        return 0;
    }

    // Output the current data size
    size_t DataSize() const override {
        return 0;
    }

private:
    PGconn *m_conn;
};


#endif  // MIDDLEWARE_PG_SILO_CONNECTOR_HPP
