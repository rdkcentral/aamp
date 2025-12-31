
// main.cpp
// throughput_estimator

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <vector>
#include <time.h>
#include <curl/curl.h>
#include <assert.h>

const double epsilon = 1e-6;

// -------- Utilities --------

/**
 * @brief get clock time as a floating point monotonic value
 */
static inline double GetCurrentTimeMonotonicSeconds()
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

/**
 * @brief safe_div avoids division by zero
 */
static inline double safe_div(double num, double den, double fallback = 0.0) {
	return (den > epsilon) ? (num / den) : fallback;
}

/**
  * @brief given a vector of floating point values, retrieve the median value
 */
static double median(std::vector<double> v) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	return (n % 2) ? v[n/2] : 0.5 * (v[n/2 - 1] + v[n/2]);
}

// -------- TransferSample --------

/**
 * @brief encapsulates performance information for a given http download
 */
struct TransferSample
{
	double size_download_bytes = 0.0;   // CURLINFO_SIZE_DOWNLOAD (double bytes)
	double total_time_s = 0.0;          // CURLINFO_TOTAL_TIME (double seconds)
	double time_to_first_byte_s = 0.0;  // CURLINFO_STARTTRANSFER_TIME (double seconds)

	double payload_download_time_s = 0.0; // derived: total_time_s-time_to_first_byte_s
	double payload_bytes_per_second = 0.0;  // derived
	
	/**
	 * @brief utility method to populate sample metrics from Curl Handle
	 */
	bool MapFromCurlHandle(CURL *curl)
	{ // using "double seconds" variants for consistency and to avoid unit mismatch.
		return
			CURLE_OK == curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size_download_bytes) &&
			CURLE_OK == curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time_s) &&
			CURLE_OK == curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &time_to_first_byte_s);
	}
};

class TransferStatistics {
private:
	// Rolling history & stats
	std::vector<TransferSample> history;
	
	// Robust per-request overhead Time to First Byte (TTFB) estimate
	double overhead_est_s = 0.0; // median TTFB - computed brute force
	
	// Robust throughput estimates (bytes/s)
	double ewma_fast_Bps = 0.0; // reacts quickly
	double ewma_slow_Bps = 0.0; // stable
	double harmonic_Bps = 0.0;  // conservative
	
	// Exponentially Weighted Moving Average (EWMA) tuning
	static constexpr double ALPHA_FAST = 0.5;
	static constexpr double ALPHA_SLOW = 0.2;
	// Harmonic mean over last N samples
	size_t harmonic_window = 8;

	/**
	 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
	 */
	void RecomputeHarmonicMeanAndMedianTTFB()
	{ // Overhead = median TTFB from all samples
		std::vector<double> ttfbs;
		ttfbs.reserve(history.size());
		for (const auto& s : history)
		{
			ttfbs.push_back(s.time_to_first_byte_s);
		}
		overhead_est_s = median(ttfbs);

		// Harmonic mean of throughput over last harmonic_window samples
		const size_t n = history.size();
		const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
		double denom = 0.0;
		size_t count = 0;
		for (size_t i = start; i < n; ++i)
		{
			double g = history[i].payload_bytes_per_second;
			if (g > epsilon) { denom += (1.0 / g); ++count; }
		}
		harmonic_Bps = (count > 0 && denom > 0.0) ? (static_cast<double>(count) / denom) : 0.0;
	}

	// Combined conservative throughput (bytes/s)
	double robust_throughput_no_buffer_Bps() const {
		double ewma_min = (ewma_fast_Bps > 0.0 && ewma_slow_Bps > 0.0)
						  ? std::min(ewma_fast_Bps, ewma_slow_Bps)
						  : std::max(ewma_fast_Bps, ewma_slow_Bps);
		if (ewma_min <= 0.0) return harmonic_Bps;
		if (harmonic_Bps <= 0.0) return ewma_min;
		const double w = 0.6; // 60% harmonic, 40% EWMA
		return w * harmonic_Bps + (1.0 - w) * ewma_min;
	}

public:
	TransferStatistics() = default;

