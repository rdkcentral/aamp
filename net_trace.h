/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// net_trace.h
#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <fstream>
#include <chrono>
#include <cmath>
#include <cstdint>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>   // getpid()
#endif

namespace aamptrace {

inline double now_monotonic_s() {
	using C = std::chrono::steady_clock;
	auto t = C::now().time_since_epoch();
	return std::chrono::duration<double>(t).count();
}

// --------- Per-burst structure -----------
struct Burst {
	int    idx = 0;
	double t_start_s = 0.0;   // relative to request start
	double dur_s     = 0.0;
	int    bytes     = 0;
	double gap_before_s = 0.0;
	bool   is_late   = false;
};

// --------- Recorder for one request -----
class NetTrace {
public:
	// Minimum non-zero span to assign to any burst duration
	static constexpr double kMinBurstDurS = 0.001; // 1 ms
	
	explicit NetTrace(uint64_t req_id,
					  const std::string& url_path,
					  const std::string& media_type,
					  bool chunked_hdr_seen,
					  double gap_threshold_s,
					  double late_gap_extra_s_threshold)
	: req_id_(req_id),
	url_path_(url_path),
	media_type_(media_type),
	gap_threshold_s_(gap_threshold_s),
	late_extra_s_threshold_(late_gap_extra_s_threshold),
	t0_(now_monotonic_s()),
	chunked_hdr_seen_(chunked_hdr_seen) {}
	
	// Called in header callback if Transfer-Encoding: chunked is seen
	void mark_chunked() { chunked_hdr_seen_ = true; }
	
	// Called in write callback; returns "true" if a new burst just started
	bool on_write(size_t num_bytes, double t_now_s) {
		if (first_payload_time_s_ < 0) first_payload_time_s_ = t_now_s;
		bool new_burst = false;
		if (!in_burst_) {
			open_burst(t_now_s, /*gap_before*/ last_end_time_s_ > 0 ? std::max(0.0, t_now_s - last_end_time_s_) : 0.0);
			new_burst = true;
		} else {
			// detect split if there's an idle gap mid-write stream
			double idle = last_cb_time_s_ > 0 ? std::max(0.0, t_now_s - last_cb_time_s_) : 0.0;
			if (idle > gap_threshold_s_) { // close previous, open new
				// Close the previous burst at the *current* ingress time to avoid 0-span bursts.
				close_burst(t_now_s);
				open_burst(t_now_s, idle);
				new_burst = true;
			}
		}
		// account
		if (!bursts_.empty()) {
			bursts_.back().bytes += int(num_bytes);
		}
		last_cb_time_s_ = t_now_s;
		return new_burst;
	}
	
	// Called at end of write stream
	void on_complete_bytes() {
		if (in_burst_) {
			// If we only saw a single callback, last_cb_time_s_ can equal open time.
			// Use a minimal non-zero duration floor.
			double t_end = last_cb_time_s_;
			if (t_end <= 0.0) t_end = now_monotonic_s();
			close_burst(t_end);
		}
	}
	
	// Fill-in curl/cdn timing after curl_easy_perform
	void set_curl_timings(double name_s, double connect_s, double appconnect_s,
						  double pre_xfer_s, double start_xfer_s, double total_s,
						  long http_code, bool conn_reused,
						  const std::string& primary_ip, long local_port,
						  size_t bytes_total) {
		name_s_ = name_s; connect_s_ = connect_s; appconnect_s_ = appconnect_s;
		pre_xfer_s_ = pre_xfer_s; start_xfer_s_ = start_xfer_s; total_s_ = total_s;
		http_code_ = http_code; conn_reused_ = conn_reused ? 1 : 0;
		primary_ip_ = primary_ip; local_port_ = local_port;
		bytes_total_ = bytes_total;
		total_done_time_s_ = now_monotonic_s();
	}
	
	// Write two CSV row sets (requests + bursts)
	void flush_csv() {
		ensure_files_open();
		// aggregate
		double gap_time_s = 0, burst_time_s = 0; int late_count = 0; double bytes = 0;
		for (auto& b : bursts_) {
			gap_time_s   += b.gap_before_s;
			burst_time_s += b.dur_s;
			bytes        += b.bytes;
			late_count   += b.is_late ? 1 : 0;
		}
		double avg_burst_rate_Bps = (burst_time_s > 0) ? (bytes / burst_time_s) : 0.0;
		
		// request row
		req_ofs_ <<
		req_id_ << ',' << t0_ << ',' << url_path_ << ',' << media_type_ << ',' <<
		bytes_total_ << ',' << http_code_ << ',' << conn_reused_ << ',' <<
		primary_ip_ << ',' << local_port_ << ',' <<
		start_xfer_s_ << ',' << total_s_ << ',' <<
		name_s_ << ',' << connect_s_ << ',' << appconnect_s_ << ',' <<
		pre_xfer_s_ << ',' << redirect_s_ << ',' <<
		(chunked_hdr_seen_ ? 1 : 0) << ',' <<
		gap_time_s << ',' << burst_time_s << ',' <<
		bursts_.size() << ',' << late_count << ',' << avg_burst_rate_Bps <<
		'\n';
		
		// burst rows
		for (auto& b : bursts_) {
			burst_ofs_ << req_id_ << ',' << b.idx << ',' << b.t_start_s << ',' <<
			b.dur_s << ',' << b.bytes << ',' << b.gap_before_s << ',' <<
			(b.is_late ? "late" : "normal") << '\n';
		}
		req_ofs_.flush();
		burst_ofs_.flush();
	}
	
