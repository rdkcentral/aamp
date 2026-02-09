/*
 * Quick test to verify CSV escaping works correctly
 */
#include <iostream>
#include <string>

static std::string CsvEscape(const std::string& s) {
	if (s.find_first_of(",\"\n\r") == std::string::npos) {
		return s;  // No special chars, return as-is
	}
	std::string escaped = "\"";
	for (char c : s) {
		if (c == '"') escaped += "\"\"";
		else escaped += c;
	}
	escaped += "\"";
	return escaped;
}

int main() {
	// Test cases that would corrupt CSV without escaping
	std::cout << "Testing CSV escaping:\n\n";
	
	std::string url1 = "/playlist.m3u8?token=abc,def&id=123";
	std::cout << "URL with comma: " << CsvEscape(url1) << "\n";
	
	std::string url2 = "/video.ts?title=\"Special Episode\"";
	std::cout << "URL with quotes: " << CsvEscape(url2) << "\n";
	
	std::string url3 = "/api/stream?param=value\n" "malicious";
	std::cout << "URL with newline: " << CsvEscape(url3) << "\n";
	
	std::string normal = "/segment-1234.ts";
	std::cout << "Normal URL (no escaping needed): " << CsvEscape(normal) << "\n";
	
	std::string ip = "192.168.1.1";
	std::cout << "Normal IP: " << CsvEscape(ip) << "\n";
	
	return 0;
}