	/**
	 * @brief record a completed download sample, update rolling estimators
	 */
	void SegmentCompleted(TransferSample sample) {
		sample.payload_download_time_s = std::max(epsilon, sample.total_time_s - sample.time_to_first_byte_s);
		sample.payload_bytes_per_second = safe_div(static_cast<double>(sample.size_download_bytes), sample.payload_download_time_s, 0.0);

		history.push_back(sample);

		// Trim history by size to avoid unbounded growth
		const size_t MAX_HISTORY = 240; // reduce?
		if (history.size() > MAX_HISTORY)
		{
			history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY));
		}

		// EWMA updates
		if (ewma_fast_Bps <= 0.0) ewma_fast_Bps = sample.payload_bytes_per_second;
		else ewma_fast_Bps = ALPHA_FAST * sample.payload_bytes_per_second + (1.0 - ALPHA_FAST) * ewma_fast_Bps;

		if (ewma_slow_Bps <= 0.0) ewma_slow_Bps = sample.payload_bytes_per_second;
		else ewma_slow_Bps = ALPHA_SLOW * sample.payload_bytes_per_second + (1.0 - ALPHA_SLOW) * ewma_slow_Bps;

		// Median TTFB + harmonic mean
		RecomputeHarmonicMeanAndMedianTTFB();
	}

	/**
	 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
	 */
	double ThroughputEstimateBps() const {
		return robust_throughput_no_buffer_Bps();
	}

	/**
	 * @brief return current overhead (TTFB) estimate (seconds)
	 */
	double OverheadEstimateS() const {
		return overhead_est_s;
	}

	/**
	 * @brief predict completion time for a new segment
	 */
	double PredictCompletionTimeInSeconds(size_t segment_size_bytes) const {
		const double thr = ThroughputEstimateBps();
		if( thr>=1.0 )
		{
			return overhead_est_s + (static_cast<double>(segment_size_bytes) / thr);
		}
		else
		{ // we have no history data to make estimate
			return 0.0;
		}
	}
};

struct AdaptiveParameters {
	double alpha_short = 0.4; // for progress EWMA (bytes/sec) - higher alpha reacts faster to changes (less smoothing)
	double safety_factor = 1.5; // bail margin multiplier (higher => safer)
};

/**
 * @brief adjust parameters for estimates at runtime based on buffer health - not yet used
 */
AdaptiveParameters AdjustParametersAtRuntime(const std::vector<double>& recent_Bps, double buffer_s) {
	AdaptiveParameters t{};
	if (recent_Bps.size() >= 3)
	{
		const double mean = std::accumulate(recent_Bps.begin(), recent_Bps.end(), 0.0) / recent_Bps.size();
		double var = 0.0;
		for (double x : recent_Bps)
		{
			var += (x - mean) * (x - mean);
		}
		var /= recent_Bps.size();
		const double stddev = std::sqrt(var);
		const double cv = (mean > 0.0) ? (stddev / mean) : 1.0; // coefficient of variation

		// More volatility => lower alpha (more smoothing)
		double base = (buffer_s < 2.0) ? 0.35 : (buffer_s < 4.0 ? 0.45 : 0.55);
		t.alpha_short = std::clamp(base - 0.5 * cv, 0.15, 0.75);

		// Safety factor grows with risk
		t.safety_factor = (buffer_s < 2.0) ? 1.3 : (buffer_s < 4.0 ? 1.5 : 1.7);
	}
	return t;
}

struct DownloadContext
{
	FILE *f = NULL; // for logging
	
	double segment_total_bytes = 0.0;      // expected size (bytes)
	double bytes_downloaded_so_far = 0.0;  // progress (bytes)
	double buffered_seconds = 0.0;         // playback buffer (seconds) remaining
	double overhead_estimate_s = 0.0;      // optional: from TransferStatistics

