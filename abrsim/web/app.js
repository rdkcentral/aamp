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

/**
 * ABR Simulator Web UI
 * Main application logic
 */

const API_BASE = '';

// Chart instances
let bitrateChart = null;
let bufferChart = null;
let bandwidthChart = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
	initializeUI();
	loadPersonas();
	setupEventListeners();
});

function initializeUI() {
	// Initialize Chart.js with common options
	Chart.defaults.font.family = '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, sans-serif';
	Chart.defaults.color = '#6c757d';
}

async function loadPersonas() {
	try {
		const response = await fetch(`${API_BASE}/api/personas`);
		const data = await response.json();
		
		const personaSelect = document.getElementById('persona');
		personaSelect.innerHTML = '';
		
		data.personas.forEach(persona => {
			const option = document.createElement('option');
			option.value = persona.filename;
			option.textContent = `${persona.name} (${persona.bandwidth} Mbps)`;
			personaSelect.appendChild(option);
		});
	} catch (error) {
		console.error('Failed to load personas:', error);
		showStatus('Failed to load network personas', 'error');
	}
}

function setupEventListeners() {
	const runBtn = document.getElementById('runBtn');
	const isLiveCheckbox = document.getElementById('isLive');
	
	runBtn.addEventListener('click', runSimulation);
	
	isLiveCheckbox.addEventListener('change', (e) => {
		const liveSettings = document.getElementById('liveSettings');
		const vodSettings = document.getElementById('vodSettings');
		
		if (e.target.checked) {
			liveSettings.style.display = 'block';
			vodSettings.style.display = 'none';
		} else {
			liveSettings.style.display = 'none';
			vodSettings.style.display = 'block';
		}
	});
}

async function runSimulation() {
	const runBtn = document.getElementById('runBtn');
	const originalText = runBtn.textContent;
	
	try {
		console.log('Starting simulation...');
		
		// Disable button and show loading state
		runBtn.disabled = true;
		runBtn.innerHTML = '<span class="spinner"></span> Running Simulation...';
		showStatus('Running simulation... This may take a few seconds', 'info');
		
		// Collect parameters
		const params = {
			persona: document.getElementById('persona').value,
			duration: parseFloat(document.getElementById('duration').value),
			is_live: document.getElementById('isLive').checked,
			target_latency: parseFloat(document.getElementById('targetLatency').value),
			max_buffer: parseFloat(document.getElementById('maxBuffer').value),
			seed: parseInt(document.getElementById('seed').value)
		};
		
		console.log('Simulation parameters:', params);
		
		// Run simulation
		const response = await fetch(`${API_BASE}/api/simulate`, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify(params)
		});
		
		console.log('Response status:', response.status);
		
		if (!response.ok) {
			const errorText = await response.text();
			console.error('Response error:', errorText);
			throw new Error(`Simulation failed: ${response.statusText}`);
		}
		
		const result = await response.json();
		console.log('Received result:', result);
		
		if (!result.success) {
			throw new Error('Simulation returned an error');
		}
		
		console.log(`Processing ${result.events.length} events...`);
		
		// Display results
		displaySummary(result.summary, params);
		displayCharts(result.events);
		
		console.log('Charts and summary displayed');
		showStatus(`Simulation completed successfully! Processed ${result.events.length} events.`, 'success');
		
	} catch (error) {
		console.error('Simulation error:', error);
		showStatus(`Error: ${error.message}`, 'error');
	} finally {
		runBtn.disabled = false;
		runBtn.textContent = originalText;
	}
}

