#include <vector>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <algorithm>
#include "pgm_index_variants.hpp"

class pgmIndex {
    private:
        int dim; // dimension for multidimensional pgm-index
        int N; // data num;

    public:
        pgmIndex(int dim_, int N_) {
            this->dim = dim_;
            this->N = N_;
        }

        int getDim() {
            return dim;
        }

        int getN() {
            return N;
        }

        virtual int rangeCount(std::vector<std::pair<int, int> > condition) = 0;
};

class oneDimPGM : public pgmIndex {
    private:
        std::shared_ptr<pgm::PGMIndex<int, 64>> index;
        std::vector<int> data;

    public:
        oneDimPGM(const std::vector<int> &data) : pgmIndex(1, (int)data.size()) {
            this->data = data;
            this->index = std::make_shared<pgm::PGMIndex<int, 64>>(this->data.begin(), this->data.end());
        }

        auto lower_bound(const int x) const {
            auto range = index->search(x);
            return std::lower_bound(data.begin() + range.lo, data.begin() + range.hi, x);
        }

        auto upper_bound(const int x) const {
            auto range = index->search(x);
            auto it = std::upper_bound(data.begin() + range.lo, data.begin() + range.hi, x);
            auto step = 1ull;
            while (it + step < data.cend() && *(it + step) == x)  // exponential search to skip duplicates
                step *= 2;
            return std::upper_bound(it + (step / 2), std::min(it + step, data.cend()), x);
        }

        int rangeCount(std::vector<std::pair<int, int> > condition) override {
            if(condition.size() > 1) {
                std::cout << "parameter num is larger than defined " << 1 << std::endl;
                exit(0);
            }
            int l = condition[0].first;
            int r = condition[0].second;
            return upper_bound(r) - lower_bound(l);
        }
};

class twoDimPGM : public pgmIndex {
    private:
        std::shared_ptr<pgm::MultidimensionalPGMIndex<2, uint32_t, 64> > index;

    public:
        twoDimPGM(std::vector<std::tuple<uint32_t, uint32_t>> data) : pgmIndex(2, (int)data.size()) {
            this->index = std::make_shared<pgm::MultidimensionalPGMIndex<2, uint32_t, 64> >(data.begin(), data.end());           
        }

        int rangeCount(std::vector<std::pair<int, int> > condition) override {
            if(condition.size() > 2) {
                std::cout << "parameter num is larger than defined" << 2 << std::endl;
                exit(0);
            }
            std::tuple<int, int> l = {condition[0].first, condition[1].first};
            std::tuple<int, int> r = {condition[0].second, condition[1].second};
            int ans = (int)std::distance(index->range(l, r), index->end());
            return ans;
        }
};