	// short window EWMA state, updated from curl progress callback
	double EWMA_BytesPerSecond = 0.0;
	double EMWA_short_window_weight = 0.4;
	curl_off_t dlnow_prev = 0;
	double time_prev = 0.0;

	// bail control
	bool allow_bail = true;
	bool bailed = false;
};

// Short-window EWMA in BYTES/SEC (monotonic wall time)
void UpdateShortWindowEWMA(DownloadContext &context,
						   curl_off_t dltotal, curl_off_t dlnow,
						   curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	// Update segment_total_bytes if server provides content length
	if (dltotal > 0 && context.segment_total_bytes <= 0.0) {
		context.segment_total_bytes = static_cast<double>(dltotal);
	}

	const double now = GetCurrentTimeMonotonicSeconds();
	// TODO: allow elapsed time to be virtual so we can model behavior faster than real time
	
	context.bytes_downloaded_so_far = static_cast<double>(dlnow);
	
	if (context.time_prev > 0.0 && now > context.time_prev) {
		const curl_off_t delta_bytes = dlnow - context.dlnow_prev;
		const double delta_time = now - context.time_prev;

		if (delta_time > epsilon && delta_bytes > 0) {
			const double throughput_Bps = static_cast<double>(delta_bytes) / delta_time; // bytes/sec

			if (context.EWMA_BytesPerSecond <= 0.0) context.EWMA_BytesPerSecond = throughput_Bps;
			else context.EWMA_BytesPerSecond = context.EMWA_short_window_weight * throughput_Bps + (1.0 - context.EMWA_short_window_weight) * context.EWMA_BytesPerSecond;
		}
	}
	if( dlnow>context.dlnow_prev )
	{
		fprintf( context.f, "%f,%ld,%ld,%ld,%f\n",
				now,
				(dltotal>0)?(100*dlnow/dltotal):0,
				dlnow,
				dltotal,
				context.EWMA_BytesPerSecond
				);
	}
	context.dlnow_prev = dlnow;
	context.time_prev = now;
}

/**
 * @brief for use in XFERINFOFUNCTION. Call only if we are not already downloading the lowest bitrate segment.
 * @retval true if download unlikely to complete before underflow (bad)
 * @retval false if download likely to complete before underflow (good)
 */
bool MidDownloadUnderflowIsLikely(DownloadContext &context) {
	// Guard: if EWMA not yet established, be conservative (do not bail immediately)
	const double EWMA_BytesPerSecond = (context.EWMA_BytesPerSecond > 1.0) ? context.EWMA_BytesPerSecond : 1.0;

	const double remaining_bytes = std::max(0.0, context.segment_total_bytes - context.bytes_downloaded_so_far);

	// Include estimated per-request overhead (TTFB) if available
	const double payload_download_time_s = remaining_bytes / EWMA_BytesPerSecond;
	const double time_remaining_s = context.overhead_estimate_s + payload_download_time_s;

	// Hysteresis: leave some margin to avoid flapping
	const double margin = 0.15; // 15% margin
	const double threshold_s = context.buffered_seconds * (1.0 - margin);

	return (time_remaining_s > threshold_s);
}

/** @brief libcurl progress callback (XFERINFOFUNCTION)
 * This is called at frequent intervals allowing application to monitor progress and abort stalled transfers
 * Note: this may be called even after all data has been downloaded, while final logic is executed
 */
static int xferinfo(void *clientp,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t ultotal, curl_off_t ulnow)
{
	auto *context = reinterpret_cast<DownloadContext*>(clientp);
	UpdateShortWindowEWMA(*context, dltotal, dlnow, ultotal, ulnow);

	if( context->buffered_seconds>0 )
	{ // first download - nothing buffered, so don't bail
		if (context->allow_bail ) {
			const bool likely_underflow = MidDownloadUnderflowIsLikely(*context);
			if (likely_underflow) {
				std::printf("decision: bail mid-download (buffer=%.2fs, ewma=%.2f B/s)\n",
							context->buffered_seconds, context->EWMA_BytesPerSecond);
				context->bailed = true;
				return 1; // non-zero aborts transfer
			}
		}
	}
	return 0; // continue
}

