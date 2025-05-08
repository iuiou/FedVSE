#include <grpcpp/grpcpp.h>
#include <grpc/grpc.h>
#include <vector>
#include <string>
#include <queue>
#include <cassert>
#include <boost/program_options.hpp>
#include <signal.h>
namespace bpo = boost::program_options;

#include "utils/DataType.hpp"
#include "utils/MetricType.hpp"
#include "utils/BenchLogger.hpp"
#include "utils/File_IO.h"
#include "middleware/BaseSiloConnector.hpp"
#include "middleware/MilvusSiloConnector.hpp"

#include "protolib/BrokerSilo.pb.h"
#include "protolib/BrokerSilo.grpc.pb.h"
#include "crypto/AES.h"
#include "crypto/util.hpp"
#include "pgm/pgm_interface.hpp"
#include "cluster/clusterBuilder.hpp"
#include "global.hpp"

const int MIN = -INT32_MAX;
const int MAX = INT32_MAX;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using google::protobuf::Empty;
using brokersilo::BrokerSilo;
using brokersilo::DiffieHellmanParams;
using brokersilo::DiffieHellmanRg;
using brokersilo::EncryptData;
using brokersilo::knnQuery;
using brokersilo::Vector;
using brokersilo::Number;

#define recordIndexBuilding

std::string MilvusPort = "19530";
bool buildClusterOption = false;
bool buildMilvusOption = false;
float alpha = 0.05;
int clusterNumber = 10;

size_t avgK = 0;
size_t minK = 1e9;

class BrokerSiloImpl final : public BrokerSilo::Service {
    private:
        std::unique_ptr<MilvusSiloConnector<Distance> > siloPtr;
        uint64_t p, g;
        std::vector<uint64_t> shared_secret_key_list; //aes key
        std::unique_ptr<VectorDataType> queryV;
        std::string condition;
        size_t queryK;
        std::vector<std::string> attributes; // ith attribute order
        std::vector<std::pair<float, std::shared_ptr<VectorDataType> >> candidates; // dis, Vector Inf
        std::vector<std::string> candAttributes; // candidates' value
        int ansNum; // ans's Num
        std::vector<std::unique_ptr<pgmIndex> > pgmIndexSet; // ith cluster's pgmIndex
        std::vector<std::unique_ptr<cluster> > clusters; // ith cluster
        BenchLogger m_logger;

    public:
    explicit BrokerSiloImpl(const int siloId, const std::string& ipAddr, const std::string& dataName, 
    const std::string& scalarName, const std::string& clusterName, const std::string& clusterAddName, 
    const std::string& collectionName, const std::string& indexType) {
        //build milvus
        m_logger.Init();
        siloPtr = std::make_unique<MilvusSiloConnector<Distance>>(siloId, ipAddr, collectionName);
        siloPtr->ImportScalarData(scalarName);
        siloPtr->ConnectDB("fzh", "fzh", "123", "localhost", MilvusPort); // only the last two attributes make sense
        if(buildMilvusOption) {
            siloPtr->ImportData(dataName);
            siloPtr->ConstructIndex(indexType);
            siloPtr->importDataToMilvus();
        }
        siloPtr->LoadData();

        //build clusters and load clusters
        if(buildClusterOption) {
            if(!buildMilvusOption) siloPtr->ImportData(dataName);
            const int iteration = 10;
            m_logger.SetStartTimer();
            std::cout << "start k means" << std::endl;
            std::unique_ptr<AvgKmeans> KmeansOperator = std::make_unique<AvgKmeans>(siloPtr->getVectorData(), clusterNumber, iteration);
            std::cout << "end k means" << std::endl;
            KmeansOperator->dumpToFile(clusterName);
            KmeansOperator->dumpClusterToFile(clusterAddName);
            m_logger.SetEndTimer();

            #ifdef recordIndexBuilding
                std::cout << "the process of clustering takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;
            #endif
        }
        
        m_logger.SetStartTimer();
        std::vector<std::vector<int>> clusterSet;
        loadClusters(clusterSet, clusterName, clusterAddName);
        attributes = siloPtr->getScalarName();
        buildPGMIndex(clusterSet);
        m_logger.SetEndTimer();

        #ifdef recordIndexBuilding
            std::cout << "the process of build pgm index takes " << m_logger.GetDurationTime() << "ms" << std::endl;
            long indexSize = 0;
            for(size_t i = 0;i < clusterSet.size();i++) {
                indexSize += sizeof(*(pgmIndexSet[i]));
                indexSize += sizeof(*(clusters[i]));
            }
            std::cout << "hybrid index's size: " << 1.0 * indexSize / 1024.0 << "KB" << std::endl;
        #endif
        queryV = nullptr;
    }

