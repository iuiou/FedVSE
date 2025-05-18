#include <grpcpp/grpcpp.h>
#include <grpc/grpc.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <cmath>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>  
#include <boost/asio/thread_pool.hpp>
#include <signal.h>
namespace bpo = boost::program_options;

#include "protolib/UserBroker.grpc.pb.h"
#include "protolib/BrokerSilo.grpc.pb.h"
#include "utils/DataType.hpp"
#include "utils/MetricType.hpp"
#include "utils/BenchLogger.hpp"
#include "utils/File_IO.h"
#include "middleware/BaseSiloConnector.hpp"
#include "middleware/MilvusSiloConnector.hpp"
#include "global.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using google::protobuf::Empty;
using userbroker::UserBroker;
using userbroker::knnQuery;
using userbroker::rpcComm;
using brokersilo::BrokerSilo;
using brokersilo::Vector;

class SiloConnector {
    private:
        std::unique_ptr<BrokerSilo::Stub> stub_;
        size_t siloId; //silo id
        std::string ipAddr; //silo ip address
        BenchLogger m_logger;
        std::vector<brokersilo::Vector> finalAns;
    
    public:
        SiloConnector(const std::shared_ptr<Channel> &channel, const size_t &id, const std::string &addr):
            stub_(BrokerSilo::NewStub(channel)), siloId(id), ipAddr(addr)  {
            m_logger.Init();
        }

        void requestAns() {
            ClientContext context;
            Empty request;
            finalAns.clear();
            std::unique_ptr<ClientReader<brokersilo::Vector> > reader(
                stub_->getAns(&context, request));
            double grpc_comm = 0;
            brokersilo::Vector nowp;
            while(reader->Read(&nowp)) {
                finalAns.emplace_back(nowp);
                grpc_comm += nowp.ByteSizeLong();
            }
            m_logger.LogAddComm(request.ByteSizeLong());
            if(recordAnswerComm) m_logger.LogAddComm(grpc_comm);
            return;
        }

        float getComm() {
            double comm = m_logger.GetQueryComm();
            m_logger.Init();
            return comm;
        }

        std::vector<VidType> getAnsId() {
            std::vector<VidType> idRes;
            for(auto &item : finalAns) {
                idRes.emplace_back(item.id());
            }
            return idRes;
        }
};

class BrokerConnector {
    private:
        std::unique_ptr<UserBroker::Stub> stub_;
        size_t siloNum;
        std::vector<std::shared_ptr<SiloConnector>> siloPtr; 
        std::vector<std::string> ipAddrSet; //SiloConnector address
        std::string ipAddr; //broker address
        BenchLogger m_logger;

    public:
        BrokerConnector(const std::shared_ptr<Channel> &channel, const std::string &ipAddr, const std::string &ipAddrFile):
            stub_(UserBroker::NewStub(channel)), ipAddr(ipAddr) {
            ReadSiloIPaddr(ipAddrFile, ipAddrSet);
            if (ipAddrSet.empty()) {
                throw std::invalid_argument("No data silo's IP addresses");  
                std::exit(EXIT_FAILURE);
            }
            siloNum = ipAddrSet.size();
            siloPtr.resize(siloNum);
            grpc::ChannelArguments args;  
            args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, INT_MAX);  
            args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, INT_MAX); //set maximum comm cost
            for(size_t i = 0;i < siloNum;i++) { //connect to each silo
                std::string url = ipAddrSet[i];
                std::cout << "User is connecting Silo #(" << std::to_string(i) << ") on IP address " << url << std::endl;
                siloPtr[i] = std::make_shared<SiloConnector>(grpc::CreateCustomChannel(url.c_str(),
                                grpc::InsecureChannelCredentials(), args), i, url);
            }
            m_logger.Init();
        }

        void sendKnnQuery(VectorDataType queryV, size_t queryK, std::string condition) {
            ClientContext context;
            knnQuery request;
            Empty response;
            int dim = queryV.data.size();
            for(int i = 0;i < dim;i++) {
                request.add_data(queryV[i]);
            }
            request.set_k(queryK);
            request.set_condition(condition);
            request.set_qid(queryV.vid);
            Status status = stub_->requestKNN(&context, request, &response);

            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get Diffie-Hellman random value A from server." << std::endl;
                std::exit(EXIT_FAILURE);
            }

            return;
        }

        double getComm() {
            ClientContext context;
            Empty request;
            rpcComm response;
            Status status = stub_->requestComm(&context, request, &response);
            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get Comm from broker." << std::endl;
                std::exit(EXIT_FAILURE);
            }
            return 1.0 * response.rpcdatanum();
        }

        static void getVector(BrokerConnector *impl, const size_t siloId) {
            impl->siloPtr[siloId]->requestAns();
        }

        void parallelGetVector() {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0;i < siloNum; i++) {
                boost::asio::post(pool, std::bind(getVector, this, i));
            }
            pool.join();
        }
        
        void kNNTest(const std::string &queryFile, const std::string &outputFile, const std::string &truthFile, const int queryK) {
            std::vector<VectorDataType> queryVList;
            std::vector<size_t> queryKList;
            std::vector<std::string> queryCList;
            ReadHybridVectorQuery(queryFile, queryVList, queryKList, queryCList);
            std::fill(queryKList.begin(), queryKList.end(), queryK);
            size_t queryNum = queryVList.size();
            std::vector<std::vector<VidType>> ansList;
            for (size_t i = 0; i < queryNum; ++i) { // test the first query
                m_logger.SetStartTimer();
                sendKnnQuery(queryVList[i], queryKList[i], queryCList[i]);
                parallelGetVector();
            
                std::vector<VidType> ansi;
                for(int i = 0;i < siloNum;i++) {
                    std::vector<VidType> idSet = siloPtr[i]->getAnsId();
                    ansi.insert(ansi.end(), idSet.begin(), idSet.end());
                }
                std::cout << i << "th result's size : " << (int)ansi.size() << std::endl;
                // assert((int)result.size() == queryKList[i]);
                ansList.emplace_back(ansi);
                m_logger.SetEndTimer();
                double queryComm = getComm();
                for (size_t j = 0; j < siloNum; j++) {
                    queryComm += siloPtr[j]->getComm();
                }
                m_logger.LogAddComm(queryComm);
                double queryTime = m_logger.GetDurationTime();
                m_logger.LogOneQuery();

                #ifdef SHOW_EVERYONE
                std::cout << std::fixed << std::setprecision(6) 
                    << "Query #(" << queryVList[i].vid << "): runtime = " << queryTime/1000.0 << " [s], communication = " << queryComm/1024.0 << " [KB]" << std::endl;
                #endif
            }
            DumpGroundTruth(outputFile, ansList);
            m_logger.Print();
            EvaluateAnswer(outputFile, truthFile);
            // std::cout << "optimized top-k time: " << 1.0 * optTopKTime / queryNum << "ms" << std::endl;
            // std::cout << "refine k time: " << 1.0 * optRefineTime / (queryNum * siloNum) << "ms" << std::endl;
        }
};

