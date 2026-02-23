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

async function loadScenarios() {
	try {
		const response = await fetch(`${API_BASE}/api/scenarios`);
		const data = await response.json();
		
		const scenarioSelect = document.getElementById('scenario');
		scenarioSelect.innerHTML = '';
		
		if (data.scenarios.length === 0) {
			const option = document.createElement('option');
			option.value = '';
			option.textContent = 'No scenarios available';
			scenarioSelect.appendChild(option);
			return;
		}
		
		data.scenarios.forEach(scenario => {
			const option = document.createElement('option');
			option.value = scenario.filename;
			option.textContent = `${scenario.name} (${scenario.stages} stages, ${scenario.total_duration}s)`;
			option.dataset.description = scenario.description;
			option.dataset.stages = scenario.stages;
			option.dataset.duration = scenario.total_duration;
			scenarioSelect.appendChild(option);
		});
		
		// Update scenario info on selection
		scenarioSelect.addEventListener('change', updateScenarioInfo);
		updateScenarioInfo();
		
	} catch (error) {
		console.error('Failed to load scenarios:', error);
		showStatus('Failed to load network scenarios', 'error');
	}
}

function updateScenarioInfo() {
	const scenarioSelect = document.getElementById('scenario');
	const durationInput = document.getElementById('duration');
	const descriptionDiv = document.getElementById('scenarioDescription');
	
	const selected = scenarioSelect.options[scenarioSelect.selectedIndex];
	if (selected && selected.dataset.description) {
		descriptionDiv.textContent = selected.dataset.description;
		// Auto-fill duration based on scenario
		if (selected.dataset.duration) {
			durationInput.value = selected.dataset.duration;
		}
	} else {
		descriptionDiv.textContent = '';
	}
}

