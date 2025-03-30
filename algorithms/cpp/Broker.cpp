#include <grpcpp/grpcpp.h>
#include <grpc/grpc.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <cmath>
#include <map>
#include <queue>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>  
#include <boost/asio/thread_pool.hpp>
#include <signal.h>
#include <algorithm>
namespace bpo = boost::program_options;

#include "protolib/BrokerSilo.grpc.pb.h"
#include "protolib/UserBroker.grpc.pb.h"
#include "crypto/AES.h"
#include "crypto/util.hpp"

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
using brokersilo::BrokerSilo;
using brokersilo::DiffieHellmanParams;
using brokersilo::DiffieHellmanRg;
using brokersilo::EncryptData;
using brokersilo::Vector;
using brokersilo::Number;
using userbroker::UserBroker;
using userbroker::rpcComm;

struct boundary {
    float bound;
    size_t l_numbouns;
    size_t r_numbouns;

    bool friend operator < (const boundary &a, const boundary &b) {
        return a.bound < b.bound;
    }

    bool friend operator == (const boundary &a, const boundary &b) {
        return a.bound == b.bound;
    }
};

struct bucket {
    float l, r;
    size_t numL, numR;
    int op; // op = 1 real interval op = 2 virtual interval

    bucket(float l_, float r_, size_t numL_, size_t numR_, int op_ = 1){
        l = l_, r = r_;
        numL = numL_, numR = numR_;
        op = op_;
    }
};

struct optBucket {
    float r;
    size_t num;
    size_t siloId;
    
    optBucket(float r_, size_t num_, size_t siloId_) {
        r = r_;
        num = num_;
        siloId = siloId_;
    }

    bool friend operator < (const optBucket &a, const optBucket &b) {
        return a.r < b.r;
    }
};

class SiloConnector {
    private:
        std::unique_ptr<BrokerSilo::Stub> stub_;
        size_t siloId; //silo id
        std::string ipAddr; //silo ip address
        BenchLogger m_logger;

    public:
        SiloConnector(const std::shared_ptr<Channel> &channel, const size_t &id, const std::string &addr):
            stub_(BrokerSilo::NewStub(channel)), siloId(id), ipAddr(addr)  {
            m_logger.Init();
        }

        std::pair<uint64_t, uint64_t> GetParams() {  
            Empty request;
            DiffieHellmanParams response;
            ClientContext context;

            Status status = stub_->GetParams(&context, request, &response);
            uint64_t p, g;
            if (status.ok()) {
                p = response.p();
                g = response.g();
                m_logger.LogAddComm(request.ByteSizeLong() + response.ByteSizeLong());
            } else {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::exit(EXIT_FAILURE);
            }
            return std::make_pair(p, g);
        }

        uint64_t PerformKeyExchange(uint64_t p, uint64_t g) {
            DiffieHellmanRg request;
            DiffieHellmanRg response;
            ClientContext context;

            // 从1到p-2之间随机生成b
            uint64_t b = sample_random(1, p-2);
            uint64_t B = mod_pow(g, b, p);
            uint64_t A;

            // 从gRPC服务获取Diffie-Hellman参数  
            request.set_rg_mod_p(B);
            Status status = stub_->GetRandom(&context, request, &response);
    
            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get Diffie-Hellman random value A from server." << std::endl;
                std::exit(EXIT_FAILURE);
            }
            A = response.rg_mod_p();
            