std::shared_ptr<BrokerConnector> brokerPtr = nullptr;

int main(int argc, char** argv) {
    std::string queryFile, outputFile, truthFile, brokerIpAddr, ipAddrFile;
    int queryK;
    try { 
        bpo::options_description option_description("Required options");
        option_description.add_options()
            ("help", "produce help message")
            ("broker-ip", bpo::value<std::string>(), "ip address of central server")
            ("silo-ip", bpo::value<std::string>(), "ip address of each data provider")
            ("query-k", bpo::value<int>(), "query parameter $k$")
            ("query-path", bpo::value<std::string>(), "file path of query vectors and attribute filter")
            ("output-path", bpo::value<std::string>(), "file path that stores output")
            ("truth-path", bpo::value<std::string>(), "file path that stores ground truth of queries");

        bpo::variables_map variable_map;
        bpo::store(bpo::parse_command_line(argc, argv, option_description), variable_map);
        bpo::notify(variable_map);    

        if (variable_map.count("help")) {
            std::cout << option_description << std::endl;
            return 0;
        }

        bool options_all_set = true;

        if (variable_map.count("broker-ip")) {
            brokerIpAddr = variable_map["broker-ip"].as<std::string>();
        } else {
            options_all_set = false;
        }

        if (variable_map.count("silo-ip")) {
            ipAddrFile = variable_map["silo-ip"].as<std::string>();
            std::cout << "Data silo's IP configuration file path was set to " << ipAddrFile << "\n";
        } else {
            std::cout << "Data silo's IP configuration file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("query-k")) {
            queryK = variable_map["query-k"].as<int>();
            std::cout << "The K of KNN was set to " << queryK << "\n";
        } else {
            std::cout << "The K of KNN was not set" << "\n";
        }

        if (variable_map.count("query-path")) {
            queryFile = variable_map["query-path"].as<std::string>();
            std::cout << "User's received query file path was set to " << queryFile << "\n";
        } else {
            std::cout << "User's received query file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("output-path")) {
            outputFile = variable_map["output-path"].as<std::string>();
            std::cout << "User's answer output file path was set to " << outputFile << "\n";
        } else {
            std::cout << "User's answer output file path was not set" << "\n";
        }

        if (variable_map.count("truth-path")) {
            truthFile = variable_map["truth-path"].as<std::string>();
            std::cout << "User's ground truth file path was set to " << truthFile << "\n";
        } else {
            std::cout << "User's ground truth file path was not set" << "\n";
        }

        if (false == options_all_set) {
            throw std::invalid_argument("Some options were not properly set");
            std::cout.flush();
            std::exit(EXIT_FAILURE);
        }
    } catch (std::exception& e) {  
        std::cerr << "Error: " << e.what() << "\n";  
        std::exit(EXIT_FAILURE);
    }

    grpc::ChannelArguments args;  
    args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, INT_MAX);  
    args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, INT_MAX);
    std::string url = brokerIpAddr;
    brokerPtr = std::make_shared<BrokerConnector>(grpc::CreateCustomChannel(brokerIpAddr.c_str(),
                                grpc::InsecureChannelCredentials(), args), brokerIpAddr, ipAddrFile);
    brokerPtr->kNNTest(queryFile, outputFile, truthFile, queryK);
}

