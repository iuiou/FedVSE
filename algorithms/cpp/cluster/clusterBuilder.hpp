#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>

#include "../utils/DataType.hpp"
#include "../utils/MetricType.hpp"
#include "../utils/BenchLogger.hpp"
#include "../utils/File_IO.h"
using Distance = EuclideanDistance;

std::random_device rd;
std::mt19937 gen(rd());

struct clusterInf{
    float bound;
    int num;

    clusterInf(float bound_, int num_) {
        bound = bound_;
        num = num_;
    }

    bool friend operator < (const clusterInf &a, const clusterInf &b) {
        return a.bound < b.bound;
    }
};

class cluster {
    private:
        std::unique_ptr<VectorDataType> center;
        std::vector<std::pair<float, int> > boundary; // distance and bouns number
        int Nc, cId;
        Distance dist_function;

    public:
        cluster(VectorDataType &c, std::vector<std::pair<float, int> > &b, int Nc_, int cId_) {
            center = std::make_unique<VectorDataType>(c.Dimension(), c.vid, c);
            boundary.resize(b.size());
            std::copy_n(b.begin(), b.size(), boundary.begin());
            Nc = Nc_;
            cId = cId_;
        }

        cluster(std::string &fileName) {
            std::ifstream file(fileName);
            if (!file.is_open()) {
                std::cerr << "Failed to open file for reading ground truth: " << fileName << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::string line;
            std::getline(file, line);
            std::istringstream iss(line);
            int dim, id;
            iss >> id >> dim;
            cId = id;
            center = std::make_unique<VectorDataType>(dim, 0);
            std::getline(file, line);
            std::istringstream isVector(line);
            for(int i = 0;i < dim;i++) {
                isVector >> center->data[i]; 
            }
            std::getline(file, line);
            std::istringstream isboundarySize(line);
            int boundarySize;
            isboundarySize >> Nc >> boundarySize;
            boundary.reserve(boundarySize);
            boundary.resize(boundarySize);
            std::getline(file, line);
            std::istringstream isboundary(line);
            for(int i = 0;i < boundarySize;i++) {
                float bound;
                int num;
                isboundary >> bound >> num;
                boundary.emplace_back(std::make_pair(bound, num));              
            }
        }

        float caldis(VectorDataType &data) {
            float ans = dist_function(*center, data);
            return ans;
        }

        int getClusterId() {
            return cId;
        }

        void genDisSet(VectorDataType &queryV, std::vector<clusterInf> &ans, float centerDis) {
            size_t boundarySize = boundary.size();
            for(size_t i = 0;i < boundarySize;i++) {
                ans.emplace_back(clusterInf(centerDis + boundary[i].first, boundary[i].second));
            }           
            return;
        }

        void dumpToFile(std::string file_name) {
            std::ofstream file(file_name);
            if (!file.is_open()) {
                std::cerr << "Failed to open file for dumping vector data: " << file_name << std::endl;
                std::exit(EXIT_FAILURE);
            }
            int dim = center->Dimension();
            // dump the cId and the cluster center
            file << cId << " " << dim << std::endl;
            for(int i = 0;i < dim;i++) file << center->data[i] << " ";
            file << std::endl;
            // dump the \sqrt NN distance
            file << Nc << " " << boundary.size() << std::endl;
            for(int i = 0;i < (int)boundary.size();i++) {
                file << boundary[i].first << " " << boundary[i].second;
                file << " ";
            }
        }
};

class AvgKmeans {
    private:
        std::vector<int> labelMp;
        std::vector<std::shared_ptr<VectorDataType> > centroids;
        std::vector<std::shared_ptr<cluster> > clusters;
        Distance dist_function;
        int clusterN;
    
    public:
        AvgKmeans(const std::vector<VectorDataType> &data, int K, int maxIterations) {
            #ifdef DEBUG
            #endif
            labelMp.clear();
            float lambda = getlambda(data);
            size_t N = data.size();
            labelMp.resize(N);
            clusterN = K;
            std::vector<VectorDataType> center = initializeCentroids(data, K);
            size_t dim = data[0].Dimension();
            for(int i = 0;i < clusterN;i++) {
                centroids.emplace_back(std::make_shared<VectorDataType>(dim, 0, center[i]));
            }
            std::vector<int> sizeMp(clusterN, 0);
            for (int iter = 0; iter < maxIterations; ++iter) {
                bool converged = true;
                // Step 1: Assign each data point to the nearest centroid
                for (int i = 0; i < N; ++i) {
                    double minDistance = std::numeric_limits<double>::max();
                    int closestCentroid = -1;
                    for (int j = 0; j < K; ++j) {
                        double distance = dist_function(data[i], *(centroids[j])) + 1.0 * lambda * sizeMp[j];
                        if (distance < minDistance) {
                            minDistance = distance;
                            closestCentroid = j;
                        }
                    }
                    if (labelMp[i] != closestCentroid) {
                        converged = false;
                    }
                    labelMp[i] = closestCentroid;
                    sizeMp[closestCentroid]++;
                }
                // Step 2: Update centroids based on the mean of assigned points
                centroids.clear();
                for (int i = 0; i < K; i++) centroids.emplace_back(std::make_shared<VectorDataType>(dim, 0));
                for (int i = 0; i < N; ++i) {
                    int cluster = labelMp[i];
                    for (int j = 0; j < dim; ++j) {
                        centroids[cluster]->data[j] += data[i].data[j];
                    }
                }
                for (int i = 0; i < K; ++i) {
                    if (sizeMp[i] > 0) {
                        for (int l = 0; l < dim; ++l) {
                            centroids[i]->data[l] /= sizeMp[i];
                        }
                    }
                }
                for (int i = 0; i < K; i++) sizeMp[i] = 0;
                // Check for convergence
                if (converged) {
                    std::cout << "Converged in iteration " << iter + 1 << std::endl;
                    break;
                }
                std::cout << "iter " << iter << " finished" << std::endl;
            }
            std::cout << "finish k means" << std::endl;
            genCluster(data);
        }

