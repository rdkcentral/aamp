
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

static constexpr double epsilon = 1e-6;

/**
 * @brief get clock time as a floating point monotonic value
 */
static double GetCurrentTimeMonotonicSeconds( void )
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

/**
  * @brief given a vector of floating point values, retrieve the median value
 */
static double median(std::vector<double> v)
{
	if( v.empty() )
	{
		return 0.0;
	}
	else
	{
		std::sort(v.begin(), v.end());
		const size_t n = v.size();
		return (n % 2) ? v[n/2] : 0.5 * (v[n/2 - 1] + v[n/2]);
	}
}

/**
 * @brief abstract network bandwith
 */
class NetworkBandwidthEstimator
{
private:
	/**
	 * @brief encapsulate performance information for a given http download
	 */
	class Sample
	{
	private:
		double size_download_bytes = 0.0;
		double total_time_seconds = 0.0;
		double time_to_first_byte_seconds = 0.0;
		
		// total_time - time_to_first_byte
		double payload_download_time_seconds = 0.0;
		
		// size_download / payload_download_time
		double payload_bytes_per_second = 0.0;
		
	public:
		/**
		 * @brief constructor - populate sample metrics
		 * @param curl Curl Handle
		 */
		Sample( CURL *curl )
		{ // using "double seconds" variants for consistency and to avoid unit mismatch.
			CURLcode rc;
			
			rc = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size_download_bytes);
			assert( rc == CURLE_OK );
			
			rc = curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time_seconds);
			assert( rc == CURLE_OK );
			
			rc = curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &time_to_first_byte_seconds);
			assert( rc == CURLE_OK );
			
			payload_download_time_seconds = std::max(epsilon, total_time_seconds - time_to_first_byte_seconds);
			
			payload_bytes_per_second = static_cast<double>(size_download_bytes) / payload_download_time_seconds;
		}
		
		double getTimeToFirstByteSeconds( void ) const
		{
			return time_to_first_byte_seconds;
		}

		double getPayloadBytesPerSecond( void ) const
		{
			return payload_bytes_per_second;
		}

		double getTotalTimeSeconds( void ) const
		{
			return total_time_seconds;
		}
	};
	
	// Rolling history & stats
	std::vector<Sample> history;
	
	// Robust per-request overhead Time to First Byte (TTFB) estimate
	double estimated_TFTB_seconds = 0.0; // median TTFB - computed brute force
	
	// Robust throughput estimates (bytes/s)
	double EWMA_fast_BytesPerSecond = 0.0; // reacts quickly
	double EWMA_slow_BytesPerSecond = 0.0; // stable
	double harmonic_BytesPerSecond = 0.0;  // conservative
	
	// Exponentially Weighted Moving Average (EWMA) tuning
	static constexpr double ALPHA_FAST = 0.5;
	static constexpr double ALPHA_SLOW = 0.2;
	// Harmonic mean over last N samples
	static constexpr size_t harmonic_window = 8;
	
	/**
	 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
	 */
	void RecomputeHarmonicMeanAndMedianTTFB()
	{ // Overhead = median TTFB from all samples
		std::vector<double> ttfbs;
		ttfbs.reserve(history.size());
		for( const auto& s : history )
		{
			ttfbs.push_back(s.getTimeToFirstByteSeconds() );
		}
		estimated_TFTB_seconds = median(ttfbs);

		// Harmonic mean of throughput over last harmonic_window samples
		const size_t n = history.size();
		const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
		double denominator = 0.0;
		size_t count = 0;
		for( size_t i = start; i < n; i++ )
		{
			double g = history[i].getPayloadBytesPerSecond();
			if( g > epsilon )
			{
				denominator += 1.0/g;
				count++;
			}
		}
		harmonic_BytesPerSecond = (count > 0 && denominator > 0.0) ? (static_cast<double>(count) / denominator) : 0.0;
	}

