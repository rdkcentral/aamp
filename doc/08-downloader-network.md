# Downloader & Network Layer

## Overview

AAMP uses libcurl as the foundation for all HTTP/HTTPS downloads, providing a robust network layer with connection pooling, intelligent retry logic, comprehensive download metrics, and configurable timeout handling. The downloader system handles manifest downloads, fragment downloads, license acquisitions, and all other HTTP-based network operations required for adaptive streaming playback.

The architecture separates download execution (`AampCurlDownloader`) from connection management (`AampCurlStore`), enabling efficient connection reuse and resource optimization. The system provides detailed metrics for each download operation, supporting bandwidth estimation, ABR decisions, and performance monitoring.

## Architecture

**Files**: `downloader/AampCurlDownloader.h/cpp`, `downloader/AampCurlStore.h/cpp`

**Key Classes**:
- **`AampCurlDownloader`**: Main download manager that executes HTTP/HTTPS downloads using libcurl. Handles download configuration, execution, retry logic, timeout management, and metrics collection. Each downloader instance maintains its own curl handle (`mCurl`) and download state (`mDownloadActive`, `mDownloadResponse`).
- **`AampCurlStore`**: Connection store that maintains a pool of curl handles for connection reuse. The store maps hostnames to curl handles, allowing multiple downloads to the same host to reuse existing TCP connections, reducing connection establishment overhead and improving download performance.

## Features

### Connection Reuse

`AampCurlStore` maintains a pool of curl handles organized by hostname, enabling efficient connection reuse:

- **Hostname-Based Pooling**: The store maintains curl handles keyed by hostname (extracted from URL), allowing downloads to the same host to reuse existing TCP connections. This eliminates the overhead of TCP handshake, TLS negotiation, and DNS resolution for subsequent downloads to the same host.
- **Connection Lifecycle**: Connections are created on-demand when first needed for a hostname and reused for subsequent downloads. Connections are kept alive between downloads (HTTP keep-alive) and automatically cleaned up when no longer needed or when connection errors occur.
- **Performance Benefits**: Connection reuse significantly reduces download latency, especially for manifest refreshes and fragment downloads where multiple requests go to the same CDN hostname. Typical improvements include 20-50ms latency reduction per download after the initial connection establishment.

The connection store uses mutex protection (`mCurlMutex`) to ensure thread-safe access when multiple download threads access the same curl handle concurrently.

### Download Metrics

The downloader tracks comprehensive metrics for each download operation, providing detailed performance data:

- **Timing Metrics**: `downloadCompleteMetrics` structure captures timing information including total download time (`total`), connection establishment time (`connect`), time to first byte (`startTransfer`), DNS resolution time (`resolve`), TLS handshake time (`appConnect`), and redirect time (`redirect`). These metrics enable detailed performance analysis and bottleneck identification.
- **Size Metrics**: Tracks downloaded size (`dlSize`), request size (`reqSize`), and calculates bandwidth (`downloadbps` = dlSize / total_time). Bandwidth calculations are used by `NetworkBandwidthEstimator` for ABR decisions.
- **HTTP Metrics**: Captures HTTP response code (`iHttpRetValue`), effective URL after redirects (`sEffectiveUrl`), and response headers (`mResponseHeader`). Error codes and headers are used for error handling and debugging.
- **Curl-Specific Metrics**: Extracts libcurl-specific metrics via `CURLINFO` queries, including `CURLINFO_SIZE_DOWNLOAD`, `CURLINFO_TOTAL_TIME`, `CURLINFO_STARTTRANSFER_TIME`, and other curl performance data.

Metrics are stored in `_downloadResponse` structure and can be logged via `show()` method for debugging. The metrics are also used by `NetworkBandwidthEstimator` for bandwidth estimation and ABR decisions.

### Retry Logic

The downloader implements sophisticated retry logic to handle transient network failures:

- **Configurable Retry Count**: Each download operation supports configurable retry attempts (`iDownloadRetryCount` in `_downloadConfig`). The retry count can be set per-download or globally via configuration. Failed downloads are retried up to the configured limit.
- **Exponential Backoff**: Retry attempts use exponential backoff delays, starting with `waitTimeBeforeRetryHttp5xx` (default: 1 second) and increasing for subsequent retries. This prevents overwhelming servers during outages while providing reasonable retry timing.
- **Profile Rampdown on Retries**: When fragment downloads fail repeatedly, the ABR system may ramp down to lower quality profiles to reduce fragment sizes and improve download success rates. This adaptive retry strategy balances quality with reliability.
- **Error-Specific Handling**: Different error types trigger different retry behaviors. HTTP 5xx errors (server errors) trigger retries with backoff, while HTTP 4xx errors (client errors) may not retry depending on the specific error code. Network timeouts trigger immediate retries, while connection failures may use longer backoff.

