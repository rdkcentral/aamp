#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Compare ABR simulation results from two runs.
Useful for validating ABR algorithm changes.
"""

import sys
import csv
from collections import defaultdict
from typing import Dict, List, Tuple

def load_simulation(filename: str) -> Tuple[List[Dict], Dict]:
    """Load simulation CSV and compute statistics."""
    events = []
    stats = {
        'total_downloads': 0,
        'profile_changes': 0,
        'rebuffer_events': 0,
        'total_duration': 0.0,
        'profile_time': defaultdict(float),
        'avg_throughput': 0.0,
        'final_buffer': 0.0
    }
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            events.append(row)
            
            event_type = row['event_type']
            time_s = float(row['time_s'])
            
            if event_type == 'download':
                stats['total_downloads'] += 1
                stats['avg_throughput'] += float(row['throughput_bps'])
            elif event_type == 'profile_change':
                stats['profile_changes'] += 1
            elif event_type == 'rebuffer_start':
                stats['rebuffer_events'] += 1
            
            stats['total_duration'] = max(stats['total_duration'], time_s)
            stats['final_buffer'] = float(row['buffer_s'])
    
    if stats['total_downloads'] > 0:
        stats['avg_throughput'] /= stats['total_downloads']
    
    # Calculate time spent in each profile
    current_profile = None
    last_time = 0.0
    for event in events:
        event_type = event['event_type']
        time_s = float(event['time_s'])
        profile_idx = int(event['profile_idx'])
        
        if current_profile is not None:
            stats['profile_time'][current_profile] += (time_s - last_time)
        
        if event_type == 'profile_change':
            current_profile = profile_idx
        elif event_type == 'download' and current_profile is None:
            current_profile = profile_idx
        
        last_time = time_s
    
    return events, stats

def print_stats(label: str, stats: Dict):
    """Print formatted statistics."""
    print(f"\n{label}:")
    print(f"  Total duration: {stats['total_duration']:.1f} seconds")
    print(f"  Downloads: {stats['total_downloads']}")
    print(f"  Profile changes: {stats['profile_changes']}")
    print(f"  Rebuffer events: {stats['rebuffer_events']}")
    print(f"  Avg throughput: {stats['avg_throughput'] / 1e6:.2f} Mbps")
    print(f"  Final buffer: {stats['final_buffer']:.2f} seconds")
    
    if stats['profile_time']:
        print(f"\n  Time per profile:")
        total_time = sum(stats['profile_time'].values())
        for profile, time_s in sorted(stats['profile_time'].items()):
            percentage = (time_s / total_time) * 100 if total_time > 0 else 0
            print(f"    Profile {profile}: {time_s:.1f}s ({percentage:.1f}%)")

def compare_simulations(file1: str, file2: str):
    """Compare two simulation runs."""
    print(f"Comparing simulations:")
    print(f"  Baseline: {file1}")
    print(f"  Modified: {file2}")
    
    events1, stats1 = load_simulation(file1)
    events2, stats2 = load_simulation(file2)
    
    print_stats("Baseline Results", stats1)
    print_stats("Modified Results", stats2)
    
    # Print differences
    print("\n=== Comparison ===")
    
    download_diff = stats2['total_downloads'] - stats1['total_downloads']
    print(f"Download count change: {download_diff:+d}")
    
    change_diff = stats2['profile_changes'] - stats1['profile_changes']
    print(f"Profile changes: {change_diff:+d}")
    
    rebuffer_diff = stats2['rebuffer_events'] - stats1['rebuffer_events']
    print(f"Rebuffer events: {rebuffer_diff:+d}")
    
    throughput_diff = (stats2['avg_throughput'] - stats1['avg_throughput']) / 1e6
    print(f"Avg throughput change: {throughput_diff:+.2f} Mbps")
    
    buffer_diff = stats2['final_buffer'] - stats1['final_buffer']
    print(f"Final buffer change: {buffer_diff:+.2f} seconds")
    
    # QoE summary
    print("\n=== Quality of Experience ===")
    rebuffer_worsened = rebuffer_diff > 0
    changes_increased = abs(change_diff) > stats1['profile_changes'] * 0.2
    
    if rebuffer_worsened:
        print("⚠️  WARNING: More rebuffering events in modified version")
    elif rebuffer_diff < 0:
        print("✓ IMPROVEMENT: Fewer rebuffering events")
    else:
        print("✓ STABLE: Same rebuffering behavior")
    
    if changes_increased:
        print("⚠️  NOTE: Significantly different profile switching behavior")
    else:
        print("✓ STABLE: Similar profile switching behavior")

def main():
    if len(sys.argv) != 3:
        print("Usage: python compare_results.py baseline.csv modified.csv")
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    
    try:
        compare_simulations(file1, file2)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error comparing results: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