            m_logger.LogAddComm(request.ByteSizeLong() + response.ByteSizeLong());
            // 计算共享密钥 A^b%p  
            uint64_t shared_secret_key = mod_pow(A, b, p);
            return shared_secret_key;
        }

        EncryptData requestContribution(const VectorDataType &queryV, const size_t &queryK, const std::string &condition) {
            ClientContext context;
            brokersilo::knnQuery nowQuery;
            EncryptData response;
            int dim = (int)queryV.data.size();
            for(int i = 0;i < dim;i++) {
                nowQuery.add_data(queryV[i]);
            }
            nowQuery.set_qid(queryV.vid);
            nowQuery.set_condition(condition);
            nowQuery.set_k(queryK);
            Status status = stub_->evaluateK(&context, nowQuery, &response);

            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get contribution information from silo " << std::to_string(siloId) << std::endl;
                std::exit(EXIT_FAILURE);
            }
            m_logger.LogAddComm(nowQuery.ByteSizeLong() + response.ByteSizeLong());
            return response;
        }

        EncryptData requestInterval(const EncryptData &limitK) {
            ClientContext context;
            EncryptData response;
            
            Status status = stub_->requestInterval(&context, limitK, &response);

            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get interval information from silo " << std::to_string(siloId) << std::endl;
                std::exit(EXIT_FAILURE);
            }
            m_logger.LogAddComm(limitK.ByteSizeLong() + response.ByteSizeLong());
            return response;
        }

        EncryptData sendRange(const EncryptData &range) {
            ClientContext context;
            EncryptData response;
            Status status = stub_->sendRange(&context, range, &response);
            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to get (dis, silo id) from silo " << std::to_string(siloId) << std::endl;
                std::exit(EXIT_FAILURE);
            }

            m_logger.LogAddComm(range.ByteSizeLong() + response.ByteSizeLong());
            return response;
        }

        void requestK(const Number &k) {
            ClientContext context;
            Empty response;
            Status status = stub_->sendNumber(&context, k, &response);
            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_message() << std::endl;
                std::cout << "Failed to send number to silo " << std::to_string(siloId) << std::endl;
                std::exit(EXIT_FAILURE);
            }
            m_logger.LogAddComm(k.ByteSizeLong() + response.ByteSizeLong());
            return;
        }      

        float getComm() {
            double comm = m_logger.GetQueryComm();
            m_logger.Init();
            return comm;
        }  
};

class UserBrokerImpl final : public UserBroker::Service {
    private:
        std::mutex dataMutex;
        std::map<size_t, uint64_t> pMap, gMap;
        std::map<size_t, std::vector<uint64_t> > shared_secret_key_list; //aes key
        size_t siloNum;
        std::vector<std::shared_ptr<SiloConnector>> siloPtr; //指向所有SiloConnector指针
        std::vector<std::string> ipAddrSet; //SiloConnector address
        std::vector<std::shared_ptr<bucket>> bucketSet;
        std::vector<std::vector<std::shared_ptr<bucket>>> eachBucketSet; 
        std::vector<std::vector<std::shared_ptr<optBucket>>> optBucketSet;
        std::map<size_t, EncryptData> databack; // save the encryptdata
        BenchLogger m_logger;

    public:
        explicit UserBrokerImpl(const std::string &ipAddrFile) {
            shared_secret_key_list.clear();
            for(size_t i = 0;i < siloNum;i++) shared_secret_key_list[i] = std::vector<uint64_t>();
            ReadSiloIPaddr(ipAddrFile, ipAddrSet);
            if (ipAddrSet.empty()) {
                throw std::invalid_argument("No data silo's IP addresses");  
                std::exit(EXIT_FAILURE);
            }
            siloNum = (size_t)ipAddrSet.size();
            siloPtr.resize(siloNum);
            grpc::ChannelArguments args;  
            args.SetInt(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, INT_MAX);  
            args.SetInt(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, INT_MAX); //set maximum comm cost
            for(size_t i = 0;i < siloNum;i++) { //connect to each silo
                std::string url = ipAddrSet[i];
                std::cout << "Broker is connecting Silo #(" << std::to_string(i) << ") on IP address " << url << std::endl;
                siloPtr[i] = std::make_shared<SiloConnector>(grpc::CreateCustomChannel(url.c_str(),
                                grpc::InsecureChannelCredentials(), args), i, url);
            }
            m_logger.Init();
        }   

        static void exchangeParamsThread(UserBrokerImpl *impl, const size_t siloId) {
            std::lock_guard<std::mutex> lock(impl->dataMutex);
            std::pair<uint64_t, uint64_t> params = impl->siloPtr[siloId]->GetParams();
            impl->pMap[siloId] = params.first;
            impl->gMap[siloId] = params.second;
        }

