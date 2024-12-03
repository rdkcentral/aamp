import os
import signal
import subprocess
import socket
import psutil
import sys
import xml.etree.ElementTree as ET
from datetime import datetime
import json
import os
import re
from datetime import datetime

# Kill any process running on the port that we want to server our L3 test files on
def kill_serving_port(port):

    try:
        result = subprocess.run(
            ["lsof", "-i", f":{port}"],
            capture_output=True,
            text=True
        )
        if result.stdout:
            lines = result.stdout.strip().split('\n')
            for line in lines[1:]:
                parts = line.split()
                pid = int(parts[1])
                os.kill(pid, signal.SIGKILL)
                print(f"Killed port {port}")
        else:
            print(f"No process found on port {port}")
    except Exception as e:
        print(f"Failed to kill process on port {port}: {e}")

# Serve the test files on an HTTP server
def serve_test_files(port):

    print(f"Serving HTTP on port {port}")
    try:
        subprocess.Popen(["python3", "-m", "http.server", str(port)])
    except Exception as e:
        subprocess.Popen(["python", "-m", "http.server", str(port)])

# Get the external private IP address of the host machine
def get_internal_ip(rdk_device_ip):
    for iface, addrs in psutil.net_if_addrs().items():
        for addr in addrs:
            if addr.family == socket.AF_INET:
                # Check if the IP address is in the same subnet as the device
                iface_ip = addr.address
                if is_same_subnet(iface_ip, rdk_device_ip):
                    return iface_ip
    return None

def is_same_subnet(ip1, ip2, subnet_mask='255.255.255.0'):
    ip1_parts = [int(part) for part in ip1.split('.')]
    ip2_parts = [int(part) for part in ip2.split('.')]
    mask_parts = [int(part) for part in subnet_mask.split('.')]

    for ip1_part, ip2_part, mask_part in zip(ip1_parts, ip2_parts, mask_parts):
        if (ip1_part & mask_part) != (ip2_part & mask_part):
            return False
    return True

# Send a command to a remote host (RDK device) via SSH
def send_ssh_command(host, command, port):

    try:
        result = subprocess.run(
            ["ssh", "-p", port, "root@" + host, command],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"Failed to run command: {result.stderr}")
            sys.exit(1)
        else:
            print(command + " Success")
    except Exception as e:
        print(f"Failed to run command on {host}: {e}")
        sys.exit(1)


# Parse a single L3 log line extracting the useful information
def parse_l3_log_line(line):
    # Example Log Lines:
    #  2024-11-25T10:10:29.229Z com.apps_com.aamp[3705]:  <console.log> L3_LOG: [TST_START] TST_2001_UVE_AampMainAndAdPlayback
    #  2024-11-25T10:11:08.206Z com.apps_com.aamp[3705]:  <console.log> L3_LOG: [TST_STEP] Switch to Ad - Wait: 15(s): PASS
    #  2024-11-25T10:11:11.645Z com.apps_com.aamp[3705]:  <console.log> L3_LOG: [TST_STEP] Switch back to Main - Wait 10(s): FAIL {Unexpected Playback Rate}
    #  2024-11-25T10:11:11.650Z com.apps_com.aamp[3705]:  <console.log> L3_LOG: [TST_END]

    lg_timestamp = 0
    lg_type = 'Unknown'
    test_name = 'Unknown'
    step_name = 'Unknown'
    step_status = "Error"
    fail_reason = "Unknown"

    try:
        # Parse the timestamp into a ISO datetime object
        lg_timestamp = datetime.strptime(line.split(' ')[0], "%Y-%m-%dT%H:%M:%S.%fZ").isoformat()

        # Get the sequence type - keeping the square brackets eg: [TST_STEP]
        lg_type = line.split('L3_LOG: ')[1].split(' ')[0]

        # For each test sequence extract additional information if present
        if lg_type == '[TST_START]':
            test_name = line.split(lg_type + ' ')[1].rsplit()[0]

        elif lg_type == '[TST_STEP]':
            parts = line.split(lg_type + ' ')[1]
            step_parts = re.split(r": PASS|: FAIL", parts)
            step_name = step_parts[0]

            if ': PASS' in parts:
                step_status = "PASS"
            elif ': FAIL' in parts:
                step_status = "FAIL"
                fail_reason = step_parts[1].rstrip().split('}')[0].split('{')[1]
            else:
                step_status = "ERROR"
    except:
        print ("Error parsing line: (", line.rstrip(), ")")

    return {
        "lg_timestamp": lg_timestamp,
        "lg_type": lg_type,
        "test_name": test_name,
        "step_name": step_name,
        "step_status": step_status,
        "fail_reason" : fail_reason
    }