public:
	NetworkBandwidthEstimator() = default;
	
	/**
	 * @brief update state for network bandwidth model given a recent complete or aborted curl download
	 *
	 * @return total download time (for optional logging)
	 */
	double UpdateDownloadMetrics( CURL *curl )
	{
		Sample sample(curl);
		const double payload_bytes_per_second = sample.getPayloadBytesPerSecond();
		const double total_time_seconds = sample.getTotalTimeSeconds();
		history.push_back(std::move(sample));
		
		// Trim history by size to avoid unbounded growth
		const size_t MAX_HISTORY = 240; // reduce?
		if (history.size() > MAX_HISTORY)
		{
			history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY));
		}
		
		// EWMA updates
		if (EWMA_fast_BytesPerSecond <= 0.0)
		{
			EWMA_fast_BytesPerSecond = payload_bytes_per_second;
		}
		else
		{
			EWMA_fast_BytesPerSecond = ALPHA_FAST * payload_bytes_per_second + (1.0 - ALPHA_FAST) * EWMA_fast_BytesPerSecond;
		}
		if (EWMA_slow_BytesPerSecond <= 0.0)
		{
			EWMA_slow_BytesPerSecond = payload_bytes_per_second;
		}
		else
		{
			EWMA_slow_BytesPerSecond = ALPHA_SLOW * payload_bytes_per_second + (1.0 - ALPHA_SLOW) * EWMA_slow_BytesPerSecond;
		}
		RecomputeHarmonicMeanAndMedianTTFB();
		return total_time_seconds;
	}

	/**
	 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
	 */
	double GetThroughputBytesPerSecond() const
	{
		double EWMA_min = (EWMA_fast_BytesPerSecond > 0.0 && EWMA_slow_BytesPerSecond > 0.0)
		? std::min(EWMA_fast_BytesPerSecond, EWMA_slow_BytesPerSecond)
		: std::max(EWMA_fast_BytesPerSecond, EWMA_slow_BytesPerSecond);
		if (EWMA_min <= 0.0)
		{
			return harmonic_BytesPerSecond;
		}
		if (harmonic_BytesPerSecond <= 0.0)
		{
			return EWMA_min;
		}
		const double w = 0.6; // 60% harmonic, 40% EWMA
		return w * harmonic_BytesPerSecond + (1.0 - w) * EWMA_min;
	}

	/**
	 * @brief return current overhead (TTFB) estimate (seconds)
	 */
	double GetTimeToFirstByteSeconds() const
	{
		return estimated_TFTB_seconds;
	}

	/**
	 * @brief predict completion time for a new segment
	 */
	double GetPredictedDownloadTimeSeconds(size_t segment_size_bytes) const
	{
		const double throughput = GetThroughputBytesPerSecond();
		if( throughput >= 1.0 )
		{
			return estimated_TFTB_seconds + (static_cast<double>(segment_size_bytes) / throughput);
		}
		else
		{ // we have no history data to make estimate
			return 0.0;
		}
	}
};

class DownloadContext
{
private:
	FILE *f = NULL; // logging
	static constexpr double ewma_short_window_weight = 0.4;
	double ewma_bytes_per_second = 0.0;
	curl_off_t dlnow_prev = 0;
	double time_prev = 0.0;

public:
	DownloadContext( FILE *f )
	{
		this->f = f;
	}
	
	/**
	 * @param dltotal total bytes to download
	 * @param dlnow downloaded bytes so far
	 */
	int xferinfo( curl_off_t dltotal, curl_off_t dlnow )
	{
		const double now = GetCurrentTimeMonotonicSeconds();
		if( time_prev > 0.0 && now > time_prev )
		{
			const curl_off_t delta_bytes = dlnow - dlnow_prev;
			const double delta_time = now - time_prev;
			
			if (delta_time > epsilon && delta_bytes > 0)
			{
				const double Bps = static_cast<double>(delta_bytes)/delta_time;
				if( ewma_bytes_per_second <= 0.0 )
				{
					ewma_bytes_per_second = Bps;
				}
				else
				{
					ewma_bytes_per_second =
					ewma_short_window_weight * Bps +
					(1.0 - ewma_short_window_weight) * ewma_bytes_per_second;
				}
			}
		}
		if( dlnow>dlnow_prev )
		{
			const auto remaining_bytes = dltotal - dlnow;
			const double remaining_time_estimate = remaining_bytes / ewma_bytes_per_second;
			fprintf( f, "%f,%ld,%ld,%ld,%f,%f\n",
					now,
					(dltotal>0)?(100*dlnow/dltotal):0,
					dlnow,
					dltotal,
					ewma_bytes_per_second,
					remaining_time_estimate );
		}
		dlnow_prev = dlnow;
		time_prev = now;
		return 0; // continue
		// returning 1 aborts transfer
	}
};

