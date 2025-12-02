---
applyTo:
  - "**/*.js"
  - "**/*.ts"
  - "**/*.jsx"
  - "**/*.tsx"
  - "**/*.mjs"
---

# JavaScript/TypeScript Copilot Instructions

## JavaScript/TypeScript Guidelines

- Use TypeScript for new JavaScript projects to ensure type safety
- Follow the [Airbnb JavaScript Style Guide](https://github.com/airbnb/javascript)
- Prefer const over let, avoid var
- Use arrow functions for callbacks and short functions
- Apply async/await over Promises for better readability
- Use meaningful variable and function names
- Include JSDoc comments for functions and classes

## TypeScript Patterns

### Interface Definitions for Media Streaming
```typescript
interface StreamConfig {
    readonly url: string;
    readonly bitrate: number;
    readonly resolution: {
        width: number;
        height: number;
    };
    readonly codec: 'h264' | 'h265' | 'vp9';
    readonly audioEnabled: boolean;
    readonly subtitles?: string;
}

interface PlayerState {
    isPlaying: boolean;
    currentTime: number;
    duration: number;
    buffered: number;
    error?: string;
}

interface PlayerEvents {
    onPlay: () => void;
    onPause: () => void;
    onError: (error: string) => void;
    onTimeUpdate: (time: number) => void;
}
```

### Class Implementation with Proper Types
```typescript
/**
 * Media player wrapper for AAMP integration
 */
class MediaPlayer {
    private state: PlayerState;
    private config: StreamConfig;
    private eventHandlers: Partial<PlayerEvents>;

    constructor(config: StreamConfig, eventHandlers: Partial<PlayerEvents> = {}) {
        this.config = config;
        this.eventHandlers = eventHandlers;
        this.state = {
            isPlaying: false,
            currentTime: 0,
            duration: 0,
            buffered: 0
        };
    }

    /**
     * Initialize the media player
     * @returns Promise that resolves when player is ready
     */
    async initialize(): Promise<void> {
        try {
            await this.loadStream(this.config.url);
            this.setupEventListeners();
        } catch (error) {
            const errorMessage = error instanceof Error ? error.message : 'Unknown error';
            this.handleError(errorMessage);
            throw error;
        }
    }

    /**
     * Start playback
     * @returns Promise that resolves when playback starts
     */
    async play(): Promise<void> {
        if (this.state.isPlaying) {
            return;
        }

        try {
            await this.performPlay();
            this.updateState({ isPlaying: true });
            this.eventHandlers.onPlay?.();
        } catch (error) {
            const errorMessage = error instanceof Error ? error.message : 'Playback failed';
            this.handleError(errorMessage);
        }
    }

    private async loadStream(url: string): Promise<void> {
        // Implementation for stream loading
        return new Promise((resolve, reject) => {
            // Simulate async operation
            setTimeout(() => {
                if (this.isValidUrl(url)) {
                    resolve();
                } else {
                    reject(new Error(`Invalid stream URL: ${url}`));
                }
            }, 100);
        });
    }

    private updateState(updates: Partial<PlayerState>): void {
        this.state = { ...this.state, ...updates };
    }

    private handleError(message: string): void {
        this.updateState({ error: message });
        this.eventHandlers.onError?.(message);
    }

    private isValidUrl(url: string): boolean {
        try {
            new URL(url);
            return true;
        } catch {
            return false;
        }
    }
}
```

## Modern JavaScript Patterns

### Async/Await for Stream Operations
```typescript
/**
 * Stream manager for handling multiple streams
 */
class StreamManager {
    private streams = new Map<string, MediaPlayer>();

    /**
     * Add a new stream to the manager
     */
    async addStream(id: string, config: StreamConfig): Promise<void> {
        try {
            const player = new MediaPlayer(config, {
                onError: (error) => this.handleStreamError(id, error)
            });
            
            await player.initialize();
            this.streams.set(id, player);
            
            console.log(`Stream ${id} added successfully`);
        } catch (error) {
            console.error(`Failed to add stream ${id}:`, error);
            throw error;
        }
    }

    /**
     * Start playback for all streams
     */
    async playAllStreams(): Promise<void> {
        const playPromises = Array.from(this.streams.values())
            .map(player => player.play());
        
        try {
            await Promise.allSettled(playPromises);
        } catch (error) {
            console.error('Some streams failed to start:', error);
        }
    }

    private handleStreamError(streamId: string, error: string): void {
        console.error(`Stream ${streamId} error:`, error);
        // Optionally remove failed stream
        this.streams.delete(streamId);
    }
}
```

### Utility Functions with Proper Types
```typescript
/**
 * Utility functions for media operations
 */

/**
 * Convert seconds to human-readable time format
 */
const formatTime = (seconds: number): string => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = Math.floor(seconds % 60);
    
    if (hours > 0) {
        return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
    }
    return `${minutes}:${secs.toString().padStart(2, '0')}`;
};

/**
 * Debounce function for reducing API calls
 */
const debounce = <T extends (...args: any[]) => void>(
    func: T,
    delay: number
): ((...args: Parameters<T>) => void) => {
    let timeoutId: number | undefined;
    
    return (...args: Parameters<T>) => {
        clearTimeout(timeoutId);
        timeoutId = window.setTimeout(() => func(...args), delay);
    };
};

/**
 * Validate stream configuration
 */
const validateStreamConfig = (config: unknown): config is StreamConfig => {
    return (
        typeof config === 'object' &&
        config !== null &&
        'url' in config &&
        'bitrate' in config &&
        'resolution' in config &&
        typeof (config as any).url === 'string' &&
        typeof (config as any).bitrate === 'number' &&
        (config as any).bitrate > 0
    );
};
```

## Error Handling and Logging

### Custom Error Classes
```typescript
class StreamError extends Error {
    constructor(
        message: string,
        public readonly streamId: string,
        public readonly code: string
    ) {
        super(message);
        this.name = 'StreamError';
    }
}

class NetworkError extends StreamError {
    constructor(streamId: string, message: string) {
        super(message, streamId, 'NETWORK_ERROR');
        this.name = 'NetworkError';
    }
}

class CodecError extends StreamError {
    constructor(streamId: string, codec: string) {
        super(`Unsupported codec: ${codec}`, streamId, 'CODEC_ERROR');
        this.name = 'CodecError';
    }
}
```

### Logging Utility
```typescript
enum LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG = 3
}

class Logger {
    private static instance: Logger;
    private logLevel: LogLevel = LogLevel.INFO;

    static getInstance(): Logger {
        if (!Logger.instance) {
            Logger.instance = new Logger();
        }
        return Logger.instance;
    }

    setLogLevel(level: LogLevel): void {
        this.logLevel = level;
    }

    error(message: string, ...args: any[]): void {
        if (this.logLevel >= LogLevel.ERROR) {
            console.error(`[ERROR] ${message}`, ...args);
        }
    }

    warn(message: string, ...args: any[]): void {
        if (this.logLevel >= LogLevel.WARN) {
            console.warn(`[WARN] ${message}`, ...args);
        }
    }

    info(message: string, ...args: any[]): void {
        if (this.logLevel >= LogLevel.INFO) {
            console.info(`[INFO] ${message}`, ...args);
        }
    }

    debug(message: string, ...args: any[]): void {
        if (this.logLevel >= LogLevel.DEBUG) {
            console.debug(`[DEBUG] ${message}`, ...args);
        }
    }
}

// Usage
const logger = Logger.getInstance();
logger.info('Stream initialized', { streamId: 'stream1', url: 'http://example.com' });
```

## Integration with Build Tools

### Module Exports for Node.js/Build Integration
```typescript
// For Node.js build scripts
export interface BuildConfig {
    inputDir: string;
    outputDir: string;
    target: 'development' | 'production';
    minify: boolean;
}

export const defaultBuildConfig: BuildConfig = {
    inputDir: './src',
    outputDir: './dist',
    target: 'development',
    minify: false
};

/**
 * Build script for processing media assets
 */
export async function buildMediaAssets(config: BuildConfig): Promise<void> {
    const { inputDir, outputDir, target, minify } = config;
    
    console.log(`Building media assets from ${inputDir} to ${outputDir}`);
    console.log(`Target: ${target}, Minify: ${minify}`);
    
    // Implementation for asset processing
    try {
        await processAssets(inputDir, outputDir, { minify, target });
        console.log('Build completed successfully');
    } catch (error) {
        console.error('Build failed:', error);
        process.exit(1);
    }
}

// For CommonJS environments
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { buildMediaAssets, defaultBuildConfig };
}
```

## Web API Integration

### Fetch with Proper Error Handling
```typescript
/**
 * HTTP client for API communication
 */
class ApiClient {
    private baseUrl: string;
    private timeout: number;

    constructor(baseUrl: string, timeout = 5000) {
        this.baseUrl = baseUrl;
        this.timeout = timeout;
    }

    async get<T>(endpoint: string): Promise<T> {
        return this.request<T>('GET', endpoint);
    }

    async post<T>(endpoint: string, data: unknown): Promise<T> {
        return this.request<T>('POST', endpoint, data);
    }

    private async request<T>(
        method: string,
        endpoint: string,
        data?: unknown
    ): Promise<T> {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeout);

        try {
            const response = await fetch(`${this.baseUrl}${endpoint}`, {
                method,
                headers: {
                    'Content-Type': 'application/json',
                },
                body: data ? JSON.stringify(data) : undefined,
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json() as T;
        } catch (error) {
            clearTimeout(timeoutId);
            
            if (error instanceof Error && error.name === 'AbortError') {
                throw new Error(`Request timeout (${this.timeout}ms)`);
            }
            
            throw error;
        }
    }
}
```