        // Function to initialize K centroids randomly from the data points
        std::vector<VectorDataType> initializeCentroids(const std::vector<VectorDataType>& data, int K) {
            size_t dim = data[0].Dimension();
            std::vector<VectorDataType> centers(K, VectorDataType(dim, 0));
            std::vector<int> ids(data.size());
            for(size_t i = 0;i < data.size();i++) ids[i] = i;
            std::shuffle(ids.begin(), ids.end(), gen);

            for (int i = 0; i < K; ++i) {
                int order = ids[i];
                centers[i] = data[order];
            }
            return centers;
        }

        float getlambda(const std::vector<VectorDataType> &data) {
            size_t size = data.size();
            std::vector<int> numbers(size, 0);
            for(size_t i = 0;i < size;i++) {
                numbers[i] = i;
            }
            std::shuffle(numbers.begin(), numbers.end(), gen);
            float sum = 0;
            for(int i = 0;i < 50;i++) {
                for(int j = 0;j < 50;j++) {
                    if(i == j) continue;
                    float dis = dist_function(data[numbers[i]], data[numbers[j]]);
                    sum += dis;
                }
            }
            sum /= 50*50-50;
            return 1.0 * sum / (int)data.size();
        } 

       void genCluster(const std::vector<VectorDataType> &data) {
            std::vector<std::vector<float> > disSet(clusterN);
            size_t N = data.size();
            for(size_t i = 0;i < N;i++) {
                int cid = labelMp[i];
                float dis = dist_function(*(centroids[cid]), data[i]);
                disSet[cid].emplace_back(dis);
            }
            for(int i = 0;i < clusterN;i++) {
                std::sort(disSet[i].begin(), disSet[i].end());
                std::vector<std::pair<float, int> > boundary;
                int clusterSize = (int)disSet[i].size();
                int blockS = (int)std::ceil(std::sqrt(clusterSize));
                int j = blockS - 1;
                for(;j < clusterSize;j += blockS) {
                    boundary.emplace_back(std::make_pair(disSet[i][j], blockS));
                }
                if(boundary.back().first != disSet[i].back()) {
                    boundary.emplace_back(std::make_pair(disSet[i].back(), clusterSize % blockS));
                }
                std::shared_ptr<cluster> nowC = std::make_shared<cluster>(*(centroids[i]), boundary, clusterSize, i); // cluster center, boundary, cluster's size, cluster id
                clusters.emplace_back(nowC);
            }
        }

        void dumpToFile(std::string file_name) {
            std::ofstream file(file_name);
            if (!file.is_open()) {
                std::cerr << "Failed to open file for dumping vector data: " << file_name << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::vector<std::vector<int>> clusterSet(clusterN);
            for(size_t i = 0;i < labelMp.size();i++) {
                clusterSet[labelMp[i]].emplace_back(i);
            }
            file << clusterN << std::endl;
            for(size_t i = 0;i < clusterN;i++) {
                file << clusterSet[i].size() << std::endl;
                for(int j : clusterSet[i]) {
                    file << j << " "; //id
                }
                file << std::endl;
            }
            return;
        }

        void dumpClusterToFile(std::string fileNamePrefix) {
            for(int i = 0;i < clusterN;i++) {
                clusters[i]->dumpToFile(fileNamePrefix + std::to_string(i));
            }
            return;
        }
};


// int main() {
//     // Example data: 2D points
//     vector<vector<double>> data = {
//         {1.0, 2.0}, {1.5, 1.8}, {5.0, 8.0}, {8.0, 8.0},
//         {1.0, 0.6}, {9.0, 11.0}, {8.0, 2.0}, {10.0, 2.0}, {9.0, 3.0}
//     };

//     int K = 3;               // Number of clusters
//     int maxIterations = 100; // Maximum iterations

//     kmeans(data, K, maxIterations);

//     return 0;
// }
