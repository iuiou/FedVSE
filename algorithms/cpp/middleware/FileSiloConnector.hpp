/**
	@author:	Yuxiang Zeng
	@email: 	yxzeng@buaa.edu.cn
	@date:		2024.09.28
*/
#ifndef MIDDLEWARE_FILE_SILO_CONNECTOR_HPP
// #define MIDDLEWARE_FILE_SILO_CONNECTOR_HPP

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
#include <cstdio>

#include "../database/VectorDB.hpp"
#include "../index/BaseIndex.hpp"
#include "../index/HnswIndex.hpp"
#include "../index/LinearScanIndex.hpp"
#include "../index/LshIndex.hpp"
#include "../index/PqIndex.hpp"
#include "../index/IvfIndex.hpp"
#include "../index/IvfPqIndex.hpp"
#include "../utils/DataType.hpp"
#include "../utils/MetricType.hpp"
#include "../utils/BenchLogger.hpp"
#include "../utils/File_IO.h"
#include "BaseSiloConnector.hpp"

template <typename Distance = EuclideanSquareDistance>
class FileSiloConnector final : public BaseSiloConnector<Distance> {
private:
    using BaseIndex_t = BaseIndex<Distance>;

public:
    FileSiloConnector(const int silo_id, const std::string& silo_ipaddr)
            : BaseSiloConnector<Distance>(silo_id, silo_ipaddr) {
        // BaseSiloConnector will init the logger
        m_index_ptr = nullptr;
    }

    FileSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port)
            : BaseSiloConnector<Distance>(silo_id, silo_ipaddr, db_name, user_name, password, db_ipaddr, db_port) {
        // BaseSiloConnector will init the logger
        // BaseSiloConnector will connect the database
        m_index_ptr = nullptr;
    }

    ~FileSiloConnector() override {

    }

    void ConnectDB(const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port) override {
        /* 
            FiloSiloConnector loads data from file instead a real vector database,
            so there is no actual step to connect the database.
        */
        std::cout << "Connect FileDB at silo #(" << this->GetSiloId() <<  ") successfully." << std::endl;
    }

    // import the data from specific file
    void ImportData(const std::string& file_name) override {
        m_file_name = file_name;
        ReadVectorData(file_name, m_data_list);
    }

    // construct the index
    void ConstructIndex(const std::string& index_options) override {
        if (index_options.find("HNSW") != std::string::npos) {
            m_index_ptr = std::make_unique<HnswIndex<Distance>>(m_data_list);
        } else if (index_options.find("Linear") != std::string::npos) {
            m_index_ptr = std::make_unique<LinearScanIndex<Distance>>(m_data_list);
        } else if (index_options.find("LSH") != std::string::npos) {
            m_index_ptr = std::make_unique<LshIndex<Distance>>(m_data_list);
        } else if (index_options.find("PQ") != std::string::npos) {
            m_index_ptr = std::make_unique<PqIndex<Distance>>(m_data_list);
        } else if (index_options.find("IVF") != std::string::npos) {
            m_index_ptr = std::make_unique<IvfIndex<Distance>>(m_data_list);
        } else if (index_options.find("IQ") != std::string::npos) {
            m_index_ptr = std::make_unique<IvfPqIndex<Distance>>(m_data_list);
        }else {// default setting
            m_index_ptr = std::make_unique<LinearScanIndex<Distance>>(m_data_list);
        }
    }

    //Load the index
    void LoadIndex(const std::string& file_path, const std::string& index_options) override {
        std::string index_filename = transformPath(file_path) + "_" + index_options + ".faissindex";
        if (index_options.find("HNSW") != std::string::npos) {
            m_index_ptr = std::make_unique<HnswIndex<Distance>>(index_filename, m_data_list);
        } else if (index_options.find("Linear") != std::string::npos) {
            m_index_ptr = std::make_unique<LinearScanIndex<Distance>>(index_filename, m_data_list);
        } else if (index_options.find("LSH") != std::string::npos) {
            m_index_ptr = std::make_unique<LshIndex<Distance>>(index_filename, m_data_list);
        } else if (index_options.find("PQ") != std::string::npos) {
            m_index_ptr = std::make_unique<PqIndex<Distance>>(index_filename, m_data_list);
        } else if (index_options.find("IVF") != std::string::npos) {
            m_index_ptr = std::make_unique<IvfIndex<Distance>>(index_filename, m_data_list);
        } else if (index_options.find("IQ") != std::string::npos) {
            m_index_ptr = std::make_unique<IvfPqIndex<Distance>>(index_filename, m_data_list);
        } else {// default setting
            m_index_ptr = std::make_unique<LinearScanIndex<Distance>>(index_filename, m_data_list);
        }
    }

    //Save the index
    void SaveIndex(const std::string& file_path, const std::string& index_option) override {
        std::string index_filename = transformPath(file_path) + "_" + index_option + ".faissindex";
        FILE* file = std::fopen(index_filename.c_str(), "r");
        if (file) {
            std::cout << "Index file already exists" << std::endl;
        } else {
            std::cout << "start to create index" << std::endl;
            m_index_ptr->SaveIndex(index_filename);
            std::cout << "Index saved to " << index_filename << std::endl;
        }
    }

    virtual bool IndexExists(const std::string& file_path, const std::string& index_option) {
        std::string index_filename = transformPath(file_path) + "_" + index_option + ".faissindex";
        FILE* file = std::fopen(index_filename.c_str(), "r");
        if (file) {
            std::fclose(file);
            return true;
        } else {
            return false;
        }
    }

    std::string transformPath(const std::string& path) {
        std::stringstream ss(path);
        std::string segment;
        std::vector<std::string> split;

        while (std::getline(ss, segment, '/')) {
            split.push_back(segment);
        }

        size_t len = split.size();

        if (len > 1) {
            split[len - 1] = split[len - 2];
        }

        std::ostringstream joinedPath;
        for (size_t i = 0; i < len; ++i) {
            joinedPath << split[i];
            if (i < len - 1) {
                joinedPath << "/";
            }
        }

        return joinedPath.str();
    }

    // perform the knn query, where exactness or approximation depends on the index
    std::vector<VectorDataType> KnnQuery(const VectorDataType& query_data, const size_t& query_k) override {
        if (m_index_ptr == nullptr) {
            throw std::invalid_argument("vector data index needs to be built before processing knn query");
            std::exit(EXIT_FAILURE);
        }

#ifdef LOCAL_DEBUG
        PrintLine(__LINE__);
        std::cout << "k = " << query_k << ", query_data = " << query_data.to_string() << std::endl;
#endif

        std::vector<VectorDataType> answer = m_index_ptr->KnnQuery(query_data, query_k);

        return answer;
    }

    void KnnQuery(const VectorDataType& query_data, const size_t& query_k, std::vector<VectorDataType>& answer_object, std::vector<VectorDimensionType>& answer_dist) override {
        if (m_index_ptr == nullptr) {
            throw std::invalid_argument("vector data index needs to be built before processing knn query");
            std::exit(EXIT_FAILURE);
        }

#ifdef LOCAL_DEBUG
        PrintLine(__LINE__);
        std::cout << "k = " << query_k << ", query_data = " << query_data.to_string() << std::endl;
#endif

        m_index_ptr->KnnQuery(query_data, query_k, answer_object, answer_dist);
    }

    // Output the current index size
    size_t IndexSize() const override {
        if (m_index_ptr == nullptr)
            return 0;
        else
            return m_index_ptr->IndexSize();
    }

    // Output the current data size
    size_t DataSize() const override {
        return m_data_list.size();
    }

private:
    std::string m_file_name;
    std::vector<VectorDataType> m_data_list;
    std::unique_ptr<BaseIndex_t> m_index_ptr;
};

#endif  // MIDDLEWARE_FILE_SILO_CONNECTOR_HPP