function displaySummary(summary, params) {
	const summaryDiv = document.getElementById('summary');
	
	// Build summary HTML
	let html = '<div class="summary-grid">';
	
	// Rebuffer events
	const rebufferClass = summary.rebuffer_events === 0 ? 'good' : 
	                      summary.rebuffer_events < 3 ? 'warning' : 'bad';
	html += `
		<div class="summary-item ${rebufferClass}">
			<label>Rebuffer Events</label>
			<div class="value">${summary.rebuffer_events || 0}</div>
		</div>
	`;
	
	// Rebuffer time
	const rebufferTime = summary.total_rebuffer_time || 0;
	const rebufferTimeClass = rebufferTime === 0 ? 'good' : 
	                          rebufferTime < 5 ? 'warning' : 'bad';
	html += `
		<div class="summary-item ${rebufferTimeClass}">
			<label>Total Rebuffer Time</label>
			<div class="value">${rebufferTime.toFixed(1)} <span class="unit">seconds</span></div>
		</div>
	`;
	
	// Final buffer
	if (summary.final_buffer !== undefined) {
		const bufferClass = summary.final_buffer > 10 ? 'good' : 
		                    summary.final_buffer > 5 ? 'warning' : 'bad';
		html += `
			<div class="summary-item ${bufferClass}">
				<label>Final Buffer Level</label>
				<div class="value">${summary.final_buffer.toFixed(1)} <span class="unit">seconds</span></div>
			</div>
		`;
	}
	
	// Latency (for live)
	if (params.is_live && summary.avg_latency !== undefined) {
		const latencyDrift = Math.abs(summary.avg_latency - params.target_latency);
		const latencyClass = latencyDrift < 1 ? 'good' : 
		                     latencyDrift < 3 ? 'warning' : 'bad';
		html += `
			<div class="summary-item ${latencyClass}">
				<label>Avg Latency (Target: ${params.target_latency}s)</label>
				<div class="value">${summary.avg_latency.toFixed(1)} <span class="unit">seconds</span></div>
			</div>
		`;
	}
	
	// Speed-up factor
	if (summary.speedup !== undefined) {
		html += `
			<div class="summary-item good">
				<label>Simulation Speed-up</label>
				<div class="value">${summary.speedup.toFixed(1)}<span class="unit">x</span></div>
			</div>
		`;
	}
	
	html += '</div>';
	summaryDiv.innerHTML = html;
}

function displayCharts(events) {
	// Extract data points
	const downloads = events.filter(e => e.event_type === 'download');
	const profileChanges = events.filter(e => e.event_type === 'profile_change');
	
	// Prepare time series data
	const times = downloads.map(e => e.time_s);
	const bitrates = downloads.map(e => getBitrateFromProfile(e.profile_idx));
	const bufferLevels = downloads.map(e => e.buffer_s);
	const throughputs = downloads.map(e => e.throughput_bps / 1000000); // Convert to Mbps
	
	// Create/update charts
	createBitrateChart(times, bitrates, profileChanges);
	createBufferChart(times, bufferLevels);
	createBandwidthChart(times, throughputs, bitrates);
}

function createBitrateChart(times, bitrates, profileChanges) {
	const ctx = document.getElementById('bitrateChart');
	
	if (bitrateChart) {
		bitrateChart.destroy();
	}
	
	bitrateChart = new Chart(ctx, {
		type: 'line',
		data: {
			labels: times,
			datasets: [{
				label: 'Selected Bitrate',
				data: bitrates,
				borderColor: '#0066cc',
				backgroundColor: 'rgba(0, 102, 204, 0.1)',
				borderWidth: 2,
				stepped: 'before',
				pointRadius: 0,
				tension: 0
			}]
		},
		options: {
			responsive: true,
			maintainAspectRatio: true,
			plugins: {
				legend: {
					display: true,
					position: 'top'
				},
				tooltip: {
					mode: 'index',
					intersect: false,
					callbacks: {
						label: (context) => {
							return `Bitrate: ${(context.parsed.y / 1000).toFixed(1)} kbps`;
						}
					}
				}
			},
			scales: {
				x: {
					title: {
						display: true,
						text: 'Time (seconds)'
					},
					ticks: {
						maxTicksLimit: 10
					}
				},
				y: {
					title: {
						display: true,
						text: 'Bitrate (bps)'
					},
					ticks: {
						callback: (value) => `${(value / 1000).toFixed(0)}k`
					}
				}
			},
			interaction: {
				mode: 'nearest',
				axis: 'x',
				intersect: false
			}
		}
	});
}

