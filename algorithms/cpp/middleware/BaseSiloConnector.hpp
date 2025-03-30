/**
	@author:	Yuxiang Zeng
	@email: 	yxzeng@buaa.edu.cn
	@date:		2024.09.27
*/
#ifndef MIDDLEWARE_BASE_SILO_CONNECTOR_HPP
#define MIDDLEWARE_BASE_SILO_CONNECTOR_HPP

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

#include "../utils/DataType.hpp"
#include "../utils/MetricType.hpp"
#include "../utils/BenchLogger.hpp"


template <typename Distance = EuclideanSquareDistance>
class BaseSiloConnector {
public:
    BaseSiloConnector(const int silo_id, const std::string& silo_ipaddr)
                        : m_silo_id(silo_id), m_silo_ipaddr(silo_ipaddr) {
        m_logger.Init();
    }

    BaseSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port)
                        : m_silo_id(silo_id), m_silo_ipaddr(silo_ipaddr), m_db_name(db_name), m_user_name(user_name), m_password(password), m_db_ipaddr(db_ipaddr), m_db_port(db_port) {
        m_logger.Init();
        ConnectDB(db_name, user_name, password, db_ipaddr, db_port);
    }

    virtual ~BaseSiloConnector() = default;

    // import the data from specific file
    virtual void ImportData(const std::string& file_name) = 0;

    // construct the index
    virtual void ConstructIndex(const std::string& index_options) = 0;

    //Load the index
    virtual void LoadIndex(const std::string& file_path, const std::string& index_options) = 0;

    // connect the vector database
    virtual void ConnectDB(const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port) = 0;

    // perform the knn query, where exactness or approximation depends on the index
    // Notice: the output vector data needs to be sorted by the distance
    virtual std::vector<VectorDataType> KnnQuery(const VectorDataType& query_data, const size_t& query_k, const std::string& condition = nullptr) = 0;
    virtual void KnnQuery(const VectorDataType& query_data, const size_t& query_k, std::vector<VectorDataType>& answer_object, std::vector<VectorDimensionType>& answer_dist, std::vector<std::string> &answer_attr, const std::string& condition = nullptr) = 0;

    //Save Index
    virtual void SaveIndex(const std::string& file_path, const std::string& index_option)=0;

    //If index exist
    virtual bool IndexExists(const std::string& file_path, const std::string& index_option) {
        return false;
    }
    
    // Output the current index size
    virtual size_t IndexSize() const = 0;

    // Output the current data size
    virtual size_t DataSize() const = 0;

    int GetSiloId() const {
        return m_silo_id;
    }

    std::string GetSiloIpAddr() const {
        return m_silo_ipaddr;
    }

    std::string to_string() {
        std::stringstream ss;

        ss << "Silo #(" << m_silo_id << ") with IP address: " << m_silo_ipaddr << "\n";

        return ss.str();
    }

protected:
    int m_silo_id;
    std::string m_silo_ipaddr;
    std::string m_db_name;
    std::string m_table_name;
    std::string m_user_name;
    std::string m_password;
    std::string m_db_port;
    std::string m_db_ipaddr;
    BenchLogger m_logger;
};

#endif  // MIDDLEWARE_BASE_SILO_CONNECTOR_HPP