function setupEventListeners() {
	const runBtn = document.getElementById('runBtn');
	const isLiveCheckbox = document.getElementById('isLive');
	const modeRadios = document.querySelectorAll('input[name="simMode"]');
	
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
	
	// Handle mode switch between persona and scenario
	modeRadios.forEach(radio => {
		radio.addEventListener('change', (e) => {
			const personaSection = document.getElementById('personaSelection');
			const scenarioSection = document.getElementById('scenarioSelection');
			
			if (e.target.value === 'persona') {
				personaSection.style.display = 'block';
				scenarioSection.style.display = 'none';
			} else {
				personaSection.style.display = 'none';
				scenarioSection.style.display = 'block';
				// Load scenarios if not already loaded
				if (document.getElementById('scenario').options.length <= 1) {
					loadScenarios();
				}
			}
		});
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
		
		// Determine mode and collect parameters
		const mode = document.querySelector('input[name="simMode"]:checked').value;
		const params = {
			duration: parseFloat(document.getElementById('duration').value),
			is_live: document.getElementById('isLive').checked,
			target_latency: parseFloat(document.getElementById('targetLatency').value),
			max_buffer: parseFloat(document.getElementById('maxBuffer').value),
			seed: parseInt(document.getElementById('seed').value)
		};
		
		if (mode === 'persona') {
			params.persona = document.getElementById('persona').value;
		} else {
			params.scenario = document.getElementById('scenario').value;
			if (!params.scenario) {
				throw new Error('Please select a scenario');
			}
		}
		
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
	// Extract actual segment downloads with valid data (same filter for all charts)
	const downloads = events.filter(e => 
		e.event_type === 'download' && 
		e.download_ms > 0 && 
		e.throughput_bps > 0
	);
	
	// Find maximum timestamp across all events for consistent X-axis
	const maxTime = Math.max(...events.map(e => e.time_s));
	
	// Prepare time series data for buffer chart
	const times = downloads.map(e => e.time_s);
	const bufferLevels = downloads.map(e => e.buffer_s);
	
	// Prepare bandwidth data - each download shows throughput during its period
	const bandwidthData = downloads.map(d => {
		const downloadDuration = d.download_ms / 1000;
		const throughputMbps = d.throughput_bps / 1000000;
		const endTime = d.time_s;
		const startTime = Math.max(0, endTime - downloadDuration);
		
		return {
			startTime: startTime,
			endTime: endTime,
			throughput: throughputMbps
		};
	});
	
	// Prepare timeline data
	const timelineData = downloads.map(d => {
		const downloadDuration = d.download_ms / 1000;
		const endTime = d.time_s;
		const startTime = Math.max(0, endTime - downloadDuration);
		
		return {
			profileIdx: d.profile_idx,
			startTime: startTime,
			endTime: endTime,
			duration: downloadDuration
		};
	});
	
	// Create/update charts with consistent time range
	createBufferChart(times, bufferLevels, maxTime);
	createBandwidthChart(bandwidthData, maxTime);
	createTimelineChart(timelineData, maxTime);
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

function createBufferChart(times, bufferLevels, maxTime) {
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
					display: false
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
					min: 0,
					max: maxTime,
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

function createBandwidthChart(bandwidthData, maxTime) {
	const ctx = document.getElementById('bandwidthChart');
	
	if (bandwidthChart) {
		bandwidthChart.destroy();
	}
	
	// Flatten to time/throughput pairs with proper steps
	const points = [];
	bandwidthData.forEach((d, i) => {
		// If not the first download, add a step down point
		if (i > 0 && d.startTime > points[points.length - 1].x) {
			// Hold previous throughput until this download starts
			points.push({ x: d.startTime, y: points[points.length - 1].y });
		}
		// Add this download's flat top
		points.push({ x: d.startTime, y: d.throughput });
		points.push({ x: d.endTime, y: d.throughput });
	});
	
	bandwidthChart = new Chart(ctx, {
		type: 'line',
		data: {
			datasets: [
				{
					label: 'Measured Throughput',
					data: points,
					borderColor: '#ffc107',
					backgroundColor: 'rgba(255, 193, 7, 0.1)',
					borderWidth: 2,
					stepped: false,
					pointRadius: 0,
					fill: true
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
						title: (context) => {
							const time = context[0].parsed.x.toFixed(3);
							return `Time: ${time}s`;
						},
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
					type: 'linear',
					title: {
						display: true,
						text: 'Time (seconds)'
					},
					min: 0,
					max: maxTime,
					ticks: {
						maxTicksLimit: 10,
						callback: (value) => value.toFixed(1)
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

// Global variable for timeline chart
let timelineChart = null;

function createTimelineChart(timelineData, maxTime) {
	const ctx = document.getElementById('timelineChart');
	
	if (timelineChart) {
		timelineChart.destroy();
	}
	
	// Debug: Log first 10 downloads
	console.log('Timeline data (first 10):');
	timelineData.slice(0, 10).forEach((d, i) => {
		console.log(`  ${i}: Profile ${d.profileIdx} at ${d.startTime.toFixed(2)}s-${d.endTime.toFixed(2)}s`);
	});
	
	// Build dataset for each profile
	const profileBitrates = [235000, 375000, 750000, 1400000, 2800000, 5000000, 8000000];
	const profileNames = [
		'235k',
		'375k',
		'750k',
		'1.4M',
		'2.8M',
		'5.0M',
		'8.0M'
	];
	
	// Single dataset with all downloads - Y position indicates profile
	const data = timelineData.map(d => ({
		x: [d.startTime, d.endTime],
		y: d.profileIdx
	}));
	
	const datasets = [{
		label: 'Segment Downloads',
		data: data,
		backgroundColor: 'rgba(75, 192, 192, 0.7)',
		borderColor: 'rgba(75, 192, 192, 1)',
		borderWidth: 1,
		categoryPercentage: 1.0,
		barPercentage: 0.8
	}];
	
	timelineChart = new Chart(ctx, {
		type: 'bar',
		data: { datasets },
		options: {
			indexAxis: 'y',
			responsive: true,
			maintainAspectRatio: false,
			plugins: {
				title: {
					display: true,
					text: 'Segment Download Timeline (AAMP Autotriage Style)',
					font: { size: 14 }
				},
				legend: {
					display: false
				},
				tooltip: {
					callbacks: {
						title: (context) => {
							const data = context[0].raw;
							const start = data.x[0].toFixed(2);
							const end = data.x[1].toFixed(2);
							const duration = (data.x[1] - data.x[0]).toFixed(3);
							return `Download: ${start}s - ${end}s (${duration}s)`;
						},
						label: (context) => {
							const profileIdx = context.raw.y;
							return profileNames[Math.round(profileIdx)];
						}
					}
				}
			},
			scales: {
				x: {
					type: 'linear',
					position: 'bottom',
					title: {
						display: true,
						text: 'Simulation Time (seconds)'
					},
					min: 0,
					max: maxTime,
					grid: {
						display: true
					}
				},
				y: {
					type: 'linear',
					min: -0.5,
					max: 6.5,
					reverse: false,
					ticks: {
						stepSize: 1,
						callback: function(value) {
							const idx = Math.round(value);
							if (idx >= 0 && idx <= 6) {
								return profileNames[idx];
							}
							return '';
						}
					},
					title: {
						display: true,
						text: 'Video Bitrate'
					},
					grid: {
						display: true,
						drawBorder: true
					}
				}
			}
		}
	});
}
