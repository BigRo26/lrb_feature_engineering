//
// Created by Zhenyu Song on 11/13/18.
//

#include "annotate.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <cstring>

//max to 4 billion (2**32-1)
const uint64_t max_next_seq = 0xffffffff;
using namespace std;


// Fast line count with large block reads, so the annotation vector can be sized exactly.
// Counts '\n' plus one for a final line without a trailing newline.
static uint64_t count_lines(const string &file) {
    ifstream in(file, ios::binary);
    if (!in) {
        cerr << "Exception opening/reading annotate original file " << file << endl;
        exit(-1);
    }
    vector<char> buf(1 << 24);
    uint64_t n = 0;
    char last = '\n';
    while (in) {
        in.read(buf.data(), buf.size());
        streamsize got = in.gcount();
        if (got <= 0)
            break;
        const char *p = buf.data(), *end = buf.data() + got;
        while ((p = static_cast<const char *>(memchr(p, '\n', end - p)))) {
            ++n;
            ++p;
        }
        last = buf[got - 1];
    }
    return last == '\n' ? n : n + 1;
}

void annotate(const string &trace_file, const int &n_extra_fields) {
    /*
     * assume trace is less than max(uint32_t)
     * */
    auto expect_file = trace_file+".ant";
    ifstream cachefile(expect_file);
    if (cachefile.good()) {
        cerr<<"file has been annotated, so skip annotation"<<endl;
        return;
    }

    // in the first path, hold ids; in the reverse path, hold next_seq
    vector<uint64_t> id_and_next_seq;
    // Reserve the exact size up front. Growing by emplace_back doubles the capacity, and the doubling
    // at 2^31 entries needs the old 16 GiB plus a new 32 GiB block at once (48 GiB), which fails with
    // bad_alloc on smaller nodes. Reserved, the full 2.8B-request wiki trace needs ~21 GiB.
    {
        uint64_t n_lines = count_lines(trace_file);
        cerr << "trace has " << n_lines << " lines, reserving "
             << double(n_lines * sizeof(uint64_t)) / (1ull << 30) << " GiB for annotation" << endl;
        id_and_next_seq.reserve(n_lines);
    }

    uint64_t id;
    int64_t i = 0;
    //not actually need t and size
    uint64_t t, size;
    vector<uint64_t> extra_features(n_extra_fields, 0);

    ifstream infile(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading annotate original file " << trace_file << endl;
        exit(-1);
    }

    while(infile>> t >> id >> size) {
        for (int j = 0; j < n_extra_fields; ++j)
            infile >> extra_features[j];
        if (!(++i%1000000))
            cerr<<"reading origin trace: "<<i<<endl;
        id_and_next_seq.emplace_back(id);
    }

    uint64_t n_req = id_and_next_seq.size();
    std::cerr << "scanned trace n=" << n_req << std::endl;
    if (n_req > max_next_seq) {
        cerr<<"Error: don't support more trace length than "<<max_next_seq<<endl;
        abort();
    }

    // get nextSeen indices
    // assume id has same size
    unordered_map<uint64_t, uint32_t> last_seen;
    for (i = n_req - 1; i >= 0; --i) {
        uint64_t current_id = id_and_next_seq[i];
        auto lit = last_seen.find(current_id);
        if (lit != last_seen.end())
            id_and_next_seq[i] = lit->second;
        else
            id_and_next_seq[i] = max_next_seq;
        last_seen[current_id] = i;
        if (!(i % 1000000))
            cerr<<"computing next t: "<<i<<endl;
    }

    // get current time
    string now;
    auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
    auto tmp_file = trace_file + ".ant.tmp." + to_string(timenow);
    cerr<<"writing the annotated trace "<<tmp_file<<endl;

    ofstream outfile(tmp_file);
    if (!outfile) {
        cerr << "Exception opening/reading tmp file " << tmp_file << endl;
        exit(-1);
    }

    infile.clear();
    infile.seekg(0, ios::beg);
    //no need to write seq, which is implicit
    for (i = 0; i < (uint32_t) n_req; ++i) {
        infile >> t >> id >> size;
        for (int j = 0; j < n_extra_fields; ++j)
            infile >> extra_features[j];
        outfile << id_and_next_seq[i] << " " << t << " " << id << " " << size;
        for (int j = 0; j < n_extra_fields; ++j)
            outfile << " " << extra_features[j];
        outfile <<endl;
        if (!(i % 1000000))
            cerr<<"writing: "<<i<<endl;
    }

    outfile.close();

    //fat has bug in renaming: cross link file system. Actually it's better to do this in python with c wrapper
    //as not using /tmp, this is not a problem anymore
//    system(("mv "+tmp_file+" "+expect_file).c_str());
    if (rename(tmp_file.c_str(), expect_file.c_str())) {
        cerr << "Exception in renaming file from " << tmp_file << " to " << expect_file << " code: " << strerror(errno)
             << endl;
        return;
    }

}
