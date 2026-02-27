#!/usr/bin/env python3
"""
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

"""
abrsim_server.py - Web server for ABR Simulator

Provides a simple HTTP server that:
- Serves the web UI for abrsim
- Runs simulations via REST API
- Returns results in JSON format for visualization
"""

import os
import sys
import json
import subprocess
import tempfile
import csv
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs, urlparse
import threading
import time

# Find abrsim binary
ABRSIM_DIR = Path(__file__).parent
ABRSIM_BIN = ABRSIM_DIR / "abrsim"
WEB_DIR = ABRSIM_DIR / "web"
PERSONAS_DIR = ABRSIM_DIR / "personas"
SCENARIOS_DIR = ABRSIM_DIR / "scenarios"

# Whitelist of allowed static files: URL path -> (filesystem Path, MIME type)
# Paths are hardcoded constants so user input never reaches path construction.
_STATIC_FILE_MAP = {
	'/':              (WEB_DIR / 'index.html',    'text/html'),
	'/index.html':    (WEB_DIR / 'index.html',    'text/html'),
	'/app.js':        (WEB_DIR / 'app.js',         'application/javascript'),
	'/chart.min.js':  (WEB_DIR / 'chart.min.js',  'application/javascript'),
	'/style.css':     (WEB_DIR / 'style.css',      'text/css'),
}

class ReuseAddrHTTPServer(HTTPServer):
	"""HTTPServer that allows address reuse"""
	allow_reuse_address = True
	
	def server_bind(self):
		"""Override to set SO_REUSEADDR socket option"""
		import socket
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		super().server_bind()

class ABRSimHandler(BaseHTTPRequestHandler):
	"""HTTP request handler for ABR simulator web interface"""
	
	def log_message(self, format, *args):
		"""Custom log formatting"""
		sys.stderr.write(f"[{self.log_date_time_string()}] {format % args}\n")
	
	def do_GET(self):
		"""Handle GET requests"""
		parsed_path = urlparse(self.path)
		
		# API endpoints
		if parsed_path.path == '/api/personas':
			self.handle_list_personas()
		elif parsed_path.path == '/api/scenarios':
			self.handle_list_scenarios()
		elif parsed_path.path == '/api/profiles':
			self.handle_list_profiles()
		elif parsed_path.path == '/api/status':
			self.handle_status()
		
		# Static files — served from hardcoded whitelist only
		elif parsed_path.path in _STATIC_FILE_MAP:
			file_path, content_type = _STATIC_FILE_MAP[parsed_path.path]
			self.serve_file(file_path, content_type)
		else:
			self.send_error(404, "File not found")
	
	def do_POST(self):
		"""Handle POST requests"""
		parsed_path = urlparse(self.path)
		
		if parsed_path.path == '/api/simulate':
			self.handle_simulate()
		else:
			self.send_error(404, "Endpoint not found")
	def serve_file(self, filepath, content_type):
		"""Serve a static file"""
		try:
			with open(filepath, 'rb') as f:
				content = f.read()
			self.send_response(200)
			self.send_header('Content-type', content_type)
			self.send_header('Content-Length', len(content))
			self.end_headers()
			self.wfile.write(content)
		except FileNotFoundError:
			self.send_error(404, f"File not found")
		except Exception as e:
			self.send_error(500, f"Internal error: {str(e)}")
	
	def handle_list_personas(self):
		"""List available network personas"""
		try:
			personas = []
			if PERSONAS_DIR.exists():
				for persona_file in PERSONAS_DIR.glob('*.json'):
					with open(persona_file) as f:
						data = json.load(f)
						personas.append({
							'filename': persona_file.name,
							'name': persona_file.stem.replace('_', ' ').title(),
							'bandwidth': data.get('mean_thr_mbps', 'Unknown')
						})
			
			self.send_json_response({'personas': personas})
		except Exception as e:
			self.send_error(500, f"Error listing personas: {str(e)}")
	
	def handle_list_scenarios(self):
		"""List available network scenarios"""
		try:
			scenarios = []
			if SCENARIOS_DIR.exists():
				for scenario_file in SCENARIOS_DIR.glob('*.json'):
					with open(scenario_file) as f:
						data = json.load(f)
						# Calculate total duration
						total_duration = sum(stage.get('duration', 0) for stage in data.get('stages', []))
						scenarios.append({
							'filename': scenario_file.name,
							'name': scenario_file.stem.replace('_', ' ').title(),
							'description': data.get('description', ''),
							'stages': len(data.get('stages', [])),
							'total_duration': total_duration
						})
			
			self.send_json_response({'scenarios': scenarios})
		except Exception as e:
			self.send_error(500, f"Error listing scenarios: {str(e)}")
	
	def handle_list_profiles(self):
		"""Load and return available ABR profiles"""
		try:
			profiles_file = ABRSIM_DIR / 'profiles-uhd.json'
			if not profiles_file.exists():
				self.send_error(404, "profiles-uhd.json not found")
				return
			
			with open(profiles_file) as f:
				profiles_data = json.load(f)
			
			self.send_json_response(profiles_data)
		except Exception as e:
			self.send_error(500, f"Error loading profiles: {str(e)}")
	
	def handle_status(self):
		"""Return server status"""
		status = {
			'abrsim_available': ABRSIM_BIN.exists(),
			'version': '1.0',
			'web_dir': str(WEB_DIR),
			'personas_dir': str(PERSONAS_DIR)
		}
		self.send_json_response(status)
	
	def handle_simulate(self):
		"""Run ABR simulation"""
		try:
			# Parse request body
			content_length = int(self.headers['Content-Length'])
			body = self.rfile.read(content_length)
			params = json.loads(body.decode('utf-8'))
			
			# Validate parameters
			if not ABRSIM_BIN.exists():
				self.send_error(500, "abrsim binary not found. Please build it first.")
				return
			
			# Check if using scenario or persona mode
			scenario_file = params.get('scenario', None)
			persona_file = params.get('persona', None)
			
			if not scenario_file and not persona_file:
				self.send_error(400, "Either 'persona' or 'scenario' parameter is required")
				return
			
			if scenario_file and persona_file:
				self.send_error(400, "Cannot specify both 'persona' and 'scenario'")
				return
			
			duration = float(params.get('duration', 3600))
			is_live = params.get('is_live', False)
			target_latency = float(params.get('target_latency', 8.0))
			max_buffer = float(params.get('max_buffer', 20.0))
			seed = int(params.get('seed', 0))
			
			# Create temp output file
			with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as tmp:
				output_file = tmp.name
			
			# Build command
			cmd = [
				str(ABRSIM_BIN),
				'--duration', str(duration),
				'--out', output_file
			]
			
			# Add scenario or persona
			if scenario_file:
				# Handle scenario path
				if scenario_file.startswith('scenarios/'):
					scenario_path = str(ABRSIM_DIR / scenario_file)
				else:
					scenario_path = str(SCENARIOS_DIR / scenario_file)
				cmd.extend(['--scenario', scenario_path])
			else:
				# Handle persona path - client may send just filename or full relative path
				if persona_file.startswith('personas/'):
					persona_path = str(ABRSIM_DIR / persona_file)
				else:
					persona_path = str(PERSONAS_DIR / persona_file)
				cmd.extend(['--persona', persona_path])
			
			if is_live:
				cmd.extend(['--live', '--target-latency', str(target_latency)])
			else:
				cmd.extend(['--max-buffer', str(max_buffer)])
			
			if seed > 0:
				cmd.extend(['--seed', str(seed)])
			
			# Add enabled profiles if specified
			enabled_profiles = params.get('enabled_profiles', [])
			if enabled_profiles:
				for profile_id in enabled_profiles:
					cmd.extend(['--enable-profile', str(profile_id)])
			
			# Run simulation
			print(f"\n{'='*60}")
			print(f"Running: {' '.join(cmd)}")
			print(f"{'='*60}")
			result = subprocess.run(
				cmd,
				capture_output=True,
				text=True,
				timeout=60
			)
			
			# Print stdout/stderr for debugging
			if result.stdout:
				print("--- Simulation Output ---")
				print(result.stdout)
			if result.stderr:
				print("--- Simulation Errors ---")
				print(result.stderr)
			print(f"{'='*60}\n")
			
			if result.returncode != 0:
				self.send_error(500, f"Simulation failed: {result.stderr}")
				return
			
			# Parse CSV output
			events = []
			with open(output_file, 'r') as f:
				reader = csv.DictReader(f)
				for row in reader:
					events.append({
						'time_s': float(row['time_s']),
						'event_type': row['event_type'],
						'profile_idx': int(row['profile_idx']),
						'download_ms': float(row['download_ms']),
						'throughput_bps': float(row['throughput_bps']),
						'buffer_s': float(row['buffer_s']),
						'description': row['description']
					})
			
			# Clean up temp file
			os.unlink(output_file)
			
			# Extract summary statistics from stdout
			summary = self.parse_summary(result.stdout)
			
			response = {
				'success': True,
				'events': events,
				'summary': summary
			}
			
			self.send_json_response(response)
			
		except subprocess.TimeoutExpired:
			self.send_error(500, "Simulation timed out")
		except Exception as e:
			self.send_error(500, f"Simulation error: {str(e)}")
	
	def parse_summary(self, stdout):
		"""Extract summary statistics from abrsim stdout"""
		summary = {}
		lines = stdout.split('\n')
		
		for line in lines:
			if 'Rebuffer events:' in line:
				summary['rebuffer_events'] = int(line.split(':')[1].strip())
			elif 'Total rebuffer time:' in line:
				summary['total_rebuffer_time'] = float(line.split(':')[1].strip().split()[0])
			elif 'Final buffer level:' in line:
				summary['final_buffer'] = float(line.split(':')[1].strip().split()[0])
			elif 'Average latency:' in line:
				summary['avg_latency'] = float(line.split(':')[1].strip().split()[0])
			elif 'Speed-up factor:' in line:
				summary['speedup'] = float(line.split(':')[1].strip().replace('x', ''))
		
		return summary
	
	def send_json_response(self, data):
		"""Send JSON response"""
		response = json.dumps(data).encode('utf-8')
		self.send_response(200)
		self.send_header('Content-type', 'application/json')
		self.send_header('Content-Length', len(response))
		self.send_header('Access-Control-Allow-Origin', '*')
		self.end_headers()
		self.wfile.write(response)

def main():
	"""Start the web server"""
	import sys
	# Allow port to be specified via command line or environment variable
	if len(sys.argv) > 1:
		port = int(sys.argv[1])
	else:
		port = int(os.getenv('PORT', 8080))
	
	# Check if abrsim is built
	if not ABRSIM_BIN.exists():
		print(f"Warning: abrsim binary not found at {ABRSIM_BIN}")
		print("Please build it first with: ./build.sh")
		print("The server will start but simulations will fail.")
		print()
	
	# Create web directory if it doesn't exist
	WEB_DIR.mkdir(exist_ok=True)
	
	server_address = ('', port)
	httpd = ReuseAddrHTTPServer(server_address, ABRSimHandler)
	
	print(f"ABR Simulator Web Server")
	print(f"========================")
	print(f"Server running on http://localhost:{port}")
	print(f"ABR binary: {ABRSIM_BIN}")
	print(f"Web files: {WEB_DIR}")
	print(f"Personas: {PERSONAS_DIR}")
	print()
	print("Press Ctrl+C to stop")
	print()
	
	try:
		httpd.serve_forever()
	except KeyboardInterrupt:
		print("\nShutting down server...")
		httpd.shutdown()

if __name__ == '__main__':
	main()
