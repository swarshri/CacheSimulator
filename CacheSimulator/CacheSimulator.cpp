/*
   Cache Simulator
   Level one L1 and level two L2 cache parameters are read from file
   (block size, line per set and set per cache).
   The 32 bit address is divided into:
   -tag bits (t)
   -set index bits (s)
   -block offset bits (b)

   s = log2(#sets)   b = log2(block size)  t=32-s-b
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>
#include <stdlib.h>
#include <cmath>
#include <bitset>

using namespace std;
//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss

struct config {
    int L1blocksize;
    int L1setsize;
    int L1size;
    int L2blocksize;
    int L2setsize;
    int L2size;
};

class Cache {
protected:
    int block_size = 0;
    int ways = 0;
    int cache_size = 0;
    int next_fill_pos = 0;

    int index_bits = 0;
    int tag_bits = 0;
    int offset_bits = 0;

    int index_start_bit = 0;
    int offset_start_bit = 0;

public:
    Cache(int block_size, int set_size, int cache_size) {

        this->block_size = block_size;
        this->ways = set_size;
        this->cache_size = cache_size * pow(2, 10);

        this->offset_bits = log2(this->block_size);

        this->offset_start_bit = 32 - this->offset_bits;
    }

    virtual bool read(bitset<32> address) {}

    virtual bool write(bitset<32> address) {}

    virtual void update(bitset<32> address) {}

    virtual bool check(bitset<32> address) {}

    virtual bitset<32> getCurrentData(bitset<32> address) {}
};

class DirectMappedCache : public Cache {
private:
    //dict(index, tuple(tag, validity, dirty)))
    map<int, tuple<int, bool, bool>> contents;

public:
    DirectMappedCache(int block_size, int set_size, int cache_size)
        : Cache(block_size, set_size, cache_size) {

        int space_bits = log2(this->cache_size);
        int way_bits = log2(this->ways);

        this->index_bits = space_bits - way_bits - offset_bits;
        this->tag_bits = 32 - this->index_bits - this->offset_bits;

        this->index_start_bit = 32 - this->index_bits - this->offset_bits;

        for (int i = 0; i < pow(2, this->index_bits); i++) {
            contents[i] = tuple<int, bool, bool>(0, false, false);
        }
    }

    // Return 1 (Read Hit) if there is a hit, otherwise return 0 (Read Miss)
    bool read(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();

        auto* current_data = &this->contents[current_index];
        if (get<int>(*current_data) == current_tag)
            return get<1>(*current_data); // return validity
        else
            return false;
    }

    // Return 1 (Write Hit) if there is a hit, otherwise return 0 (Write Miss) 
    bool write(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();

        auto* current_data = &this->contents[current_index];
        if (get<int>(*current_data) == current_tag) {
            *current_data = tuple<int, bool, bool>(current_tag, true, true);
            return true;
        }
        else
            return false;
    }

    // Update the cache content
    void update(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();

        this->contents[current_index] = tuple<int, bool, bool>(current_tag, true, false);
    }

    // Returns the dirty bit
    bool check(bitset<32> address) {
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();
        return get<2>(this->contents[current_index]); // return dirty bit
    }


    bitset<32> getCurrentData(bitset<32> address) {
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();
        bitset<32> index_bits = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits));
        bitset<32> tag_val = bitset<32>(get<int>(this->contents[current_index]));
        //bitset<32> tag_bits - to do
    }
};

class SetAssociativeCache : public Cache {
private:
    //dict(index, tuple(way_pointer, ways(tuple(tag, validity, dirty))))
    map<int, tuple<int, vector<tuple<int, bool, bool>>>> contents;
public:
    SetAssociativeCache(int block_size, int set_size, int cache_size)
        : Cache(block_size, set_size, cache_size) {

        int space_bits = log2(this->cache_size);
        int way_bits = log2(this->ways);

        this->index_bits = space_bits - way_bits - offset_bits;
        this->tag_bits = 32 - this->index_bits - this->offset_bits;

        this->index_start_bit = 32 - this->index_bits - this->offset_bits;

        for (int i = 0; i < pow(2, this->index_bits); i++) {
            get<1>(contents[i]).resize(this->ways); // resize the vector in contents to be equal to the number of ways
            contents[i] = tuple<int, vector<tuple<int, bool, bool>>>(0, { tuple<int, bool, bool>(0, false, false) });
        }
    }

    // Return 1 if there is a way with the same tag and valid bit (Read hit). Otherwise, Read Miss.
    bool read(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();

        auto* current_data = &get<1>(this->contents[current_index]);
        for (auto way_data = current_data->begin(); way_data < current_data->end(); way_data++) {
            if (get<int>(*way_data) == current_tag)
                return get<1>(*way_data);
        }
        return false;
    }

    // "QUESTION" Before writing, shouldn't we check the dirty bit to write back to L2/main memory?
    bool write(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();

        auto* current_data = &get<1>(this->contents[current_index]);
        for (auto way_data = current_data->begin(); way_data < current_data->end(); way_data++) {
            if (get<int>(*way_data) == current_tag) {
                *way_data = tuple<int, bool, bool>(current_tag, true, true);
                return true;
            }
        }
        return false;
    }

    void update(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();
        int next_fill_pos = get<0>(this->contents[current_index]);

        get<1>(this->contents[current_index])[get<0>(this->contents[current_index])] = tuple<int, bool, bool>(current_tag, true, false);
        if (get<0>(this->contents[current_index]) == this->ways - 1)
            get<0>(this->contents[current_index]) = 0;
        else
            get<0>(this->contents[current_index])++;
    }

    bool check(bitset<32> address) {
        int current_index = bitset<32>(address.to_string().substr(this->index_start_bit, this->index_bits)).to_ulong();
        return get<2>(get<1>(this->contents[current_index])[get<0>(this->contents[current_index])]); // return dirty bit
    }

    bitset<32> getCurrentData(bitset<32> address) {

    } 
};

class FullyAssociativeCache : public Cache {
private:
    // tuple(way_pointer, vector(tuple(tag, validity, dirty))))
    tuple<int, vector<tuple<int, bool, bool>>> contents;

public:
    FullyAssociativeCache(int block_size, int set_size, int cache_size)
        : Cache(block_size, set_size, cache_size) {

        this->tag_bits = 32 - this->index_bits - this->offset_bits;
        this->ways = pow(2, this->tag_bits);

        get<1>(this->contents).resize(this->cache_size);
        for (int i = 0; i < this->cache_size; i++) {
            get<1>(this->contents)[i] = tuple<int, bool, bool>(0, false, false);
        }
    }

    bool read(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        auto way_data = get<1>(this->contents);

        for (auto data = way_data.begin(); data < way_data.end(); data++) {
            if (get<int>(*data) == current_tag)
                return get<1>(*data);
        }
        return false;
    }

    bool write(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();
        auto way_data = get<1>(this->contents);

        for (auto data = way_data.begin(); data < way_data.end(); data++) {
            if (get<int>(*data) == current_tag) {
                *data = tuple<int, bool, bool>(current_tag, true, true);
                return true;
            }
        }
        return false;
    }

    void update(bitset<32> address) {
        int current_tag = bitset<32>(address.to_string().substr(32, this->tag_bits)).to_ulong();

        get<1>(this->contents)[get<0>(this->contents)] = tuple<int, bool, bool>(current_tag, true, false);
        if (get<0>(this->contents) == this->ways - 1)
            get<0>(this->contents) = 0;
        else
            get<0>(this->contents)++;
    }

    bool check(bitset<32> address) {
        return get<2>(get<1>(this->contents)[get<0>(this->contents)]); // return dirty bit
    }

    bitset<32> getCurrentData(bitset<32> address) {

    }
};

Cache* createCache(int block_size, int set_size, int cache_size) {
    Cache* cache;
    if (set_size == 0)
        cache = new FullyAssociativeCache(block_size, set_size, cache_size);
    else if (set_size == 1)
        cache = new DirectMappedCache(block_size, set_size, cache_size);
    else
        cache = new SetAssociativeCache(block_size, set_size, cache_size);
}

int main(int argc, char* argv[]) {
    config cacheconfig;
    ifstream cache_params;
    string dummyLine;

    cache_params.open(argv[1]);

    while (!cache_params.eof())  // read config file
    {
        cache_params >> dummyLine;
        cache_params >> cacheconfig.L1blocksize;
        cache_params >> cacheconfig.L1setsize;
        cache_params >> cacheconfig.L1size;
        cache_params >> dummyLine;
        cache_params >> cacheconfig.L2blocksize;
        cache_params >> cacheconfig.L2setsize;
        cache_params >> cacheconfig.L2size;
    }

    // initialize the hirearch cache system with those configs
    // probably you may define a Cache class for L1 and L2, or any data structure you like
    Cache* L1Cache = createCache(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size);
    Cache* L2Cache = createCache(cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);

    int L1AcceState = 0; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = 0; // L2 access state variable, can be one of NA, RH, RM, WH, WM;

    ifstream traces;
    ofstream tracesout;
    string outname;
    outname = string(argv[2]) + ".out";

    traces.open(argv[2]);
    tracesout.open(outname.c_str());

    string line;
    string accesstype;  // the Read/Write access type from the memory trace;
    string xaddr;       // the address from the memory trace store in hex;
    unsigned int addr;  // the address from the memory trace store in unsigned int;        
    bitset<32> accessaddr; // the address from the memory trace store in the bitset;

    if (traces.is_open() && tracesout.is_open()) {
        while (getline(traces, line)) {   // read mem access file and access Cache

            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr)) { break; }
            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32>(addr);

            // access the L1 and L2 Cache according to the trace;
            if (accesstype.compare("R") == 0)
            {
                // read access to the L1 Cache, 
                bool l1_read_hit = L1Cache->read(accessaddr);
                if (l1_read_hit)
                    L1AcceState = 1;
                else {
                    L1AcceState = 2;

                    //  and then L2 (if required),
                    bool l2_read_hit = L2Cache->read(accessaddr);
                    if (l2_read_hit)
                        L2AcceState = 1;
                    else {
                        L2AcceState = 2;
                        bool l2_dirty = L2Cache->check(accessaddr);
                        // skipping the part where the dirtiness is checked
                        // to decide if the data should be written back, since there is no dmem.
                        L2Cache->update(accessaddr);
                    }

                    bool l1_dirty = L1Cache->check(accessaddr);
                    if (l1_dirty) {
                        bitset<32> wb_addr = L1Cache->getCurrentData(accessaddr);
                        bool l2_dirty = L2Cache->check(wb_addr);
                        // skipping the part where the dirtiness is checked
                        // to decide if the data should be written back, since there is no dmem.
                        L2Cache->update(wb_addr); // Write back if dirty.
                    }
                    L1Cache->update(accessaddr);
                }
            }
            else if (accesstype.compare("W") == 0)
            {
                // write access to the L1 Cache,
                bool l1_write_hit = L1Cache->write(accessaddr);
                if (l1_write_hit)
                    L1AcceState = 3;
                else {
                    L1AcceState = 4;

                    //and then L2 (if required),
                    bool l2_write_hit = L2Cache->write(accessaddr);
                    if (l2_write_hit)
                        L2AcceState = 3;
                    else {
                        L2AcceState = 4;
                    }
                }
            }

            // Output hit/miss results for L1 and L2 to the output file
            tracesout << L1AcceState << " " << L2AcceState << endl;
        }
        traces.close();
        tracesout.close();
    }
    else cout << "Unable to open trace or traceout file ";

    return 0;
}