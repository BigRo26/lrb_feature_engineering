//
// Created by Zhenyu Song on 10/30/18.
//

#ifndef WEBCACHESIM_SIMULATION_H
#define WEBCACHESIM_SIMULATION_H

#include <string>
#include <chrono>
#include <fstream>
#include <vector>
#include <bsoncxx/builder/basic/document.hpp>
#include <unordered_set>
#include <unordered_map>
#include "cache.h"
#include "bsoncxx/document/view.hpp"
#include "bloom_filter.h"


/*
 * single thread simulation. Returns results.
 */
bsoncxx::builder::basic::document simulation(std::string trace_file, std::string cache_type,
                                             uint64_t cache_size, std::map<std::string, std::string> params);

using namespace webcachesim;

class FrameWork {
public:
    bool uni_size = false;
    uint64_t segment_window = 1000000;
    //unit: second
    uint64_t real_time_segment_window = 600;
    uint n_extra_fields = 0;
    bool is_metadata_in_cache_size = false;
    unique_ptr<Cache> webcache = nullptr;
    std::ifstream infile;
    int64_t n_early_stop = -1;  //-1: no stop
    int64_t seq_start = 0;

    std::string _trace_file;
    std::string _cache_type;
    uint64_t _cache_size;
    const unordered_set<string> offline_algorithms = {"Belady", "BeladySample", "RelaxedBelady", "BinaryRelaxedBelady",
                                                      "PercentRelaxedBelady"};
    bool is_offline;

    /*
     * bloom filter
     */
    bool bloom_filter = false;
    AkamaiBloomFilter *filter;

    //=================================================================
    //simulation parameter
    int64_t t, id, size, usize, next_seq;
    int same_chunk;
    //measure every segment
    int64_t byte_req = 0, byte_miss = 0, obj_req = 0, obj_miss = 0;
    //rt: real_time
    int64_t rt_byte_req = 0, rt_byte_miss = 0, rt_obj_req = 0, rt_obj_miss = 0;
    //global statistics
    std::vector<int64_t> seg_byte_req, seg_byte_miss, seg_object_req, seg_object_miss;
    std::vector<int64_t> seg_rss;
    std::vector<int64_t> seg_byte_in_cache;
    //rt: real_time
    std::vector<int64_t> rt_seg_byte_req, rt_seg_byte_miss, rt_seg_object_req, rt_seg_object_miss;
    std::vector<int64_t> rt_seg_rss;
    uint64_t time_window_end;
    std::vector<uint16_t> extra_features;
    //use relative seq starting from 0
    uint64_t seq = 0;
    std::chrono::system_clock::time_point t_now;
    //=================================================================
    //good decision ratio (LRB, NSDI '20): an eviction is "good" if the evicted object's next request
    //is at least belady_boundary requests in the future (or it is never requested again).
    //enable with param track_good_decision=1. Requires annotating the trace with next_seq.
    bool track_good_decision = false;
    //boundary in number of requests. <=0 means derive it:
    //  if byte_million_req is given: cache_size * 1e6 / byte_million_req (same as LRB's EVICTION_LOGGING)
    //  otherwise: cache_size / (mean object size of requests seen so far)
    double belady_boundary_param = -1;
    double byte_million_req = -1;
    //object id -> absolute trace index of its next request (from the annotated trace)
    std::unordered_map<uint64_t, uint64_t> next_seq_of;
    //absolute index (line number) of the request currently being processed
    uint64_t abs_seq = 0;
    //running totals (never reset) for the derived boundary
    int64_t total_byte_req = 0, total_obj_req = 0;
    //measure every segment
    int64_t n_good_eviction = 0, n_eviction = 0;
    std::vector<int64_t> seg_good_eviction, seg_eviction;

    double current_belady_boundary() const;

    void on_eviction(const uint64_t &key);

    //decompose miss
//    int64_t byte_miss_cache = 0;
//    int64_t byte_miss_filter = 0;


    FrameWork(const std::string &trace_file, const std::string &cache_type, const uint64_t &cache_size,
              std::map<std::string, std::string> &params);

    bsoncxx::builder::basic::document simulate();

    bsoncxx::builder::basic::document simulation_results();

    void adjust_real_time_offset();

    void update_real_time_stats();

    void update_stats();
};



#endif //WEBCACHESIM_SIMULATION_H
