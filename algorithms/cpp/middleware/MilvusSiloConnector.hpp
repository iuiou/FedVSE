/**
	@author:	Zeheng Fan
	@email: 	fanzh@buaa.edu.cn
	@date:		2024.12.15
*/
#ifndef MIDDLEWARE_MILVUS_SILO_CONNECTOR_HPP
#define MIDDLEWARE_MILVUS_SILO_CONNECTOR_HPP

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
#include <random>

#include "../utils/DataType.hpp"
#include "../utils/MetricType.hpp"
#include "../utils/BenchLogger.hpp"
#include "../utils/File_IO.h"
#include "BaseSiloConnector.hpp"
#include "../milvus/include/milvus/MilvusClient.h"
#include "../milvus/include/milvus/types/CollectionSchema.h"

template <typename Distance = EuclideanSquareDistance>
class MilvusSiloConnector final : public BaseSiloConnector<Distance> {
private:
    std::shared_ptr<milvus::MilvusClient> db;
    BenchLogger m_logger;

public:
    MilvusSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& collection_Name)
            : BaseSiloConnector<Distance>(silo_id, silo_ipaddr) {
        // BaseSiloConnector will init the logger
        db = nullptr;
        collection_name = collection_Name;
        partition_name = "data_partition";
        m_logger.Init();
    }

    MilvusSiloConnector(const int silo_id, const std::string& silo_ipaddr, const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port)
            : BaseSiloConnector<Distance>(silo_id, silo_ipaddr, db_name, user_name, password, db_ipaddr, db_port) {
        // BaseSiloConnector will init the logger
        // BaseSiloConnector will connect the database
        db = nullptr;
        collection_name = "LOCAL_DATA" + std::to_string(silo_id);
        partition_name = "data_partition";
    }

    void CheckMilvusStatus(std::string&& prefix, const milvus::Status& status) {
        if (!status.IsOk()) {
            std::cout << prefix << " " << status.Message() << std::endl;
            exit(1);
        }
    }

    void ConnectDB(const std::string& db_name, const std::string& user_name, const std::string& password, const std::string& db_ipaddr, const std::string& db_port) override {
        db = milvus::MilvusClient::Create();
        milvus::ConnectParam connect_param{db_ipaddr, (uint16_t)std::stoi(db_port)};
        auto status = db->Connect(connect_param);
        CheckMilvusStatus("Failed to connect milvus server:", status);
        std::cout << "Connect Milvus Server at silo #(" << this->GetSiloId() <<  ") successfully." << std::endl;
    }

    // import the data from specific file and build milvus collection
    void ImportData(const std::string& file_name) override {
        m_file_name = file_name;
        ReadVectorData(file_name, m_data_list);
    }
    
    void ImportScalarData(const std::string& file_name) {
        m_scalar_file_name = file_name;
        ReadScalarData(file_name, m_scalar_data_list);
    }

    // construct the index
    void ConstructIndex(const std::string& index_options) override {
        // create milvus table
        std::cout << "start construct index" << std::endl;
        auto status = db->DropCollection(collection_name);
        int attrNum = (int)m_scalar_data_list.size();
        milvus::CollectionSchema collection_schema(collection_name);
        collection_schema.AddField({"id", milvus::DataType::INT64, "vector id", true, false});
        for(int i = 0;i < attrNum;i++) {
            if(m_scalar_data_list[i]->getType() == INT) {
                collection_schema.AddField({m_scalar_data_list[i]->getName(), milvus::DataType::INT32, "attribute " + std::to_string(i)});
            } else if (m_scalar_data_list[i]->getType() == FLOAT) {
                collection_schema.AddField({m_scalar_data_list[i]->getName(), milvus::DataType::FLOAT, "attribute " + std::to_string(i)});
            } else {
                milvus::FieldSchema stringField(m_scalar_data_list[i]->getName(), milvus::DataType::VARCHAR, "attribute " + std::to_string(i));
                std::map<std::string, std::string> attrMp;
                attrMp["max_length"] = "50";
                stringField.SetTypeParams(attrMp);
                collection_schema.AddField(stringField);
            }
        }
        std::cout << "after import data" << std::endl;
        int dim = (int)m_data_list[0].data.size();
        collection_schema.AddField(milvus::FieldSchema("vector", milvus::DataType::FLOAT_VECTOR, "vector field").WithDimension(dim));
        status = db->CreateCollection(collection_schema);
        CheckMilvusStatus("Failed to create collection:", status);
        std::cout << "Successfully create collection." << std::endl;
        milvus::IndexType indexType;
        if (index_options == "FLAT") {
            indexType = milvus::IndexType::FLAT;
            milvus::IndexDesc index_desc("vector", "", indexType, milvus::MetricType::L2, 0);
            status = db->CreateIndex(collection_name, index_desc);
        } else if (index_options == "HNSW") {
            indexType = milvus::IndexType::HNSW;
            milvus::IndexDesc index_desc("vector", "", indexType, milvus::MetricType::L2, 0);
            index_desc.AddExtraParam("efConstruction", 512);
            index_desc.AddExtraParam("M", 32);
            status = db->CreateIndex(collection_name, index_desc);
        } else {
            indexType = milvus::IndexType::IVF_FLAT;
            milvus::IndexDesc index_desc("vector", "", indexType, milvus::MetricType::L2, 0);
            int N = (int)m_data_list.size();
            index_desc.AddExtraParam("nlist", N / 1000);
            status = db->CreateIndex(collection_name, index_desc);
        }
        CheckMilvusStatus("Failed to create index:", status);
        std::cout << "Successfully create index." << std::endl;
    }

    //load the vector and meta data to Milvus
    void importDataToMilvus() {
        auto status = db->CreatePartition(collection_name, partition_name);
        CheckMilvusStatus("Failed to create partition:", status);
        std::cout << "Successfully create partition." << std::endl;
        status = db->LoadCollection(collection_name);
        CheckMilvusStatus("Failed to load collection:", status);
        int m = (int)m_scalar_data_list.size();
        int n = (int)m_data_list.size();
        std::vector<int64_t> idSet; //vector id
        std::vector<std::vector<float>> vectorSet; //vector field
        idSet.resize(n);
        vectorSet.resize(n);
        for(int i = 0;i < n;i++) {
            idSet[i] = m_data_list[i].vid;
            vectorSet[i] = std::vector<float>(m_data_list[i].data.begin(), m_data_list[i].data.end());
        }
        int roundNum = (n + 4999) / 5000;
        for(int i = 0;i < roundNum;i++) {
            std::vector<milvus::FieldDataPtr> fields_data;
            int ptr = i * 5000;
            int len = (i == roundNum - 1) ? n % 5000 : 5000;
            len = (len == 0) ? 5000 : len;
            // std::cout << ptr << " " << len << std::endl;
            std::vector<int64_t> idNowSet(idSet.begin() + ptr, idSet.begin() + ptr + len);
            std::vector<std::vector<float>> vectorNowSet(vectorSet.begin() + ptr, vectorSet.begin() + ptr + len);
            fields_data.emplace_back(std::make_shared<milvus::Int64FieldData>("id", idNowSet));
            // std::cout << idNowSet.size() << std::endl;
            fields_data.emplace_back(std::make_shared<milvus::FloatVecFieldData>("vector", vectorNowSet));
            for(int j = 0;j < m;j++) {
                if(m_scalar_data_list[j]->getType() == FLOAT) {
                    std::vector<float> floatSet = dynamic_cast<FloatType*>(m_scalar_data_list[j].get())->getArray(ptr, len);
                    fields_data.emplace_back(std::make_shared<milvus::FloatFieldData>(m_scalar_data_list[j]->getName(), floatSet));
                } else if(m_scalar_data_list[j]->getType() == STRING){
                    std::vector<std::string> stringSet = dynamic_cast<StringType*>(m_scalar_data_list[j].get())->getArray(ptr, len);
                    fields_data.emplace_back(std::make_shared<milvus::VarCharFieldData>(m_scalar_data_list[j]->getName(), stringSet));
                } else {
                    std::vector<int> intSet = dynamic_cast<IntType*>(m_scalar_data_list[j].get())->getArray(ptr, len);
                    fields_data.emplace_back(std::make_shared<milvus::Int32FieldData>(m_scalar_data_list[j]->getName(), intSet));
                }
            }
            milvus::DmlResults dml_results;
            status = db->Insert(collection_name, partition_name, fields_data, dml_results);
            CheckMilvusStatus("Failed to insert:", status);
        }
        //release milvus collection
        status = db->ReleaseCollection(collection_name);
        CheckMilvusStatus("Failed to drop collection:", status);
        std::cout << "release collection " << collection_name << std::endl;
        return;
    }

    //Load the index
    void LoadIndex(const std::string& file_path, const std::string& index_options) override {
        return; //index is preserved by milvus, so do not need to load any index from local storage
    }

    //Save the index
    void SaveIndex(const std::string& file_path, const std::string& index_option) override {
        return; //do not need to save index in the local storage
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

    void LoadData() {
        m_logger.SetStartTimer();
        auto status = db->LoadCollection(collection_name);
        CheckMilvusStatus("Failed to load collection:", status);
        std::cout << "load collection " << collection_name << std::endl;
        m_logger.SetEndTimer();
        std::cout << "load collection takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl; 
    }

    void ReleaseData() {
        m_logger.SetStartTimer();
         //release milvus collection
        auto status = db->ReleaseCollection(collection_name);
        CheckMilvusStatus("Failed to drop collection:", status);
        std::cout << "release collection " << collection_name << std::endl;
        m_logger.SetEndTimer();
        std::cout << "release collection takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;
    }

    // perform the knn query, where exactness or approximation depends on the index
    std::vector<VectorDataType> KnnQuery(const VectorDataType& query_data, const size_t& query_k, const std::string& condition = nullptr) override {
        milvus::SearchArguments arguments{};
        arguments.SetCollectionName(collection_name);
        arguments.AddPartitionName(partition_name);
        arguments.SetTopK(query_k);
        arguments.AddOutputField("vector");
        arguments.SetExpression(condition);
        arguments.AddTargetVector("vector", std::move(query_data.data));
        milvus::SearchResults search_results{};
        auto status = db->Search(arguments, search_results);
        CheckMilvusStatus("Failed to search:", status);
        std::cout << "Successfully search." << std::endl;
        std::vector<VectorDataType> ans;
        for (auto& result : search_results.Results()) {
            auto& ids = result.Ids().IntIDArray();
            auto& distances = result.Scores();
            auto vector_field = result.OutputField("vector");
            milvus::FloatVecFieldDataPtr vector_field_ptr = std::static_pointer_cast<milvus::FloatVecFieldData>(vector_field);
            auto& vector_data = vector_field_ptr->Data();
            for (size_t i = 0; i < ids.size(); ++i) {
                std::vector<float> nowVec = vector_data[i];
                int dim = (int)nowVec.size();
                VectorDataType now(dim, ids[i]);
                for(int j = 0;j < dim;j++) now.data[j] = nowVec[j];
                ans.emplace_back(now);
            }
        }
         //release milvus collection
        status = db->ReleaseCollection(collection_name);
        CheckMilvusStatus("Failed to drop collection:", status);
        std::cout << "release collection " << collection_name << std::endl;
        return ans;
    }

    void KnnQuery(const VectorDataType &query_data, const size_t &query_k, std::vector <VectorDataType> &answer_object,
                  std::vector <VectorDimensionType> &answer_dist, std::vector <std::string> &answer_attr,
                  const std::string &condition = nullptr) override {
        milvus::SearchArguments arguments{};
        arguments.SetCollectionName(collection_name);
        arguments.AddPartitionName(partition_name);
        arguments.SetTopK(query_k);
        arguments.AddOutputField("vector");
        for (auto &attr: m_scalar_data_list) {
            arguments.AddOutputField(attr->getName());
        }
        // arguments.AddExtraParam("ef", query_k);
        arguments.SetExpression(condition);
        arguments.AddTargetVector("vector", std::move(query_data.data));
        milvus::SearchResults search_results{};
        auto status = db->Search(arguments, search_results);
        CheckMilvusStatus("Failed to search:", status);
        for (auto &result: search_results.Results()) {
            auto &ids = result.Ids().IntIDArray();
            if (ids.size() == 0) break;
            auto &distances = result.Scores();
            auto vector_field = result.OutputField("vector");
            milvus::FloatVecFieldDataPtr vector_field_ptr = std::static_pointer_cast<milvus::FloatVecFieldData>(
                    vector_field);
            auto &vector_data = vector_field_ptr->Data();
            std::vector <std::string> attr_str;
            attr_str.assign(ids.size(), "");
            for (auto &attr: m_scalar_data_list) {
                auto scalar_data = result.OutputField(attr->getName());
                if (attr->getType() == INT) {
                    milvus::Int32FieldDataPtr int_field_ptr = std::static_pointer_cast<milvus::Int32FieldData>(
                            scalar_data);
                    auto &int_data = int_field_ptr->Data();
                    for (size_t i = 0; i < ids.size(); ++i) {
                        if (attr_str[i] != "") {
                            attr_str[i] += " and ";
                        }
                        attr_str[i] += attr->getName() + "=" + std::to_string(int_data[i]);
                    }
                } else {
                    milvus::VarCharFieldDataPtr str_field_ptr = std::static_pointer_cast<milvus::VarCharFieldData>(
                            scalar_data);
                    auto &str_data = str_field_ptr->Data();
                    for (size_t i = 0; i < ids.size(); ++i) {
                        if (attr_str[i] != "") {
                            attr_str[i] += " and ";
                        }
                        attr_str[i] += attr->getName() + "=" + str_data[i];
                    }
                }
            }
            for (size_t i = 0; i < ids.size(); ++i) {
                std::vector<float> nowVec = vector_data[i];
                int dim = (int) nowVec.size();
                VectorDataType now(dim, ids[i]);
                for (int j = 0; j < dim; j++) now.data[j] = nowVec[j];
                answer_object.emplace_back(now);
                answer_dist.emplace_back(distances[i]);
                answer_attr.emplace_back(attr_str[i]);
            }
        }
        return;
    }

    // Output the current index size
    size_t IndexSize() const override {
        // index is managed by milvus, so cannot get the index
        return 0;
    }

    // Output the current data size
    size_t DataSize() const override {
        return m_scalar_data_list[0]->getSize();
    }

    std::vector<VectorDataType> getVectorData() const {
        return m_data_list;
    }

    size_t scalarSize() {
        return m_scalar_data_list.size(); 
    }

    std::vector<std::string> getScalarName() {
        std::vector<std::string> nameSet;
        for(auto &attr : m_scalar_data_list) {
            nameSet.emplace_back(attr->getName());
        }
        return nameSet;
    }

    std::vector<int> getScalarData(int scalarIndex) {
        auto &attr = m_scalar_data_list[scalarIndex];
        if(attr->getType() == FLOAT) {
            std::cout << "invalid type" << std::endl;
            exit(0);
        } else if(attr->getType() == INT) {
            return dynamic_cast<IntType*>(attr.get())->getArray(0, DataSize());
        } else {
            return dynamic_cast<StringType*>(attr.get())->getIdArray(0, DataSize());
        }
    }

    int getStringId(int index, std::string value) {
        return (dynamic_cast<StringType*>(m_scalar_data_list[index].get()))->getStrId(value);
    }

    ~MilvusSiloConnector() override {
        ReleaseData();
    }

private:
    std::string collection_name;
    std::string partition_name;
    std::string m_file_name;
    std::string m_scalar_file_name;
    std::vector<VectorDataType> m_data_list;
    std::vector<std::shared_ptr<attribute> > m_scalar_data_list;
};

#endif  // MIDDLEWARE_MILVUS_SILO_CONNECTOR_HPP