// -------- Demo main --------

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{ // stub to avoid spewing download contents to console log
	return size*nmemb;
}

int main(int argc, const char* argv[])
{
	const char *path = std::getenv("outpath");
	if( path == nullptr )
	{
		printf( "please set environment variable 'outpath' to a valid directory\n" );
		exit(1);
	}
	std::string ewmaPath = std::string(path) + "/ewma.csv";
	FILE *f_ewma = fopen(ewmaPath.c_str(),"wb");
	assert( f_ewma );
	fprintf( f_ewma, "Time,Pct,dlnow,dltotal,Bps\n" );
	
	std::string abrPath = std::string(path) + "/abr.csv";
	FILE *f_abr = fopen(abrPath.c_str(),"wb");
	assert( f_abr );
	if( f_abr )
	{
		const double segment_duration_s = 2.0; // playback duration of media segment
		const double representation_bitrate_bps = 5000000; // representation/encoder target bitsPerSecond
		const double estimated_bytes = (representation_bitrate_bps * segment_duration_s) / 8.0;

		CURL *curl = curl_easy_init();
		assert( curl );
		TransferStatistics transferStatistics;
		
		const double buffered_seconds = 0;
		
		fprintf( f_abr, "TTFB(s),Throughput(Bps),Predicted Download Time(s),Actual Download Time(s)\n" );
		
		for( int i=0; i<30; i++ )
		{
			// here we simply download same media segment repeatedly on good network to collect baseline performance data.
			const char * url = "https://aamp-test-content.s3.us-east-1.amazonaws.com/VideoTestStream/dash/1080p_001.m4s";
			
			fprintf( f_ewma, "%f,%f\n", GetCurrentTimeMonotonicSeconds(), 0.0 );

			fprintf( f_abr, "%f,%f,%f",
					transferStatistics.OverheadEstimateS(),
					transferStatistics.ThroughputEstimateBps(),
					transferStatistics.PredictCompletionTimeInSeconds(estimated_bytes) );
			
			DownloadContext ctx{};
			ctx.f = f_ewma;
			ctx.segment_total_bytes = estimated_bytes;
			ctx.buffered_seconds = buffered_seconds;
			ctx.overhead_estimate_s = 0.0;
			ctx.EMWA_short_window_weight = 0.4;
			ctx.allow_bail = true;
			
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // allow compressed transfers
			//curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L * 64); // larger buffer may produce smoother progress
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "throughput-estimator/1.0");
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			const CURLcode res = curl_easy_perform(curl);
			if (ctx.bailed)
			{
				std::printf("download aborted due to predicted underflow\n");
			}
			// Create a sample from the completed (or partial) transfer
			TransferSample sample{};
			if (res == CURLE_OK || ctx.bailed)
			{
				if (sample.MapFromCurlHandle(curl))
				{ // note: if we bailed early, SIZE_DOWNLOAD will reflect partial bytes; that's fine for statistics.
					transferStatistics.SegmentCompleted(sample);
				}
				else
				{
					std::fprintf(stderr, "warning: failed to map metrics from curl handle\n");
				}
			}
			else
			{
				std::fprintf(stderr, "curl_easy_perform error: %s\n", curl_easy_strerror(res));
			}
			fprintf( f_abr, ",%f\n", sample.total_time_s );
			fprintf( f_ewma, "\n" );
		}
		fclose( f_abr );
		fclose( f_ewma );
		curl_easy_cleanup(curl);
	}
	return EXIT_SUCCESS;
}
