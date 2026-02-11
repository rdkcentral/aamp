# Downloader & Network Layer

## Overview

AAMP uses libcurl for all HTTP/HTTPS downloads with connection pooling, retry logic, and comprehensive metrics.

## Architecture

**Files**: `downloader/AampCurlDownloader.h/cpp`, `downloader/AampCurlStore.h/cpp`

**Key Classes**:
- `AampCurlDownloader`: Download manager
- `AampCurlStore`: Connection store for reuse

## Features

### Connection Reuse

`AampCurlStore` maintains a pool of curl handles:
- Reuses connections for same host
- Reduces connection overhead
- Improves download performance

### Download Metrics

Tracks comprehensive metrics:
- Download time
- Connection time
- Bandwidth calculation
- Error codes

### Retry Logic

Automatic retry on failures:
- Configurable retry count
- Exponential backoff
- Profile rampdown on retries

### Timeout Handling

Multiple timeout types:
- `connectTimeout`: Connection timeout
- `networkTimeout`: Download timeout
- `stallTimeout`: Stall detection
- `startTimeout`: First byte timeout

## Configuration

Key download configuration:
- `networkTimeout`: Download timeout (seconds)
- `connectTimeout`: Connection timeout (seconds)
- `downloadStallTimeout`: Stall timeout (seconds)
- `networkProxy`: Proxy server
- `sslVerifyPeer`: SSL verification

## Summary

The downloader provides:
- Efficient connection reuse
- Comprehensive error handling
- Detailed metrics
- Configurable timeouts