        void parallelExchangeParams() {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0; i < siloNum; i++) {
                boost::asio::post(pool, std::bind(exchangeParamsThread, this, i)); 
            }
            pool.join();
        }

        static void exchangeKey(UserBrokerImpl *impl, const size_t siloId) {
            std::lock_guard<std::mutex> lock(impl->dataMutex);
            for(int i = 0;i < 8;i++) {
                uint64_t key = impl->siloPtr[siloId]->PerformKeyExchange(impl->pMap[siloId], impl->gMap[siloId]);
                impl->shared_secret_key_list[siloId].emplace_back(key);
            }
        }

        void parallelExchangeKey() {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0; i < siloNum; i++) {
                boost::asio::post(pool, std::bind(exchangeKey, this, i)); 
            }
            pool.join();
        }

        static void getContribution(UserBrokerImpl *impl, const size_t siloId, const VectorDataType &queryV, const size_t queryK, const std::string &condition) {
            EncryptData contribution = impl->siloPtr[siloId]->requestContribution(queryV, queryK, condition);
            std::lock_guard<std::mutex> lock(impl->dataMutex);
            impl->databack[siloId] = contribution;
        }

        void parallelGetContribution(const VectorDataType &queryV, const size_t queryK, const std::string &condition) {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0;i < siloNum; i++) {
                boost::asio::post(pool, std::bind(getContribution, this, i, queryV, queryK, condition)); 
            }
            pool.join();
        }

        static void getInterval(UserBrokerImpl *impl, const size_t siloId, const EncryptData &kCiperText) {
            std::shared_ptr<SiloConnector> siloptr = impl->siloPtr[siloId];
            EncryptData interval = siloptr->requestInterval(kCiperText);
            std::unique_lock<std::mutex> lock(impl->dataMutex);
            impl->databack[siloId] = interval;
        }              

        void parallelGetInterval(const std::map<size_t, EncryptData> &kCiperTextMp) {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0;i < siloNum; i++) {
                boost::asio::post(pool, std::bind(getInterval, this, i, kCiperTextMp.at(i))); 
            }
            pool.join();
        }

        static void getDisInf(UserBrokerImpl *impl, const size_t siloId, const EncryptData &rCiperText) {
            EncryptData disInf = impl->siloPtr[siloId]->sendRange(rCiperText);
            std::lock_guard<std::mutex> lock(impl->dataMutex);
            impl->databack[siloId] = disInf;
        }

        void parallelGetDisInf(const std::map<size_t, EncryptData> &rCiperTextMp) {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0;i < siloNum; i++) {
                boost::asio::post(pool, std::bind(getDisInf, this, i, rCiperTextMp.at(i)));
            }
            pool.join();
        } 

        static void sendK(UserBrokerImpl *impl, const size_t siloId, const Number &kCiperText) {
            impl->siloPtr[siloId]->requestK(kCiperText);
        }

        void parallelsendK(const std::map<size_t, Number> &kCiperTextMp) {
            boost::asio::thread_pool pool(std::thread::hardware_concurrency());
            for (size_t i = 0;i < siloNum; i++) {
                boost::asio::post(pool, std::bind(sendK, this, i, kCiperTextMp.at(i)));
            }
            pool.join();
        }

        void secretKeyExchange() {
            pMap.clear(), gMap.clear();
            parallelExchangeParams();
            for(size_t i = 0;i < siloNum;i++) shared_secret_key_list[i].clear();
            parallelExchangeKey();
        }

        void analyizeContribution(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const VectorDataType &queryV,
            const size_t queryK, const std::string &condition, std::vector<size_t> &ans) {
            databack.clear();
            parallelGetContribution(queryV, queryK, condition);
            AES aes(AESKeyLength::AES_128);
            std::vector<float> contributions(siloNum);
            float minC = FLOAT_INF, maxC = 0;
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i];
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key.at(i), aes_iv.at(i));
                float contri = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                minC = std::min(minC, contri);
                maxC = std::max(maxC, contri);
                contributions[i] = contri;
            }
            ans.resize(siloNum);
            if(localSearchOption == 1) {
                for(int i = 0;i < siloNum;i++) {
                    ans[i] = queryK;
                }
            } else if(localSearchOption == 2) {
                for(int i = 0;i < siloNum;i++) {
                    int nowK = (int)std::ceil(1.0 * contributions[i] / maxC * queryK);
                    nowK = std::max(nowK, 1);
                    ans[i] = (size_t)nowK;
                }
            } else {
                for(int i = 0;i < siloNum;i++) {
                    int nowK = (int)std::ceil(1.0 * minC / contributions[i] * queryK);
                    nowK = std::max(nowK, 1);
                    ans[i] = (size_t)nowK;
                }
            }
            return;
        }

        void analyzeIntervalBase(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const std::map<size_t, EncryptData> &kCiperTextMp, const int queryK) {
            std::cout << "analyize intervals with basic method" << std::endl;
            eachBucketSet.clear();
            databack.clear();
            // m_logger.SetStartTimer();
            parallelGetInterval(kCiperTextMp);

            AES aes(AESKeyLength::AES_128);
            eachBucketSet.resize(siloNum);
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i];
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key.at(i), aes_iv.at(i));
                size_t k = (size_t)UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                size_t bucketSize = (size_t)std::ceil(std::sqrt(queryK));
                int num = (k + bucketSize - 1) / bucketSize;
                std::vector<std::pair<float, float> > buckets;
                for(int j = 0;j < num;j++) {
                    float l = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 8 * j, plainText.begin() + 4 + 8 * j + 4));
                    float r = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 8 * j + 4, plainText.begin() + 4 + 8 * j + 8));
                    buckets.emplace_back(std::make_pair(l, r));
                }
                for(int j = 0;j < num;j++) {
                    float l = buckets[j].first, r = buckets[j].second;
                    if(j == 0) {
                        eachBucketSet[i].emplace_back(new bucket(-FLOAT_INF, l, 0, 0, 2));
                    } 
                    if (j == num - 1) {
                        eachBucketSet[i].emplace_back(new bucket(l, r, j * bucketSize + 1, k));
                        eachBucketSet[i].emplace_back(new bucket(r, FLOAT_INF, k, k, 2));
                    } else {
                        eachBucketSet[i].emplace_back(new bucket(l, r, j * bucketSize + 1, (j + 1) * bucketSize));
                        eachBucketSet[i].emplace_back(new bucket(r, buckets[j + 1].first, (j + 1) * bucketSize, (j + 1) * bucketSize, 2));
                    }
                }
            }
            return;
        }

        void analyzeIntervalOpt(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const std::map<size_t, EncryptData> &kCiperTextMp, const int queryK) {
            std::cout << "analyize intervals with priority queue" << std::endl;
            optBucketSet.clear();
            databack.clear();
            parallelGetInterval(kCiperTextMp);
            AES aes(AESKeyLength::AES_128);
            optBucketSet.resize(siloNum);
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i];
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key.at(i), aes_iv.at(i));
                size_t k = (size_t)UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                size_t bucketSize = (size_t)std::ceil(std::sqrt(queryK));
                int num = (k + bucketSize - 1) / bucketSize;
                for(int j = 0;j < num;j++) {
                    float radius = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 4 * j, plainText.begin() + 8 + 4 * j));
                    if(j == num - 1) {
                        optBucketSet[i].emplace_back(new optBucket(radius, k - j * bucketSize, i));
                    } else {
                        optBucketSet[i].emplace_back(new optBucket(radius, bucketSize, i));
                    }
                }
            }
        }

        void analyzeInterval(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const std::map<size_t, EncryptData> &kCiperTextMp, const int queryK) {
            std::cout << "analyize intervals with intervals merged" << std::endl;
            bucketSet.clear();
            databack.clear();
            // m_logger.SetStartTimer();
            parallelGetInterval(kCiperTextMp);
            // m_logger.SetEndTimer();
            // std::cout << "get interval takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;

            AES aes(AESKeyLength::AES_128);
            std::vector<boundary> boundarySet;
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i];
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key.at(i), aes_iv.at(i));
                size_t k = (size_t)UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                size_t bucketSize = (size_t)std::ceil(std::sqrt(queryK));
                int num = (k + bucketSize - 1) / bucketSize;
                if(num == 1) {
                    float bound = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4, plainText.begin() + 8));
                    boundarySet.emplace_back((boundary){bound, 1, k});
                } else {
                    for(int j = 0;j < num;j++) {
                        float bound = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 4 * j, plainText.begin() + 8 + 4 * j));
                        if(j == 0) {
                            boundarySet.emplace_back((boundary){bound, 1, bucketSize});
                        } else if(j == num - 1) {
                            boundarySet.emplace_back((boundary){bound, bucketSize, k - (bucketSize) * j});
                        } else {
                            boundarySet.emplace_back((boundary){bound, bucketSize, bucketSize});
                        }
                    }
                }
            }
            sort(boundarySet.begin(), boundarySet.end());
            int numL = 0;
            int numR = 0;
            for(size_t i = 0;i < boundarySet.size();i++) {
                float nowL = boundarySet[i].bound;
                numL += boundarySet[i].l_numbouns;
                numR += boundarySet[i].r_numbouns;
                while(i + 1 < boundarySet.size() && boundarySet[i + 1] == boundarySet[i]) {
                    i++;
                    numL += boundarySet[i].l_numbouns;
                    numR += boundarySet[i].r_numbouns;
                }
                float nowR = (i == (boundarySet.size() - 1)) ? FLOAT_INF : boundarySet[i + 1].bound;
                bucketSet.emplace_back(new bucket(nowL, nowR, numL, numR));
            }
        }

        void analyzeIntervalWithOutMerge(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const std::map<size_t, EncryptData> &kCiperTextMp, const int queryK) {
            std::cout << "analyize the intervals without intervals merged" << std::endl;
            eachBucketSet.clear();
            databack.clear();
            // m_logger.SetStartTimer();
            parallelGetInterval(kCiperTextMp);
            // m_logger.SetEndTimer();
            // std::cout << "get interval takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;
            AES aes(AESKeyLength::AES_128);
            std::vector<std::vector<boundary> > boundarySet(siloNum);
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i];
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key.at(i), aes_iv.at(i));
                size_t k = (size_t)UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                size_t bucketSize = (size_t)std::ceil(std::sqrt(queryK));
                int num = (k + bucketSize - 1) / bucketSize;
                if(num == 1) {
                    float bound = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4, plainText.begin() + 8));
                    boundarySet[i].emplace_back((boundary){bound, 1, k});
                } else {
                    for(int j = 0;j < num;j++) {
                        float bound = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 4 * j, plainText.begin() + 8 + 4 * j));
                        if(j == 0) {
                            boundarySet[i].emplace_back((boundary){bound, 1, bucketSize});
                        } else if(j == num - 1) {
                            boundarySet[i].emplace_back((boundary){bound, bucketSize, k - (bucketSize) * j});
                        } else {
                            boundarySet[i].emplace_back((boundary){bound, bucketSize, bucketSize});
                        }
                    }
                }
                std::sort(boundarySet[i].begin(), boundarySet[i].end());
            }
            eachBucketSet.resize(siloNum);
            for(size_t i = 0;i < siloNum;i++) {
                int numL = 0;
                int numR = 0;
                for(size_t j = 0;j < boundarySet[i].size();j++) {
                    float nowL = boundarySet[i][j].bound;
                    numL += boundarySet[i][j].l_numbouns;
                    numR += boundarySet[i][j].r_numbouns;
                    while(j + 1 < boundarySet[i].size() && boundarySet[i][j + 1] == boundarySet[i][j]) {
                        j++;
                        numL += boundarySet[i][j].l_numbouns;
                        numR += boundarySet[i][j].r_numbouns;
                    }
                    float nowR = (j == (boundarySet[i].size() - 1)) ? FLOAT_INF : boundarySet[i][j + 1].bound;
                    eachBucketSet[i].emplace_back(new bucket(nowL, nowR, numL, numR));
                }
                assert(eachBucketSet[i].back()->r == FLOAT_INF);
            }
        }

        float analyzeIntervalZero(std::map<size_t, std::vector<unsigned char>> &aes_key,
            std::map<size_t, std::vector<unsigned char>> &aes_iv, const std::map<size_t, EncryptData> &kCiperTextMp, const int queryK) {
            std::cout << "exactly send tuples" << std::endl;
            databack.clear();
            // m_logger.SetStartTimer();
            parallelGetInterval(kCiperTextMp);
            return FLOAT_INF;
        }

        float binarySearchBase(int queryK) {
            float l = 0, r = 0;
            for(size_t i = 0;i < siloNum;i++) {
                for(size_t j = 0;j < eachBucketSet[i].size();j++) {
                    r = std::max(eachBucketSet[i][j]->l, r);
                }
            }
            r += 1;
            const float eps = 1e-3;
            std::vector<int> id2pos; //record each id's interval id 
            while(r - l > eps) {
                float mid = (r + l) / 2;
                size_t Lnum = 0, Rnum = 0;
                id2pos.clear();
                for(size_t i = 0;i < siloNum;i++) {
                    size_t L = 0, R = eachBucketSet[i].size() - 1;
                    size_t ans = -1;
                    while(L <= R) {
                        size_t Mid = (L + R) / 2;
                        if((eachBucketSet[i][Mid]->op == 1 && (eachBucketSet[i][Mid]->l <= mid && mid <= eachBucketSet[i][Mid]->r)) ||
                        (eachBucketSet[i][Mid]->op == 2 && (eachBucketSet[i][Mid]->l < mid && mid < eachBucketSet[i][Mid]->r))) {
                            ans = Mid;
                            break;
                        } else if(eachBucketSet[i][Mid]->l > mid) {
                            R = Mid - 1;
                        } else {
                            L = Mid + 1;
                        }
                    }
                    if(ans != -1) {
                        Lnum += eachBucketSet[i][ans]->numL;
                        Rnum += eachBucketSet[i][ans]->numR;
                    }
                    id2pos.emplace_back(ans);
                }
                if(Lnum <= queryK && Rnum >= queryK) {
                    float radius = 0;
                    for(int i = 0;i < siloNum;i++) {
                        int id = id2pos[i];
                        if(id == -1) continue;
                        radius = std::max(radius, eachBucketSet[i][id]->r);
                    }
                    return radius;
                } else if(queryK < Lnum) {
                    r = mid;
                } else {
                    l = mid;
                }
            }
            return FLOAT_INF;
        }

        float binarySearchOpt(int queryK) {
            std::priority_queue<std::pair<optBucket, size_t>, std::vector<std::pair<optBucket, size_t>>, std::greater<std::pair<optBucket, size_t>>> q;
            for(int i = 0;i < siloNum;i++) {
                if(optBucketSet[i].empty()) continue;
                q.push(std::make_pair(*(optBucketSet[i][0]), 0));
            }
            int cnt = 0;
            float radius = 0;
            while(cnt < queryK && !q.empty()) {
                auto item = q.top();
                q.pop();
                cnt += item.first.num;
                radius = std::max(radius, item.first.r);
                size_t siloId = item.first.siloId;
                if(item.second + 1 < optBucketSet[siloId].size()) {
                    q.push(std::make_pair(*(optBucketSet[siloId][item.second + 1]), item.second + 1));
                }
            }
            return radius;
        }

        float binarySearchMultiSilo(int queryK) {
            float l = 0, r = 0;
            for(size_t i = 0;i < siloNum;i++) {
                for(size_t j = 0;j < eachBucketSet[i].size();j++) {
                    r = std::max(eachBucketSet[i][j]->l, r);
                }
            }
            std::cout << "the max radius is " << r << std::endl;
            r += 1; // expand r
            const float eps = 1e-3;
            std::vector<int> id2pos; //record each id's interval id 
            while(r - l > eps) {
                float mid = (r + l) / 2;
                size_t Lnum = 0, Rnum = 0;
                id2pos.clear();
                for(size_t i = 0;i < siloNum;i++) {
                    if(mid < eachBucketSet[i][0]->l) {
                        id2pos.emplace_back(-1);
                    } else {
                        size_t L = 0, R = eachBucketSet[i].size() - 1;
                        size_t ans;
                        while(L <= R) {
                            size_t Mid = (L + R) / 2;
                            if(eachBucketSet[i][Mid]->l <= mid && mid < eachBucketSet[i][Mid]->r) {
                                ans = Mid;
                                break;
                            } else if(eachBucketSet[i][Mid]->l > mid) {
                                R = Mid - 1;
                            } else {
                                L = Mid + 1;
                            }
                        }
                        Lnum += eachBucketSet[i][ans]->numL;
                        Rnum += eachBucketSet[i][ans]->numR;
                        id2pos.emplace_back(ans);
                    }
                }
                if(Lnum <= queryK && Rnum >= queryK) {
                    float minBound = FLOAT_INF;
                    size_t minPos = -1;
                    for(size_t i = 0;i < siloNum;i++) {
                        if(id2pos[i] == -1) continue;
                        int nowid = id2pos[i];
                        if(eachBucketSet[i][nowid]->r < minBound) {
                            minPos = i;
                            minBound = eachBucketSet[i][nowid]->r;
                        }
                    }
                    if(minPos == -1) {
                        return minBound;
                    } else {
                        for(size_t i = 0;i < siloNum;i++) {
                            int nowid = id2pos[i];
                            if(nowid == -1) continue;
                            if(eachBucketSet[i][nowid]->r == minBound) {
                                Lnum += eachBucketSet[i][nowid + 1]->numL - eachBucketSet[i][nowid]->numL;
                                Rnum += eachBucketSet[i][nowid + 1]->numR - eachBucketSet[i][nowid]->numR;
                            }
                        }
                        if(Lnum > queryK) {
                            return minBound;
                        } else {
                            l = mid;
                        }
                    }
                } else if(Lnum > queryK) {
                    r = mid;
                } else {
                    l = mid;
                }
            }
            return r;
        }

        float binarySearch(int queryK) {
            int l = 0, r = (int)bucketSet.size() - 1;
            float ans;
            while(l <= r) {
                int mid = (l+r)/2;
                if(bucketSet[mid]->numL <= queryK && bucketSet[mid]->numR >= queryK) {
                    if(mid == ((int)bucketSet.size() - 1) || bucketSet[mid + 1]->numL > queryK) {
                        ans = bucketSet[mid]->r;
                        break;
                    } else {
                        l = mid + 1;
                    }
                } else if(bucketSet[mid]->numL > queryK) {
                    r = mid - 1;
                } else {
                    l = mid + 1;
                }
            }
            if(ans == FLOAT_INF) {
                ans = 0;
            }
            return ans;
        }

        void executeKNN(const VectorDataType &queryV, const size_t queryK, const std::string &condition) {
            secretKeyExchange();
            //initialize aes
            std::map<size_t, std::vector<unsigned char>> aes_key, aes_iv;
            for(size_t i = 0;i < siloNum;i++) {
                aes_key[i] = std::vector<unsigned char>();
                aes_iv[i] = std::vector<unsigned char>();
            }
            for(size_t i = 0;i < siloNum;i++) { //cal key
                for (int j = 0; j < 4; ++j) {
                    std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i][j]);
                    aes_key[i].insert(aes_key[i].end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
                }
                for (int j = 4; j < 8; ++j) {
                    std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i][j]);
                    aes_iv[i].insert(aes_iv[i].end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
                }
            }
            AES aes(AESKeyLength::AES_128);
            std::vector<size_t> kSet;
            
            // contribution analysis
            m_logger.SetStartTimer();
            analyizeContribution(aes_key, aes_iv, queryV, queryK, condition, kSet);
            for(size_t i = 0;i < kSet.size();i++) {
                std::cout << kSet[i] << " ";
            }
            std::cout << std::endl;
            m_logger.SetEndTimer();
            std::cout << "contribution analysis takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;

            float radius;
            std::map<size_t, EncryptData> ciperMp; 
            for(size_t i = 0;i < siloNum;i++) {
                size_t prunedK = kSet[i];
                std::vector<unsigned char> plainText = Int32ToUnsignedVector((int)prunedK);
                if (plainText.size() % 16 != 0) {
                    // std::cout << "[BEFORE Padding] plainText.size() = " << plainText.size() << std::endl;
                    for (int c = plainText.size() % 16; c < 16; ++c) {
                        plainText.emplace_back('\0');
                    }
                    // std::cout << "[AFTER Padding] plainText.size() = " << plainText.size() << std::endl;
                }
                std::vector<unsigned char> ciperText = aes.EncryptCBC(plainText, aes_key[i], aes_iv[i]);
                EncryptData data;
                data.set_data(std::string(ciperText.begin(), ciperText.end()));
                ciperMp[i] = data; 
            }
            
            m_logger.SetStartTimer();
            if(topKOption > 1) {    
                if(topKOption == 2) {
                    analyzeIntervalWithOutMerge(aes_key, aes_iv, ciperMp, queryK);
                    radius = binarySearchMultiSilo(queryK);
                } else if(topKOption == 3){
                    analyzeInterval(aes_key, aes_iv, ciperMp, queryK);
                    radius = binarySearch(queryK);
                } else if(topKOption == 4) {
                    analyzeIntervalBase(aes_key, aes_iv, ciperMp, queryK);
                    radius = binarySearchBase(queryK);
                } else if(topKOption == 5) {
                    analyzeIntervalOpt(aes_key, aes_iv, ciperMp, queryK);
                    radius = binarySearchOpt(queryK);
                }
            } else {
                radius = analyzeIntervalZero(aes_key, aes_iv, ciperMp, queryK);
            }
            m_logger.SetEndTimer();
            optTopKTime += m_logger.GetDurationTime();
            std::cout << "interval analysis and milvus query takes " << m_logger.GetDurationTime() << "ms" << std::endl;
            
            // get (dis, id) Set
            m_logger.SetStartTimer();    
            databack.clear();
            std::vector<unsigned char> plainText = FloatToUnsignedVector(radius);
            if (plainText.size() % 16 != 0) {
                // std::cout << "[BEFORE Padding] plainText.size() = " << plainText.size() << std::endl;
                for (int c = plainText.size() % 16; c < 16; ++c) {
                    plainText.emplace_back('\0');
                }
                // std::cout << "[AFTER Padding] plainText.size() = " << plainText.size() << std::endl;
            }
            ciperMp.clear();
            for(size_t i = 0;i < siloNum;i++) {
                std::vector<unsigned char> ciperText = aes.EncryptCBC(plainText, aes_key[i], aes_iv[i]);
                EncryptData data;
                data.set_data(std::string(ciperText.begin(), ciperText.end()));
                ciperMp[i] = data; 
            }
            parallelGetDisInf(ciperMp);
            m_logger.SetEndTimer();
            std::cout << "get dis Set takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;

            // final top-k with pq 
            m_logger.SetStartTimer();
            std::priority_queue<std::pair<float, size_t>, std::vector<std::pair<float, size_t>>, std::greater<std::pair<float, size_t>>> q;
            std::vector<std::vector<float>> disSet;
            disSet.resize(siloNum);
            for(size_t i = 0;i < siloNum;i++) {
                EncryptData data = databack[i]; 
                std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(data.data()), aes_key[i], aes_iv[i]);
                int cnt = UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
                for(int j = 0;j < cnt;j++) {
                    float dis = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin() + 4 + 4 * j, plainText.begin() + 8 + 4 * j));
                    // std::cout << dis << " ";
                    disSet[i].emplace_back(dis); 
                }
                // std::cout << std::endl;
            }
            std::map<size_t, int> posMp;
            std::map<size_t, int> numMp;
            for(int i = 0;i < siloNum;i++) {
                posMp[i] = numMp[i] = 0;
                if(posMp[i] < disSet[i].size()) {
                    q.push(std::make_pair(disSet[i][posMp[i]], i));
                    posMp[i]++;
                }
            }
            int cnt = 0;
            while(cnt < queryK && !q.empty()) {
                auto item = q.top();
                q.pop();
                size_t siloId = item.second;
                numMp[siloId]++;
                cnt++;
                if(posMp[siloId] < disSet[siloId].size()) {
                    q.push(std::make_pair(disSet[siloId][posMp[siloId]], siloId));
                    posMp[siloId]++;
                }
            }
            std::map<size_t, Number> newMp;
            for(size_t i = 0;i < siloNum;i++) {
                // std::cout << numMp[i] << " ";
                Number nowNum;
                nowNum.set_num(numMp[i]);
                newMp[i] = nowNum;
            }
            // std::cout << std::endl;
            parallelsendK(newMp);
            m_logger.SetEndTimer();
            std::cout << "final top-k with pq takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;
        }

        Status requestKNN(ServerContext* context, const userbroker::knnQuery* request, Empty* response) {
            size_t dim = request->data_size();
            VectorDataType queryV(dim, request->qid());
            for(int i = 0;i < dim;i++) {
                queryV[i] = request->data(i);
            }
            size_t queryK = request->k();
            std::string cond = request->condition();
            m_logger.LogAddComm(request->ByteSizeLong());
            executeKNN(queryV, queryK, cond);
            return Status::OK;
        }

        Status requestComm(ServerContext* context, const Empty* request, rpcComm* response) override {
            for(int i = 0;i < siloNum;i++) {
                m_logger.LogAddComm(siloPtr[i]->getComm());
            }
            response->set_rpcdatanum((int64_t)m_logger.GetQueryComm());
            m_logger.Init();
            return Status::OK;
        } 
};