# Parse test log files generated during the test run and create a JUnit report
def parse_log_files_to_xml(log_directory, output_file):
    import xml.etree.ElementTree as ET

    # Create the root element for the JUnit report
    testsuites_element = ET.Element("testsuites")

    # Create a single testsuite element
    timestamp = datetime.now().isoformat()
    testsuite_element = ET.SubElement(testsuites_element, "testsuite",
                                      name="L3_tests",
                                      errors="0",
                                      failures="0",
                                      skipped="0",
                                      tests="0",
                                      time="0",
                                      timestamp="0",
                                      hostname=os.uname().nodename)

    total_tests = 0
    total_failures = 0
    total_duration = 0
    total_starttime = 0
    log_files = os.listdir(log_directory)
    log_files.sort()
    for log_file in log_files:
        if log_file.endswith(".txt"):
            with open(os.path.join(log_directory, log_file), "r") as file:
                suite_name = ""
                testsuite_duration = 0
                lines = file.readlines()
                current_line = 0

                if lines:

                    while current_line < len(lines):

                        l3_log_line = parse_l3_log_line(lines[current_line])

                        if "[TST_START]" in l3_log_line['lg_type']:
                            suite_name = l3_log_line['test_name']
                            testsuite_start_time = l3_log_line['lg_timestamp']
                            if total_starttime == 0:
                                total_starttime = testsuite_start_time

                        elif "[TST_STEP]" in l3_log_line['lg_type']:
                            classname = f"{suite_name}"
                            total_tests += 1
                            step_status = l3_log_line['step_status']
                            current_step = l3_log_line['step_name']
                            step_description = current_step
                            while current_step == step_description:
                                l3_log_line = parse_l3_log_line(lines[current_line])
                                line_status = l3_log_line['step_status']

                                if line_status != "PASS":
                                    step_status = "FAILED"
                                current_line += 1

                                # Log test results don't have TEST_END marker, need to increase the test time
                                if current_line >= len(lines):
                                    break

                                l3_log_line = parse_l3_log_line(lines[current_line])
                                step_description = l3_log_line['step_name']

                                if current_step != step_description:
                                    current_line -= 1
                                    l3_log_line = parse_l3_log_line(lines[current_line])
                                    step_description = l3_log_line['step_name']
                                    break #Emulates a do-while loop (Python doesn't have a do-while loop)

                            if step_status != "PASS":
                                testcase_element = ET.SubElement(testsuite_element, "testcase",
                                                                classname=classname,
                                                                name=step_description)
                                failure_element = ET.SubElement(testcase_element, "failure")
                                failure_element.text = l3_log_line['fail_reason']
                                total_failures += 1
                            else:
                                testcase_element = ET.SubElement(testsuite_element, "testcase",
                                                                classname=classname,
                                                                name=step_description)
                        elif "[TST_END]" in lines[current_line]:
                            testsuite_end_time = l3_log_line['lg_timestamp']
                            testsuite_duration = (datetime.fromisoformat(testsuite_end_time) - datetime.fromisoformat(testsuite_start_time)).total_seconds()
                            total_duration += testsuite_duration
                            break

                        current_line += 1

                else:
                    suite_name = log_file[:len(log_file) - 4]
                    classname = suite_name
                    total_tests += 1
                    total_failures += 1
                    testcase_element = ET.SubElement(testsuite_element, "testcase",
                                                            classname=classname,
                                                            name=classname)
                    failure_element = ET.SubElement(testcase_element, "failure")
                    failure_element.text = "Test failed"
                    total_failures += 1
                    break

    # Update the tests and failures count
    testsuite_element.set("tests", str(total_tests))
    testsuite_element.set("failures", str(total_failures))
    testsuite_element.set("time", str(round(total_duration, 2)))

    if total_starttime == 0:
        total_starttime = timestamp
    testsuite_element.set("timestamp", total_starttime)

    # Create an ElementTree and write it to the output file
    tree = ET.ElementTree(testsuites_element)
    tree.write(output_file, encoding='utf-8', xml_declaration=True)

    if total_failures > 0:
        return False
    return True


