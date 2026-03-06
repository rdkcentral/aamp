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

(function() {
	'use strict';

const API_BASE = '';

// Chart instances
let bitrateChart = null;
let bufferChart = null;
let bandwidthChart = null;

// ── Shared X-axis zoom / pan ──────────────────────────────────────────────────
let viewState = { totalDuration: 0, windowSize: 0, windowStart: 0 };

function applyView() {
	const { totalDuration, windowSize, windowStart } = viewState;
	const xMin = windowSize === 0 ? 0 : windowStart;
	const xMax = windowSize === 0 ? totalDuration : Math.min(windowStart + windowSize, totalDuration);

	for (const chart of [bufferChart, bandwidthChart, timelineChart]) {
		if (!chart) continue;
		chart.options.scales.x.min = xMin;
		chart.options.scales.x.max = xMax;
		chart.update('none');
	}

	const label = document.getElementById('viewRange');
	if (label) {
		const fmt = s => s < 60 ? `${Math.round(s)}s` : `${(s / 60).toFixed(1)}m`;
		label.textContent = `${fmt(xMin)} \u2013 ${fmt(xMax)}`;
	}
}

function setupViewControls(totalDuration) {
	viewState.totalDuration = totalDuration;
	viewState.windowSize = 0;   // Full by default
	viewState.windowStart = 0;

	document.getElementById('viewControls').style.display = 'flex';

	// Reset buttons: activate Full, hide presets wider than the simulation
	document.querySelectorAll('.zoom-btn').forEach(btn => {
		btn.classList.remove('active');
		const w = parseFloat(btn.dataset.window);
		btn.style.display = (w === 0 || w < totalDuration) ? '' : 'none';
		if (w === 0) btn.classList.add('active');
	});

	// Reset pan slider bounds and hide pan row (Full selected)
	const slider = document.getElementById('panSlider');
	slider.min = 0;
	slider.max = totalDuration;
	slider.value = 0;
	slider.step = 1;
	document.getElementById('panRow').style.display = 'none';
}

function initViewControls() {
	// Zoom preset buttons — listeners added once at startup
	document.querySelectorAll('.zoom-btn').forEach(btn => {
		btn.addEventListener('click', () => {
			document.querySelectorAll('.zoom-btn').forEach(b => b.classList.remove('active'));
			btn.classList.add('active');

			const w = parseFloat(btn.dataset.window);
			viewState.windowSize = w;

			const panRow = document.getElementById('panRow');
			const slider = document.getElementById('panSlider');

			if (w === 0) {
				viewState.windowStart = 0;
				panRow.style.display = 'none';
			} else {
				// Clamp current start so window fits within total duration
				const maxStart = Math.max(0, viewState.totalDuration - w);
				viewState.windowStart = Math.min(viewState.windowStart, maxStart);
				slider.max = maxStart;
				slider.step = 1;
				slider.value = viewState.windowStart;
				panRow.style.display = 'flex';
			}
			applyView();
		});
	});

	// Pan slider
	const slider = document.getElementById('panSlider');
	slider.addEventListener('input', () => {
		viewState.windowStart = parseFloat(slider.value);
		applyView();
	});

	// Pan buttons: step by half the window width
	document.getElementById('panLeft').addEventListener('click', () => {
		const step = Math.max(1, viewState.windowSize * 0.5);
		viewState.windowStart = Math.max(0, viewState.windowStart - step);
		slider.value = viewState.windowStart;
		applyView();
	});
	document.getElementById('panRight').addEventListener('click', () => {
		const step = Math.max(1, viewState.windowSize * 0.5);
		viewState.windowStart = Math.min(viewState.totalDuration - viewState.windowSize, viewState.windowStart + step);
		slider.value = viewState.windowStart;
		applyView();
	});
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
	if (typeof Chart === 'undefined') {
		showStatus('Failed to load Chart.js library. Please ensure chart.min.js is present in the web directory.', 'error');
		return;
	}
	initializeUI();
	loadPersonas();
	loadProfiles();
	setupEventListeners();
});

function initializeUI() {
	// Initialize Chart.js with common options
	Chart.defaults.font.family = '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, sans-serif';
	Chart.defaults.color = '#6c757d';
	initViewControls();
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

// Store loaded profiles globally
let availableProfiles = [];

async function loadProfiles() {
	try {
		const response = await fetch(`${API_BASE}/api/profiles`);
		const data = await response.json();
		
		availableProfiles = data.profiles || [];
		
		// Render profile checkboxes
		const container = document.getElementById('profilesContainer');
		container.innerHTML = '';
		
		if (availableProfiles.length === 0) {
			container.innerHTML = '<div style="color: #666;">No profiles available</div>';
			return;
		}
		
		availableProfiles.forEach(profile => {
			const label = document.createElement('label');
			label.style.display = 'block';
			label.style.marginBottom = '4px';
			label.style.fontSize = '0.9em';
			
			const checkbox = document.createElement('input');
			checkbox.type = 'checkbox';
			checkbox.value = profile.id;
			checkbox.checked = profile.enabled !== false; // default to enabled
			checkbox.id = `profile_${profile.id}`;
			
			const bitrateKbps = (profile.bitrate_bps / 1000).toFixed(0);
			const text = document.createTextNode(` ${profile.name} (${bitrateKbps} kbps, ${profile.width}x${profile.height})`);
			
			label.appendChild(checkbox);
			label.appendChild(text);
			container.appendChild(label);
		});
	} catch (error) {
		console.error('Failed to load profiles:', error);
		const container = document.getElementById('profilesContainer');
		container.innerHTML = '<div style="color: #c00;">Failed to load profiles</div>';
	}
}

function getEnabledProfiles() {
	const enabled = [];
	availableProfiles.forEach(profile => {
		const checkbox = document.getElementById(`profile_${profile.id}`);
		if (checkbox && checkbox.checked) {
			enabled.push(profile.id);
		}
	});
	return enabled;
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
			seed: parseInt(document.getElementById('seed').value),
			enabled_profiles: getEnabledProfiles() // Add selected profiles
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
	// Check if we have any events
	if (!events || events.length === 0) {
		console.warn('No events to display');
		showStatus('Simulation completed but produced no events. Check profile selection.', 'warning');
		return;
	}
	
	// Actual completed segment downloads — used for bandwidth and timeline charts.
	const downloads = events.filter(e => 
		e.event_type === 'download' && 
		e.download_ms > 0 && 
		e.throughput_bps > 0
	);
	
	// Check if we have any valid downloads
	if (downloads.length === 0) {
		console.warn('No valid download events to display');
		showStatus('Simulation completed but produced no valid downloads. Check profile selection.', 'warning');
		return;
	}
	
	// Find maximum timestamp across all events for consistent X-axis
	const maxTime = Math.max(...events.map(e => e.time_s));
	
	// Buffer chart: nadir (download_ms>0) + injection (segment_injected) pairs only.
	// segment_start is excluded — the line from the previous injection point to the
	// next nadir already has slope -1 without it, and including it causes duplicate-
	// timestamp artifacts on the category axis.
	// Data uses {x,y} format so the chart X-axis can be type:'linear', which plots
	// two events at the same time_s at the SAME x coordinate, producing a true
	// vertical jump at injection rather than a spread-out step.
	const bufferData = events
		.filter(e => e.event_type === 'download' &&
		             (e.download_ms > 0 || e.description === 'segment_injected'))
		.map(e => ({ x: e.time_s, y: e.buffer_s }));

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
	
	// Prepare timeline data and determine which profiles were actually used
	const usedProfileIndices = new Set();
	const timelineData = downloads.map(d => {
		const downloadDuration = d.download_ms / 1000;
		const endTime = d.time_s;
		const startTime = Math.max(0, endTime - downloadDuration);
		
		usedProfileIndices.add(d.profile_idx);
		
		return {
			profileIdx: d.profile_idx,
			startTime: startTime,
			endTime: endTime,
			duration: downloadDuration
		};
	});
	
	// Build profile info for the profiles that were actually used
	// The enabled profiles in sequential order (0, 1, 2...) after renumbering
	const enabledProfiles = availableProfiles.filter(p => {
		const checkbox = document.getElementById(`profile_${p.id}`);
		return checkbox && checkbox.checked;
	}).sort((a, b) => b.bitrate_bps - a.bitrate_bps); // Sort by bitrate descending (highest first)
	
	// Create profile info array for timeline chart (maps renumbered index to profile data)
	const profilesForTimeline = enabledProfiles.map((p, idx) => ({
		index: idx,
		name: p.name,
		bitrate_bps: p.bitrate_bps,
		used: usedProfileIndices.has(idx)
	}));
	
	// Create/update charts with consistent time range
	createBufferChart(bufferData, maxTime);
	createBandwidthChart(bandwidthData, maxTime);
	createTimelineChart(timelineData, profilesForTimeline, maxTime);
	setupViewControls(maxTime);
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

function createBufferChart(bufferData, maxTime) {
	const ctx = document.getElementById('bufferChart');
	
	if (bufferChart) {
		bufferChart.destroy();
	}
	
	bufferChart = new Chart(ctx, {
		type: 'line',
		data: {
			datasets: [{
				label: 'Buffer Level',
				data: bufferData,  // [{x, y}] — linear scale, no labels array
				borderColor: '#28a745',
				backgroundColor: 'rgba(40, 167, 69, 0.1)',
				borderWidth: 2,
				fill: true,
				pointRadius: 0,
				tension: 0
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
					type: 'linear',  // proportional time axis; same time_s → same x pixel
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
	// Look up bitrate from loaded profiles
	const profile = availableProfiles.find(p => p.id === profileIdx);
	return profile ? profile.bitrate_bps : 0;
}

function showStatus(message, type = 'info') {
	const statusDiv = document.getElementById('status');
	statusDiv.textContent = message;
	statusDiv.className = `status ${type}`;
	statusDiv.style.display = 'block';
}

// Global variable for timeline chart
let timelineChart = null;

function createTimelineChart(timelineData, profilesInfo, maxTime) {
	const ctx = document.getElementById('timelineChart');
	
	if (timelineChart) {
		timelineChart.destroy();
	}
	
	// Debug: Log profile mapping
	console.log('Profile mapping for timeline:');
	profilesInfo.forEach(p => {
		console.log(`  Index ${p.index}: ${p.name} (${(p.bitrate_bps/1000).toFixed(0)} kbps) - ${p.used ? 'USED' : 'unused'}`);
	});
	
	// Build Y-axis labels from enabled profiles (all enabled, not just used)
	const yLabels = profilesInfo.map(p => {
		return p.name; // Use profile name (e.g., "720p")
	});
	
	// Create index to label mapping
	const indexToLabel = {};
	profilesInfo.forEach((p, i) => {
		indexToLabel[p.index] = yLabels[i];
		console.log(`  indexToLabel[${p.index}] = '${yLabels[i]}'`);
	});
	
	// Single dataset with all downloads - Y value is the category label string
	// Store profileIdx for tooltip access
	const data = timelineData.map(d => ({
		x: [d.startTime, d.endTime],
		y: indexToLabel[d.profileIdx], // Use category label instead of numeric index
		profileIdx: d.profileIdx // Store for tooltip
	}));
	
	console.log(`Sample timeline data points (first 5):`);
	data.slice(0, 5).forEach((d, i) => {
		console.log(`  [${i}] profileIdx=${timelineData[i].profileIdx} -> y='${d.y}' | time: ${d.x[0].toFixed(2)}-${d.x[1].toFixed(2)}s`);
	});
	
	const datasets = [{
		label: 'Segment Downloads',
		data: data,
		backgroundColor: 'rgba(75, 192, 192, 0.7)',
		borderColor: 'rgba(0, 0, 0, 0.5)',
		borderWidth: 1.5,
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
							const profileIdx = data.profileIdx; // Use stored numeric index
							const profile = profilesInfo.find(p => p.index === profileIdx);
							const bitrate = profile ? (profile.bitrate_bps / 1000).toFixed(0) : 'Unknown';
							return `Profile: ${bitrate} kbps | ${start}s - ${end}s`;
						},
						label: (context) => {
							const data = context[0].raw;
							const duration = (data.x[1] - data.x[0]).toFixed(3);
							return `Duration: ${duration}s`;
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
						text: 'Time (seconds)'
					},
					min: 0,
					max: maxTime
				},
				y: {
					type: 'category',
					labels: yLabels,
					reverse: false, // Already sorted highest to lowest
					title: {
						display: true,
						text: 'Bitrate Profile'
					},
					offset: true
				}
			}
		}
	});
}
})(); // End IIFE