std::unique_ptr<UserBrokerImpl> brokerPtr = nullptr;

void RunService(std::string ipAddr ,std::string ipAddrFile) {
    brokerPtr = std::make_unique<UserBrokerImpl>(ipAddrFile);
    ServerBuilder builder;
    builder.AddListeningPort(ipAddr, grpc::InsecureServerCredentials());
    builder.RegisterService(brokerPtr.get());
    builder.SetMaxSendMessageSize(INT_MAX);
    builder.SetMaxReceiveMessageSize(INT_MAX);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Broker listening on " << ipAddr << std::endl;
    server->Wait();
    std::cout.flush();
}

int main(int argc, char** argv) {
    std::string ipAddrFile, ipAddr;
    try { 
        bpo::options_description option_description("Required options");
        option_description.add_options()
            ("help", "produce help message")
            ("broker-ip", bpo::value<std::string>(), "Broker's IP address")
            ("silo-ip", bpo::value<std::string>(), "Data silo's IP configuration file path");

        bpo::variables_map variable_map;
        bpo::store(bpo::parse_command_line(argc, argv, option_description), variable_map);
        bpo::notify(variable_map);    

        if (variable_map.count("help")) {
            std::cout << option_description << std::endl;
            return 0;
        }

        bool options_all_set = true;

        if (variable_map.count("silo-ip")) {
            ipAddrFile = variable_map["silo-ip"].as<std::string>();
            std::cout << "Data silo's IP configuration file path was set to " << ipAddrFile << "\n";
        } else {
            std::cout << "Data silo's IP configuration file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("broker-ip")) {
            ipAddr = variable_map["broker-ip"].as<std::string>();
            std::cout << "Broker's IP address was set to " << ipAddr << "\n";
        } else {
            std::cout << "Broker's IP address was not set" << "\n";
            options_all_set = false;
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
    RunService(ipAddr, ipAddrFile);
}