	// Configure a "late" gap classifier (optional)
	void classify_gaps(double cadence_s, double jitter_s) {
		double late_thr = cadence_s + 2.0*std::max(0.010, jitter_s);
		for (auto& b : bursts_) {
			if (b.gap_before_s > late_thr) b.is_late = true;
		}
	}
	
	// NEW: per-process CSVs to avoid cross-process interleaving/appends.
	// Produces: <path>.PID (e.g., /tmp/aamp_net_requests.csv.12345)
	static void set_paths_with_pid(const std::string& req_path, const std::string& burst_path) {
		pid_t pid = getpid();
		{
			std::lock_guard<std::mutex> g(file_m_);
			req_path_= req_path+ "." + std::to_string(pid);
			burst_path_ = burst_path + "." + std::to_string(pid);
		}
	}
	
private:
	void open_burst(double t_start_abs_s, double gap_before_s) {
		in_burst_ = true;
		Burst b;
		b.idx = int(bursts_.size());
		b.t_start_s = t_start_abs_s - t0_;
		b.gap_before_s = gap_before_s;
		bursts_.push_back(b);
	}
	void close_burst(double t_end_abs_s) {
		if (!in_burst_ || bursts_.empty()) return;
		auto &b = bursts_.back();
		// Floor durations to avoid zero-time bursts from single write callbacks.
		double raw = t_end_abs_s - (t0_ + b.t_start_s);
		if (raw < kMinBurstDurS) raw = kMinBurstDurS;
		b.dur_s = raw;
		last_end_time_s_ = t_end_abs_s;
		in_burst_ = false;
	}
	static void ensure_files_open() {
		std::lock_guard<std::mutex> g(file_m_);
		if (!req_ofs_.is_open()) {
			req_ofs_.open(req_path_, std::ios::app);
			if (req_ofs_.tellp() == 0) {
				req_ofs_ << "req_id,when_start_s,url_path,media_type,bytes_total,http_code,conn_reused,primary_ip,local_port,ttfb_s,total_s,namelookup_s,connect_s,appconnect_s,pretransfer_s,redirect_s,chunked,gap_time_s,burst_time_s,burst_count,late_gap_count,avg_burst_rate_Bps\n";
			}
		}
		if (!burst_ofs_.is_open()) {
			burst_ofs_.open(burst_path_, std::ios::app);
			if (burst_ofs_.tellp() == 0) {
				burst_ofs_ << "req_id,burst_idx,t_start_s,duration_s,bytes,gap_before_s,class\n";
			}
		}
	}
	
	// request identity
	uint64_t req_id_;
	std::string url_path_, media_type_;
	double t0_;
	bool chunked_hdr_seen_ = false;
	
	// write/burst state
	bool   in_burst_ = false;
	double last_cb_time_s_ = 0.0;
	double last_end_time_s_ = 0.0;
	double first_payload_time_s_ = -1.0;
	double gap_threshold_s_;
	double late_extra_s_threshold_;
	std::vector<Burst> bursts_;
	
	// curl results
	long   http_code_ = -1;
	int    conn_reused_ = 0;
	std::string primary_ip_;
	long   local_port_ = 0;
	size_t bytes_total_ = 0;
	double name_s_=0, connect_s_=0, appconnect_s_=0, pre_xfer_s_=0, start_xfer_s_=0, total_s_=0, redirect_s_=0;
	double total_done_time_s_ = 0.0;
	
	// global CSV files
	static std::mutex file_m_;
	static std::string req_path_, burst_path_;
	static std::ofstream req_ofs_, burst_ofs_;
};

// static members
inline std::mutex NetTrace::file_m_;
inline std::string NetTrace::req_path_  = "/tmp/aamp_net_requests.csv";
inline std::string NetTrace::burst_path_= "/tmp/aamp_net_bursts.csv";
inline std::ofstream NetTrace::req_ofs_;
inline std::ofstream NetTrace::burst_ofs_;

} // namespace aamptrace