function createBufferChart(times, bufferLevels) {
	const ctx = document.getElementById('bufferChart');
	
	if (bufferChart) {
		bufferChart.destroy();
	}
	
	bufferChart = new Chart(ctx, {
		type: 'line',
		data: {
			labels: times,
			datasets: [{
				label: 'Buffer Level',
				data: bufferLevels,
				borderColor: '#28a745',
				backgroundColor: 'rgba(40, 167, 69, 0.1)',
				borderWidth: 2,
				fill: true,
				pointRadius: 0,
				tension: 0.2
			}]
		},
		options: {
			responsive: true,
			maintainAspectRatio: true,
			plugins: {
				legend: {
					display: true,
					position: 'top'
				},
				tooltip: {
					mode: 'index',
					intersect: false,
					callbacks: {
						label: (context) => {
							return `Buffer: ${context.parsed.y.toFixed(1)}s`;
						}
					}
				}
			},
			scales: {
				x: {
					title: {
						display: true,
						text: 'Time (seconds)'
					},
					ticks: {
						maxTicksLimit: 10
					}
				},
				y: {
					title: {
						display: true,
						text: 'Buffer (seconds)'
					},
					min: 0
				}
			},
			interaction: {
				mode: 'nearest',
				axis: 'x',
				intersect: false
			}
		}
	});
}

function createBandwidthChart(times, throughputs, bitrates) {
	const ctx = document.getElementById('bandwidthChart');
	
	if (bandwidthChart) {
		bandwidthChart.destroy();
	}
	
	bandwidthChart = new Chart(ctx, {
		type: 'line',
		data: {
			labels: times,
			datasets: [
				{
					label: 'Measured Throughput',
					data: throughputs,
					borderColor: '#ffc107',
					backgroundColor: 'rgba(255, 193, 7, 0.1)',
					borderWidth: 2,
					pointRadius: 1,
					tension: 0.2
				},
				{
					label: 'Selected Bitrate',
					data: bitrates.map(b => b / 1000000), // Convert to Mbps
					borderColor: '#0066cc',
					backgroundColor: 'transparent',
					borderWidth: 2,
					borderDash: [5, 5],
					stepped: 'before',
					pointRadius: 0,
					tension: 0
				}
			]
		},
		options: {
			responsive: true,
			maintainAspectRatio: true,
			plugins: {
				legend: {
					display: true,
					position: 'top'
				},
				tooltip: {
					mode: 'index',
					intersect: false,
					callbacks: {
						label: (context) => {
							const label = context.dataset.label;
							const value = context.parsed.y.toFixed(2);
							return `${label}: ${value} Mbps`;
						}
					}
				}
			},
			scales: {
				x: {
					title: {
						display: true,
						text: 'Time (seconds)'
					},
					ticks: {
						maxTicksLimit: 10
					}
				},
				y: {
					title: {
						display: true,
						text: 'Bandwidth (Mbps)'
					},
					min: 0
				}
			},
			interaction: {
				mode: 'nearest',
				axis: 'x',
				intersect: false
			}
		}
	});
}

function getBitrateFromProfile(profileIdx) {
	// Standard profile bitrates (matching abrsim.cpp)
	const profiles = [
		235000,   // 0: 240p
		375000,   // 1: 360p
		750000,   // 2: 480p
		1400000,  // 3: 720p
		2800000,  // 4: 1080p
		5000000,  // 5: 1080p high
		8000000   // 6: 4K
	];
	return profiles[profileIdx] || 0;
}

function showStatus(message, type = 'info') {
	const statusDiv = document.getElementById('status');
	statusDiv.textContent = message;
	statusDiv.className = `status ${type}`;
	statusDiv.style.display = 'block';
}