# Parse test log files generated during the test run and create a JSON report
def parse_log_files_to_json(log_directory, output_file):

    report = {
        "test_cases_results": {
            "tests": 0,
            "failures": 0,
            "disabled": 0,
            "errors": 0,
            "time": "0s",
            "name": "L3_tests",
            "testsuites": []
        }
    }

    total_tests = 0
    total_failures = 0
    total_duration = 0
    log_files = os.listdir(log_directory)
    log_files.sort()
    for log_file in log_files:
        if log_file.endswith(".txt"):
            testsuite_name = ""
            testsuite_tests = 0
            testsuite_failures = 0
            testsuite_start_time = datetime.now().isoformat()
            testsuite_duration = 0
            testsuite = {
                "name": "",
                "tests": 0,
                "failures": 0,
                "disabled": 0,
                "errors": 0,
                "timestamp": testsuite_start_time,
                "time": "0s",
                "testsuite": []
            }
            with open(os.path.join(log_directory, log_file), "r") as file:
                lines = file.readlines()
                current_line = 0

                if lines:
                    while current_line < len(lines):

                        l3_log_line = parse_l3_log_line(lines[current_line])

                        if "[TST_START]" in l3_log_line['lg_type']:
                            testsuite_name = l3_log_line['test_name']
                            testsuite_start_time = l3_log_line['lg_timestamp']
                            testsuite["name"] = testsuite_name
                            testsuite["testsuite"] = []
                            testsuite["timestamp"] = testsuite_start_time

                        elif "[TST_STEP]" in l3_log_line['lg_type']:
                            total_tests += 1
                            testsuite_tests += 1
                            teststep_start_time = 0
                            step_status = l3_log_line['step_status']
                            current_step = l3_log_line['step_name']
                            step_description = current_step
                            while current_step == step_description:

                                l3_log_line = parse_l3_log_line(lines[current_line])
                                line_status = l3_log_line['step_status']

                                if teststep_start_time == 0:
                                    teststep_start_time = l3_log_line['lg_timestamp']

                                if line_status != "PASS":
                                    step_status = "FAILED"
                                current_line += 1

                                # Log test results don't have TEST_END marker, need to increase the test time
                                if current_line >= len(lines):
                                    break

                                l3_log_line = parse_l3_log_line(lines[current_line])
                                step_description = l3_log_line['step_name']
                                teststep_end_time = l3_log_line['lg_timestamp']

                                if current_step != step_description:
                                    current_line -= 1
                                    l3_log_line = parse_l3_log_line(lines[current_line])
                                    step_description = l3_log_line['step_name']
                                    break #Emulates a do-while loop (Python doesn't have a do-while loop)

                            step_time = (datetime.fromisoformat(teststep_end_time) - datetime.fromisoformat(teststep_start_time)).total_seconds()

                            testcase = {
                                "name": step_description,
                                "status": "RUN",
                                "result": "COMPLETED" if step_status == "PASS" else "FAILED",
                                "timestamp": teststep_start_time,
                                "time": str(round(step_time, 2)) + 's',
                                "classname": testsuite_name
                            }
                            if step_status != "PASS":
                                testcase["failure"] = l3_log_line['fail_reason']
                                total_failures += 1
                                testsuite_failures += 1

                            testsuite["testsuite"].append(testcase)

                        elif "[TST_END]" in l3_log_line['lg_type']:
                            testsuite_end_time = l3_log_line['lg_timestamp']
                            testsuite_duration = (datetime.fromisoformat(testsuite_end_time) - datetime.fromisoformat(testsuite_start_time)).total_seconds()
                            total_duration += testsuite_duration
                            break

                        current_line += 1

                    testsuite["tests"] = testsuite_tests
                    testsuite["failures"] = testsuite_failures
                    testsuite["time"] = str(round(testsuite_duration,2)) + 's'
                    report["test_cases_results"]["testsuites"].append(testsuite)

                # If AAMP fails to launch, add a dummy test case to the report
                else:
                    testName = f"{log_file[:len(log_file) - 4]}"
                    testsuite["name"] = testName
                    testsuite["testsuite"] = [
                        {
                        "name": testName,
                        "status": "FAILED",
                        "result": "AAMP failed to launch -FAILED",
                        "timestamp": testsuite_start_time,
                        "time": "0s",
                        "classname": testName
                    }]
                    testsuite["tests"] = 1
                    testsuite["failures"] = 1
                    report["test_cases_results"]["testsuites"].append(testsuite)
                    total_failures += 1
                    break

    report["test_cases_results"]["tests"] = total_tests
    report["test_cases_results"]["failures"] = total_failures
    report["test_cases_results"]["time"] = str(round(total_duration, 2)) + 's'

    with open(output_file, "w") as json_file:
        json.dump(report, json_file, indent=2)