The retry logic is implemented in the `Download()` method's retry loop, which continues until success, maximum retries reached, or non-retryable errors occur.

### Timeout Handling

The downloader supports multiple timeout types to handle various network failure scenarios:

- **`connectTimeout`**: Socket connection timeout in seconds (default: 3s). This controls how long the system waits to establish a TCP connection to the server. Short timeouts prevent hanging on unreachable hosts but may cause premature failures on slow networks.
- **`networkTimeout`**: Overall download timeout in seconds (default: 10s). This is the maximum time allowed for the entire download operation, including connection, download, and processing. Downloads exceeding this timeout are aborted and may trigger retries.
- **`stallTimeout`**: Stall detection timeout in seconds (default: 0s, disabled). When enabled, monitors download progress and detects when downloads stall (no data received for the timeout period). Stalled downloads are aborted and retried, preventing hangs on slow or stuck connections.
- **`startTimeout`**: Time to first byte timeout in seconds (default: 0s, disabled). Controls how long to wait for the first data byte after connection establishment. This detects server-side delays or connection issues that don't trigger connection timeouts.

Timeout values are configured in `_downloadConfig` structure and can be set per-download or globally. The system uses curl's timeout options (`CURLOPT_CONNECTTIMEOUT`, `CURLOPT_TIMEOUT`) and progress callbacks to implement timeout detection.

## Configuration

The downloader system exposes comprehensive configuration parameters for network behavior:

- **`networkTimeout`**: Overall download timeout in seconds (default: 10s). Maximum time allowed for complete download operations. Downloads exceeding this timeout are aborted and may trigger retries or error handling.

- **`connectTimeout`**: Socket connection timeout in seconds (default: 3s). Time to wait for TCP connection establishment. Shorter values prevent hanging on unreachable hosts but may cause premature failures.

- **`downloadStallTimeout`**: Stall detection timeout in seconds (default: 0s, disabled). When enabled, detects when downloads stall (no progress for timeout period) and aborts stalled downloads for retry.

- **`downloadStartTimeout`**: Time to first byte timeout in seconds (default: 0s, disabled). Maximum time to wait for first data byte after connection. Detects server-side delays.

- **`networkProxy`**: Proxy server address for all downloads (default: none). Allows routing download traffic through HTTP/HTTPS proxy servers for network configuration or security requirements.

- **`licenseProxy`**: Proxy server address specifically for license requests. Allows separate proxy configuration for license traffic vs. media downloads.

- **`sslVerifyPeer`**: Enable/disable SSL peer certificate verification (default: false, disabled). When enabled, validates server SSL certificates against CA certificates. Disabling allows self-signed certificates but reduces security.

- **`dnsCacheTimeout`**: DNS cache entry lifetime in seconds (default: 180s). DNS resolution results are cached for this duration to reduce DNS lookup overhead for repeated hostname resolutions.

- **`userAgent`**: HTTP User-Agent header string for download requests (default: Mozilla/5.0...). Some servers require specific user agents or use them for analytics/blocking.

- **`customHeader`**: Custom HTTP headers to include in download requests. Useful for authentication, content identification, or CDN-specific headers.

- **`uriParameter`**: URI parameters to append to download URLs. Allows adding query parameters to fragment URLs for CDN configuration or tracking.

- **`sharedSSL`**: Enable/disable shared SSL session across downloads (default: true). When enabled, SSL sessions are reused for multiple downloads to the same host, reducing TLS handshake overhead.

- **`fragmentDownloadFailThreshold`**: Maximum retry attempts for non-init fragment curl timeout failures (range 1-10, default: 10). Controls retry behavior for fragment download failures.

- **`initFragmentRetryCount`**: Max retry attempts for init fragment curl timeout failures (default: 1). Init fragments are critical for playback, so they get extra retry attempts.

## Summary

The downloader provides a robust, efficient network layer for adaptive streaming:

- **Efficient Connection Reuse**: Hostname-based connection pooling eliminates connection establishment overhead for repeated downloads to the same hosts. This significantly improves download performance, especially for manifest refreshes and fragment sequences from the same CDN.

- **Comprehensive Error Handling**: Sophisticated retry logic with exponential backoff handles transient network failures gracefully. Error-specific handling and profile rampdown on failures ensure reliable playback even under adverse network conditions.

- **Detailed Metrics**: Comprehensive download metrics enable bandwidth estimation, performance monitoring, and debugging. Timing breakdowns (connection, DNS, TLS, download) help identify performance bottlenecks and optimize network behavior.

- **Configurable Timeouts**: Multiple timeout types (connection, download, stall, start) provide fine-grained control over network behavior. Configurable timeouts allow adaptation to different network environments (fast/slow, stable/unstable) and use cases (low-latency vs. high-quality).
