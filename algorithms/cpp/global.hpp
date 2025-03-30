#include <random>
#include <mutex>
#include <thread>
#include <cmath>
#include <limits>

const int localSearchOption = 3; // 1:do not prune  2:global build pgmindex and evaluate  3:build clusters and evaluate
const int topKOption = 5; // 1:exact submit newK numbers 2: without merge intervals  3 our algorithms 4 choose the max right bound 5 pop with priority queue
const bool buildMilvusOption = false;
const bool buildClusterOption = false;
const bool recordAnswerComm = true;

const float FLOAT_INF = 1e12;
const float alpha = 0.05;

double optTopKTime = 0;
double optRefineTime = 0;