    void loadClusters(std::vector<std::vector<int>> &clusterSet, const std::string& clusterName, const std::string& clusterAddName) {
        std::ifstream file(clusterName);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading cluster information" << clusterName << std::endl;
            std::exit(EXIT_FAILURE);
        }
        int clusterNum, vid;
        file >> clusterNum; 
        clusterSet.resize(clusterNum);
        for(int i = 0;i < clusterNum;i++) {
            int number;
            file >> number;
            clusterSet[i].resize(number);
            for(int j = 0;j < number;j++) {
                file >> vid;
                clusterSet[i][j] = vid;
            }
        }
        file.close();
        for(int i = 0;i < clusterNum;i++) {
            std::string clusterInfFile = clusterAddName + std::to_string(i);
            std::unique_ptr<cluster> c = std::make_unique<cluster>(clusterInfFile);
            clusters.emplace_back(std::move(c)); 
        }
    }

    void buildPGMIndex(std::vector<std::vector<int> > &clusterSet) {
        std::cout << "start build pgm index" << std::endl;
        size_t clusterNum = clusterSet.size();
        size_t scalarNum = siloPtr->scalarSize();
        if(scalarNum == 1) {
            std::vector<int> attr1 = siloPtr->getScalarData(0);
            for(size_t i = 0;i < clusterNum;i++) {
                std::vector<int> data(clusterSet[i].size());
                for(size_t j = 0;j < clusterSet[i].size();j++) {
                    int vid = clusterSet[i][j];
                    data[j] = attr1[vid];
                }
                std::sort(data.begin(), data.end());
                std::unique_ptr<pgmIndex> index = std::make_unique<oneDimPGM>(data);
                pgmIndexSet.emplace_back(std::move(index));
            }
        } else if(scalarNum == 2) {
            std::vector<int> attr1 = siloPtr->getScalarData(0);
            std::vector<int> attr2 = siloPtr->getScalarData(1);
            for(size_t i = 0;i < clusterNum;i++) {
                std::vector<std::tuple<uint32_t, uint32_t>> data(clusterSet[i].size());
                for(size_t j = 0;j < clusterSet[i].size();j++) {
                    int vid = clusterSet[i][j];
                    data[j] = std::make_tuple((uint32_t)attr1[vid], (uint32_t)attr2[vid]);
                }
                std::unique_ptr<pgmIndex> index = std::make_unique<twoDimPGM>(data);
                pgmIndexSet.emplace_back(std::move(index));
            }
        } else {
            std::cerr << "attribute number is larger than limited" << clusterNum << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    std::tuple<std::string, int, int> parseUnEqualExp(std::string &token) {
        int ptr = 0;
        std::string digit = "";
        int l, r;
        while(std::isdigit(token[ptr])){
            digit.push_back(token[ptr]);
            ptr++;
        } 
        l = std::atoi(digit.data());
        ptr++;
        ptr++; // <=
        std::string name = "";
        while(std::isalpha(token[ptr])) {
            name.push_back(token[ptr]);
            ptr++;
        }  
        ptr++;
        ptr++; // <=
        digit = "";
        while(ptr < token.size() && std::isdigit(token[ptr])) {
            digit.push_back(token[ptr]);
            ptr++;
        }
        r = std::atoi(digit.data());
        return std::make_tuple(name, l, r);
    }

    std::pair<std::string, std::string> parseEqualExp(std::string token) {
        int ptr = 0;
        std::string name = "";
        while(std::isalpha(token[ptr])) {
            name.push_back(token[ptr]);
            ptr++;
        }
        ptr++;
        ptr++; //="
        std::string value = "";
        while(token[ptr] != '\"') {
            value.push_back(token[ptr]);
            ptr++;
        }
        return std::make_pair(name, value);
    }

    void parseQuery(std::vector<std::pair<int,int> > &conditionList) {
        std::istringstream iss(condition);
        std::vector<std::string> tokenSet;
        std::string token;
        while(std::getline(iss, token, ' ')) {
            tokenSet.emplace_back(token);
        }
        condition = "";
        if(std::isdigit(tokenSet[0][0])) { // unequal exp
            std::tuple<std::string, int, int> cond = parseUnEqualExp(tokenSet[0]);
            conditionList.emplace_back(std::make_pair(std::get<1>(cond), std::get<2>(cond)));
            condition += tokenSet[0];
        } else {
            std::pair<std::string, std::string> cond = parseEqualExp(tokenSet[0]);
            int idValue = siloPtr->getStringId(0, std::get<1>(cond));
            conditionList.emplace_back(std::make_pair(idValue, idValue));
            condition += cond.first + "==\"" + cond.second + "\"";
        }
        if((int)tokenSet.size() > 1) {
            condition += " and ";
            assert(tokenSet[1] == "and");
            if(std::isdigit(tokenSet[2][0])) {
                std::tuple<std::string, int, int> cond = parseUnEqualExp(tokenSet[2]);
                conditionList.emplace_back(std::make_pair(std::get<1>(cond), std::get<2>(cond)));
                condition += tokenSet[2];
            } else {
                std::pair<std::string, std::string> cond = parseEqualExp(tokenSet[2]);
                int idValue = siloPtr->getStringId(1, std::get<1>(cond));
                conditionList.emplace_back(std::make_pair(idValue, idValue));
                condition += cond.first + "==\"" + cond.second + "\"";
            }
        }
    }

    Status GetParams(ServerContext* context, const Empty* request,  
                        DiffieHellmanParams* response) override {
        p = 797546779;// 质数 
        g = 3; // 原根
        response->set_p(p);
        response->set_g(g);
        shared_secret_key_list.clear(); //reset the aes key
        return Status::OK;
    }

    Status GetRandom(ServerContext* context, const DiffieHellmanRg* request,  
                        DiffieHellmanRg* response) override {
        // 从1到p-2之间随机生成一个a
        uint64_t a = sample_random(1, p-2);
        uint64_t A = mod_pow(g, a, p);
        uint64_t B = request->rg_mod_p();
        response->set_rg_mod_p(A);

        // 计算共享密钥 B^a%p  
        uint64_t shared_secret_key = mod_pow(B, a, p);
        shared_secret_key_list.emplace_back(shared_secret_key);
        return Status::OK;
    }

    float globalEstimate(std::vector<std::pair<int,int>> &conditionList) {
        std::map<int, float> disSet;
        for(const auto &c : clusters) {
            float dis = c->caldis(*queryV);
            disSet[c->getClusterId()] = dis;
        }
        int count = 0,sum = 0;
        for(const auto &item : disSet) {
            count += pgmIndexSet[item.first]->rangeCount(conditionList);
            sum += pgmIndexSet[item.first]->getN();
        }
        float sel = 1.0 * count / sum;
        return sel;
    }

    float clusterEstimate(std::vector<std::pair<int,int>> &conditionList) {
        float disMin = FLOAT_INF;
        std::map<int, float> disSet;
        for(const auto &c : clusters) {
            float dis = c->caldis(*queryV);
            disMin = std::min(disMin, dis);
            disSet[c->getClusterId()] = dis;
        }
        float radius = disMin * (1 + alpha);
        std::vector<int> clustersId;
        std::vector<std::vector<clusterInf> > disIntervals;
        int count = 0, sum = 0;
        for(const auto &item : disSet) {
            if(item.second <= radius) {
                int cid = item.first;
                clustersId.emplace_back(cid);
                count += pgmIndexSet[cid]->rangeCount(conditionList);
                sum += pgmIndexSet[cid]->getN();
                std::vector<clusterInf> nowCluster;
                clusters[cid]->genDisSet(*queryV, nowCluster, item.second);
                disIntervals.emplace_back(nowCluster);
            }
        }
        // std::cout << "Filtered clusters' number is " << disIntervals.size() << std::endl;
        float sel = 1.0 * count / sum;
        if(sel == 0) {
            return FLOAT_INF;
        }
        int newK = (int)std::ceil(1.0 * queryK / sel);
        float maxRadius = 0;
        std::priority_queue<std::pair<clusterInf, size_t>, std::vector<std::pair<clusterInf, size_t>>, std::greater<std::pair<clusterInf, size_t>>> q;
        std::map<size_t, int> posMp;
        for(size_t i = 0;i < disIntervals.size();i++) {
            posMp[i] = 0;
            if(posMp[i] < disIntervals[i].size()) {
                maxRadius = std::max(maxRadius, disIntervals[i].back().bound);
                q.push(std::make_pair(disIntervals[i][posMp[i]], i));
                posMp[i]++;    
            }
        }
        if(newK > sum) {
            return maxRadius;
        }
        int cnt = 0;
        radius = 0;
        while(cnt < newK && !q.empty()) {
            auto item = q.top();
            radius = std::max(radius, item.first.bound);
            q.pop();
            size_t clusterId = item.second;
            posMp[clusterId]++;
            cnt += item.first.num;
            if(posMp[clusterId] < disIntervals[clusterId].size()) {
                q.push(std::make_pair(disIntervals[clusterId][posMp[clusterId]], clusterId));
                posMp[clusterId]++;
            }
        }
        // std::sort(disIntervals.begin(), disIntervals.end());
        
        // std::vector<std::pair<float, int> > accum;
        // size_t intervalSize = disIntervals.size();
        // int ub = 0;
        // for(size_t i = 0;i < intervalSize;i++) {
        //     ub += disIntervals[i].num;
        //     while(i + 1 < intervalSize && disIntervals[i + 1].bound == disIntervals[i].bound) {
        //         i++;
        //         ub += disIntervals[i].num;
        //     }
        //     float bound = disIntervals[i].bound;
        //     accum.emplace_back(std::make_pair(bound, ub));
        // }
        // // std::cout << "the last distance : " << accum.back().first << std::endl;
        // // std::cout << "the last vector point number : " << accum.back().second << std::endl;
        // int l = 0,r = (int)accum.size() - 1;
        // float ans = -1;
        // while(l <= r) {
        //     int mid = (l + r) / 2;
        //     if(accum[mid].second >= newK) {
        //         ans = accum[mid].first;
        //         r = mid - 1;
        //     } else {
        //         l = mid + 1;
        //     }
        // }
        // if(ans == -1) { // silo中的点都包含也无法达到newK，即无法提供那么多点
        //     return accum.back().first;
        // }
        // std::cout << "the selectivity is " << sel << std::endl;
        // std::cout << "the approximated k is " << newK << std::endl;
        // std::cout << "contribution radius is " << radius << std::endl;
        return radius;
    }

    float evaluateUpperbound() {
        std::vector<std::pair<int,int>> conditionList;
        parseQuery(conditionList);
        // m_logger.SetStartTimer();
        // std::cout << "now condition is " << condition << std::endl;
        if(localSearchOption == 1) { // exact
            return -1;
        } else if(localSearchOption == 2) { // use cluster to estimate
            float ans = clusterEstimate(conditionList);
            // m_logger.SetEndTimer();
            // std::cout << m_logger.GetDurationTime() << "ms" << std::endl;
            return ans;
        }
    }

    void exactKNN(size_t k) {
        m_logger.SetStartTimer();
        std::vector<VectorDataType> ansV;
        std::vector<float> ansD;
        std::vector<std::string> ansA;
        siloPtr->KnnQuery(*queryV, k, ansV, ansD, ansA, condition);
        m_logger.SetEndTimer();
        for(size_t i = 0;i < ansV.size();i++) {
            std::shared_ptr<VectorDataType> v = std::make_shared<VectorDataType>(ansV[i].Dimension(), ansV[i].vid, ansV[i]);
            float dis = ansD[i];
            // std::cout << dis << " ";
            candidates.emplace_back(std::make_pair(dis, v));
            candAttributes.emplace_back(ansA[i]);
        }
        // std::cout << std::endl;
        std::cout << "access the milvus and query " << k << " vectors takes " << m_logger.GetDurationTime() / 1000 << "s" << std::endl;
        return;
    }

    Status evaluateK(ServerContext* context, const knnQuery* request,  
                        EncryptData* response) override {
        m_logger.SetStartTimer();
        
        size_t dim = request->data_size();
        queryV = std::make_unique<VectorDataType>(dim, request->qid());
        for(int i = 0;i < dim;i++) {
            (*queryV)[i] = request->data(i);
        }
        queryK = request->k();
        condition = request->condition();
        float upperb = evaluateUpperbound();
        std::vector<unsigned char> aes_key, aes_iv;
        for (int i=0; i<=3; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_key.insert(aes_key.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        for (int i=4; i<=7; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_iv.insert(aes_iv.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        std::vector<unsigned char> plainText = FloatToUnsignedVector(upperb);
        AES aes(AESKeyLength::AES_128);
        padding(plainText);
        std::vector<unsigned char> ciperText = aes.EncryptCBC(plainText, aes_key, aes_iv);
        response->set_data(std::string(ciperText.begin(), ciperText.end()));
        double grpc_comm = request->ByteSizeLong() + response->ByteSizeLong();
        m_logger.LogAddComm(grpc_comm);

        m_logger.SetEndTimer();
        m_logger.LogAddTime();
        return Status::OK;
    }

    Status requestInterval(ServerContext* context, const EncryptData* request,  
                        EncryptData* response) override {
        m_logger.SetStartTimer();

        std::vector<unsigned char> aes_key, aes_iv;
        for (int i=0; i<=3; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_key.insert(aes_key.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        for (int i=4; i<=7; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_iv.insert(aes_iv.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        std::string kCiperText = request->data();
        AES aes(AESKeyLength::AES_128);
        std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(kCiperText), aes_key, aes_iv);
        size_t newK = (size_t)UnsignedVectorToInt32(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
        // std::cout << "pruned k : " << newK << std::endl;
        exactKNN(newK);
        newK = std::min(newK, candidates.size());

        // std::cout << "after vector search k : " << newK << std::endl;

        avgK += newK;
        minK = std::min(minK, newK); // ablation study for optimization #2

        int blockS = (int)std::ceil(std::sqrt(queryK));
        if(topKOption == 1) { // refine with binary search
            std::vector<std::pair<float, float>> border;
            int ptr = 0;
            while(ptr + blockS < candidates.size()) {
                 border.emplace_back(std::make_pair(candidates[ptr].first, candidates[ptr + blockS - 1].first));
                 ptr += blockS;
            }
            int lastId = (int)candidates.size() - 1;
            if(ptr < candidates.size()) border.emplace_back(candidates[ptr].first, candidates[lastId].first);
            plainText.clear(); 
            std::vector<unsigned char> kPlain = Int32ToUnsignedVector((int)newK);
            plainText.insert(plainText.begin(), kPlain.begin(), kPlain.end());
            for(auto &item : border) {
                std::vector<unsigned char> boundleft = FloatToUnsignedVector(item.first);
                std::vector<unsigned char> boundright = FloatToUnsignedVector(item.second);
                plainText.insert(plainText.end(), boundleft.begin(), boundleft.end());
                plainText.insert(plainText.end(), boundright.begin(), boundright.end());
            }
        } else if(topKOption == 2) { // refine with priority queue (our algorithm)
            std::vector<float> border;
            int ptr = blockS - 1;
            while(ptr < candidates.size()) {
                border.emplace_back(candidates[ptr].first);
                ptr += blockS;
            }
            int num = (newK + blockS - 1) / blockS;

            // printf("[OUT OF SGX] silo_id : %d, bucketSize : %d, num : %d\n", siloPtr->GetSiloId(), blockS, num);

            if(border.size() < num) border.emplace_back(candidates.back().first);
            plainText.clear(); 
            std::vector<unsigned char> kPlain = Int32ToUnsignedVector((int)newK);
            plainText.insert(plainText.begin(), kPlain.begin(), kPlain.end());
            for(auto &item : border) {
                std::vector<unsigned char> bound = FloatToUnsignedVector(item);
                plainText.insert(plainText.end(), bound.begin(), bound.end());
            }
        }
        padding(plainText);
        std::vector<unsigned char> ciperText = aes.EncryptCBC(plainText, aes_key, aes_iv);
        response->set_data(std::string(ciperText.begin(), ciperText.end()));
        double grpc_comm = request->ByteSizeLong() + response->ByteSizeLong();
        m_logger.LogAddComm(grpc_comm);

        m_logger.SetEndTimer();
        m_logger.LogAddTime();
        return Status::OK;
    }

    //send (dis, silo Id)
    Status sendRange(ServerContext* context, const EncryptData* request, EncryptData* response) override {
        m_logger.SetStartTimer();
        const std::string rCiperText = request->data();
        AES aes(AESKeyLength::AES_128);
        //get aes key;
        std::vector<unsigned char> aes_key, aes_iv;
        for (int i=0; i<=3; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_key.insert(aes_key.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        for (int i=4; i<=7; ++i) {
            std::vector<unsigned char> aes_tmp = Uint64ToUnsignedVector(shared_secret_key_list[i]);
            aes_iv.insert(aes_iv.end(), aes_tmp.begin(), aes_tmp.begin() + 4);// 只取32位
        }
        std::vector<unsigned char> plainText = aes.DecryptCBC(StringToUnsignedVector(rCiperText), aes_key, aes_iv);
        float range = UnsignedVectorToFloat(std::vector<unsigned char>(plainText.begin(), plainText.begin() + 4));
        // std::cout << "pruned range : " << range << std::endl;
        if(range <= 0) {
            range = FLOAT_INF;
        } 
        plainText.clear();
        int cnt = 0;
        for(size_t i = 0;i < candidates.size();i++) {
            if(candidates[i].first > range) break;
            cnt++;
            float dis = candidates[i].first;
            std::vector<unsigned char> disPlain = FloatToUnsignedVector(dis);
            plainText.insert(plainText.end(), disPlain.begin(), disPlain.end());
        }
        std::vector<unsigned char> cntPlain = Int32ToUnsignedVector(cnt);
        plainText.insert(plainText.begin(), cntPlain.begin(), cntPlain.end());
        padding(plainText);
        std::vector<unsigned char> ciperText = aes.EncryptCBC(plainText, aes_key, aes_iv);
        response->set_data(std::string(ciperText.begin(), ciperText.end()));

        double grpc_comm = request->ByteSizeLong() + response->ByteSizeLong();
        m_logger.LogAddComm(grpc_comm);

        m_logger.SetEndTimer();
        m_logger.LogAddTime();
        return Status::OK;
    }

    // send number of answers
    Status sendNumber(ServerContext* context, const Number* request, Empty* response) override {
        m_logger.SetStartTimer();
        ansNum = request->num();
        int dim = (int)queryV->data.size();
        double grpc_comm = 0;
        grpc_comm += request->ByteSizeLong();
        m_logger.LogAddComm(grpc_comm);

        m_logger.SetEndTimer();
        m_logger.LogAddTime();

        return Status::OK;
    }

    Status getAns(ServerContext* context, const Empty* request, ServerWriter<brokersilo::Vector>* writer) override {
        m_logger.SetStartTimer();
        double grpc_comm = 0;
        int dim = queryV->Dimension();
        for(int i = 0;i < ansNum;i++) {
            brokersilo::Vector v;
            v.set_id(candidates[i].second->vid);
            for(int j = 0;j < dim;j++) {
                v.add_data(candidates[i].second->data[j]);
            }
            v.set_distance(candidates[i].first);
            v.set_attribute(candAttributes[i]);
            grpc_comm += v.ByteSizeLong();
            writer->Write(v);
        }
        grpc_comm += request->ByteSizeLong();
        candidates.clear();
        candAttributes.clear();
        ansNum = 0;
        m_logger.LogAddComm(grpc_comm);
        m_logger.SetEndTimer();
        m_logger.LogAddTime();

        return Status::OK;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << "k is " << 1.0 * avgK / 100 << "in average" << std::endl;
        ss << "the minimum k is " << 1.0 * minK << std::endl;
        return ss.str();
    }
};

std::unique_ptr<BrokerSiloImpl> siloptr = nullptr;

void SignalHandler(int signal) {
    if (siloptr != nullptr) {
        std::string log_info = siloptr->to_string();
        std::cout << log_info;
        std::cout.flush();
    }
    quick_exit(0);
}

//在程序结束或者崩溃时输出log
void ResetSignalHandler() {
    signal(SIGINT, SignalHandler);
    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGKILL, SignalHandler);
}

void RunSilo(int siloId , std::string ipAddr, std::string dataName, std::string scalarName, std::string clusterFile, std::string addClusterFile, std::string collectionName, std::string indexType) {
    siloptr = std::make_unique<BrokerSiloImpl>(siloId, ipAddr, dataName, scalarName, clusterFile, addClusterFile, collectionName, indexType);
    ServerBuilder builder;
    builder.AddListeningPort(ipAddr, grpc::InsecureServerCredentials());
    builder.RegisterService(siloptr.get());
    builder.SetMaxSendMessageSize(INT_MAX);
    builder.SetMaxReceiveMessageSize(INT_MAX);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Data Silo #(" << siloId << ") listening on " << ipAddr << std::endl;
    server->Wait();
    std::cout.flush();
}

int main(int argc, char** argv){
    std::string ipAddr, dataFile, scalarFile, clusterFile, collectionName, indexType;
    int siloId;
    
    try { 
        bpo::options_description option_description("Required options");
        option_description.add_options()
            ("help", "produce help message")
            ("id", bpo::value<int>(&siloId)->default_value(0), "Data silo's ID")
            ("ip", bpo::value<std::string>(), "Data silo's IP address + port")
            ("data-path", bpo::value<std::string>(), "Data file path")
            ("scalardata-path", bpo::value<std::string>(), "scalar data file path")
            ("cluster-path", bpo::value<std::string>(), "cluster file path")
            ("milvus-port", bpo::value<std::string>(), "running port of Milvus")
            ("collection-name", bpo::value<std::string>(), "milvus collection's name")
            ("index-type", bpo::value<std::string>(), "Index's type of local vector database")
            ("cluster-option", bpo::value<std::string>(), "whether build clusters")
            ("milvus-option", bpo::value<std::string>(), "whether import data")
            ("alpha", bpo::value<float>(&alpha)->default_value(0.05), "our CLI's hyper-parameter $\\alpha$")
            ("cluster-num", bpo::value<int>(&clusterNumber)->default_value(10), "our CLI's hyper-parameter: number of clusters")
        ;

        bpo::variables_map variable_map;
        bpo::store(bpo::parse_command_line(argc, argv, option_description), variable_map);
        bpo::notify(variable_map);    

        if (variable_map.count("help")) {
            std::cout << option_description << std::endl;
            return 0;
        }

        bool options_all_set = true;

        std::cout << "Data silo's ID is " << siloId << "\n";

        if (variable_map.count("ip")) {
            ipAddr = variable_map["ip"].as<std::string>();
            std::cout << "Data silo's IP address was set to " << ipAddr << "\n";
        } else {
            std::cout << "Data silo's IP address was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("data-path")) {
            dataFile = variable_map["data-path"].as<std::string>();
            std::cout << "Data silo's data file path was set to " << dataFile << "\n";
        } else {
            std::cout << "Data silo's data file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("scalardata-path")) {
            scalarFile = variable_map["scalardata-path"].as<std::string>();
            std::cout << "Data silo's meta data file path was set to " << scalarFile << "\n";
        } else {
            std::cout << "Data silo's meta data file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("cluster-path")) {
            clusterFile = variable_map["cluster-path"].as<std::string>();
            std::cout << "this data's cluster file path was set to " << clusterFile << "\n";
        } else {
            std::cout << "this data's cluster file path was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("collection-name")) {
            collectionName = variable_map["collection-name"].as<std::string>();
            std::cout << "the collection's name was set to " << collectionName << "\n";
        } else {
            std::cout << "the collection's name was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("index-type")) {
            indexType = variable_map["index-type"].as<std::string>();
            std::cout << "Data silo's index type was set as " << indexType << "\n";
        } else {
            std::cout << "Data silo's index type was not set" << "\n";
            options_all_set = false;
        }

        if (variable_map.count("milvus-port")) {
            MilvusPort = variable_map["milvus-port"].as<std::string>();
            std::cout << "Local Milvus's port was set as " << MilvusPort << "\n";
        } else {
            std::cout << "Local Milvus's port was not set " << "\n";
            options_all_set = false;
        }

        if (variable_map.count("cluster-option")) {
            std::string buildCluster = variable_map["cluster-option"].as<std::string>();
            std::cout << "Build Cluster Option was set as " << buildCluster << "\n";
            buildClusterOption = (buildCluster == "ON" ? true : false);
        } else {
            std::cout << "Build Cluster Option was not set " << "\n";
            options_all_set = false;
        }

        if (variable_map.count("milvus-option")) {
            std::string buildMilvus = variable_map["milvus-option"].as<std::string>();
            std::cout << "Build Milvus Option was set as " << buildMilvus << "\n";
            buildMilvusOption = (buildMilvus == "ON" ? true : false);
        } else {
            std::cout << "Build Milvus Option was not set " << "\n";
            options_all_set = false;
        }

        std::cout << "hyper parameter alpha was set as " << alpha << "\n"; 
        std::cout << "cluster's number was set as " << clusterNumber << "\n"; 

        if (false == options_all_set) {
            throw std::invalid_argument("Some options were not properly set");
            std::cout.flush();
            std::exit(EXIT_FAILURE);
        }
    } catch (std::exception& e) { 
        std::cerr << "Error: " << e.what() << "\n";  
        std::exit(EXIT_FAILURE);
    }
    ResetSignalHandler();
    std::string addClusterFile = clusterFile + "Inf";
    RunSilo(siloId, ipAddr, dataFile, scalarFile, clusterFile, addClusterFile, collectionName, indexType);
    return 0;
}