/** @brief libcurl progress callback (XFERINFOFUNCTION)
 * This is called at frequent intervals allowing application to monitor progress and abort stalled transfers
 * Note: this may be called even after all data has been downloaded while final logic is executed
 *
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes to upload
 * @param ulnow uploaded bytes so far
 */
static int xferinfo(void *clientp,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t ultotal, curl_off_t ulnow)
{
	auto *context = reinterpret_cast<DownloadContext*>(clientp);
	return context->xferinfo( dltotal, dlnow );
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
	std::string pathEWMA = std::string(path) + "/ewma.csv";
	FILE *f_EWMA = fopen(pathEWMA.c_str(),"wb");
	assert( f_EWMA );
	fprintf( f_EWMA, "Time,Pct,dlnow,dltotal,Bps,est remaining(s)\n" );
	
	std::string pathABR = std::string(path) + "/abr.csv";
	FILE *f_ABR = fopen(pathABR.c_str(),"wb");
	assert( f_ABR );
	
	const double segment_duration_seconds = 2.0; // playback duration of media segment
	const double representation_BytesPerSecond = 5000000/8;
	const double segment_size_bytes = representation_BytesPerSecond * segment_duration_seconds;
	
	CURL *curl = curl_easy_init();
	assert( curl );

	fprintf( f_ABR, "TTFB(s),Throughput(Bps),Predicted Download Time(s),Actual Download Time(s)\n" );

	NetworkBandwidthEstimator NetworkBandwidthEstimator;
	
	const int iteration_count = 10;
	for( int i=0; i<iteration_count; i++ )
	{
		// here we download same media segment(s) repeatedly on good network to collect baseline performance data
		const char * url = "https://aamp-test-content.s3.us-east-1.amazonaws.com/VideoTestStream/dash/1080p_001.m4s";
		
		fprintf( f_EWMA, "%f,%f\n", GetCurrentTimeMonotonicSeconds(), 0.0 );
		
		fprintf( f_ABR, "%f,%f,%f",
				NetworkBandwidthEstimator.GetTimeToFirstByteSeconds(),
				NetworkBandwidthEstimator.GetThroughputBytesPerSecond(),
				NetworkBandwidthEstimator.GetPredictedDownloadTimeSeconds(segment_size_bytes) );
		
		DownloadContext downloadContext(f_EWMA);
		
		curl_easy_setopt(curl, CURLOPT_URL, url);
		
		// enable CURLOPT_XFERINFOFUNCTION, modern alternative to CURLOPT_PROGRESSFUNCTION
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &downloadContext);
		
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // allow compressed transfers
		//curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L * 64); // larger buffer may produce smoother progress
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "throughput-estimator/1.0");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		const CURLcode res = curl_easy_perform(curl);
		// Create a sample from the completed (or partial) transfer
		if (res == CURLE_OK ) // || ctx.bailed)
		{ // note: if we bailed early, partial bytes are still fine for statistics
			double total_time_seconds = NetworkBandwidthEstimator.UpdateDownloadMetrics(curl);
			fprintf( f_ABR, ",%f\n", total_time_seconds );
		}
		else
		{
			std::fprintf(stderr, "curl_easy_perform error: %s\n", curl_easy_strerror(res));
		}
		fprintf( f_EWMA, "\n" );
	}
	fclose( f_ABR );
	fclose( f_EWMA );
	curl_easy_cleanup(curl);

	return EXIT_SUCCESS;
}
