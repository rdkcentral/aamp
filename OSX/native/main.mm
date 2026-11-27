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

#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Redirect stdout/stderr (where AAMP logs) to the log file.
static NSString *RedirectLogs(void)
{
	const char *env = getenv("AAMP_PLAYER_LOG");
	NSString *path = env ? @(env) : [NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:@"output.log"];
	int fd = open(path.fileSystemRepresentation, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
	{
		fprintf(stderr, "Cannot open log file %s, logging to console\n", path.fileSystemRepresentation);
		return path;
	}
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);
	return path;
}

int main(int argc, char *argv[])
{
	@autoreleasepool
	{
		NSString *logPath = RedirectLogs();
		NSApplication *app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
		AppDelegate *delegate = [[AppDelegate alloc] init];
		delegate.logPath = logPath;
		if (argc > 1 && argv[1][0] != '-')
		{
			delegate.initialURL = @(argv[1]);
		}
		app.delegate = delegate;
		[app run];
	}
	return 0;
}
