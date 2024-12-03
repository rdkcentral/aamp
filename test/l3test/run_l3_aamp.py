import argparse
import os
import glob
import subprocess
import shutil
import sys
import time
import re
import l3_utils

# Parse command line arguments - Device IP
parser = argparse.ArgumentParser(description="RDK Device IP")
parser.add_argument("--ip", type=str, required=True, help="IP address of connected RDK device.")
parser.add_argument("--port", type=int, required=True, help="SSH Port of connected RDK device.")
parser.add_argument("-t", "--testsuites_run", nargs='*', help="Specify the test suite numbers to run.")
parser.add_argument("-d", "--dont_end_testsuit_on_failure", help="Dont end a testsuit on first failure", action="store_true")
parser.add_argument("-r", "--rialto", help="To run AAMP on Rialto", action="store_true")
args = parser.parse_args()

# Port to serve the test files on
PORT = 8999

max_test_time = 1800  # Maximum time to run each test in seconds

# Under test RDK device IP address
rdk_device_ip = args.ip
rdk_device_port = str(args.port)
host_ip = l3_utils.get_internal_ip(rdk_device_ip)

passed = True
envVar = "DONT_END_TESTSUITE_ON_FAILURE=0"
aamp_widget ="com.aamp" #Set defalut to non rialto

# Test setup initialization
def initialize():

    global aamp_widget

    global envVar
    # Create a shell script at /tmp/data on the device to set the required environment variables.
    if args.dont_end_testsuit_on_failure:
        envVar = "DONT_END_TESTSUITE_ON_FAILURE=1"

    current_directory = os.getcwd()
    print(current_directory)
    if os.path.isdir(current_directory+"/test_results"):
        shutil.rmtree(current_directory+"/test_results")

    if os.path.isdir(current_directory+"/AAMP_Logs"):
        shutil.rmtree(current_directory+"/AAMP_Logs")
    
    # Downloads and defines the AAMP widget
    if args.rialto:
        aamp_widget = "com.aamp.rialto"

    else:
        aamp_widget ="com.aamp"
    
    # Commands to run before the test starts
    init_command_list = [
                    "cd / && SetPowerState ON",
                    "sleep 10",
                    f"curl -o /tmp/{aamp_widget}.wgt https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/widgets/{aamp_widget}.wgt",
                    "appsservicectl set-test-preference forceallappslaunchable true",
                    "appsservicectl set-test-preference softcatdisableremoveapps true",
                    f"curl -X POST --header \"Content-Type:application/zip\" --data-binary @/tmp/{aamp_widget}.wgt 127.0.0.1:8090/pm/install?sign=auto",
                    f"curl 'http://127.0.0.1:9001/as/apps/action/close?appId={aamp_widget}' -X POST -d '{{}}'"
                    ]
    
    # Server the test files on a HTTP server
    l3_utils.kill_serving_port(PORT) #Kill the port if it is already in use
    l3_utils.serve_test_files(PORT)    

    print("Attempting to connect to the RDK device...")

    # Run the initialization commands
    for i in init_command_list:
        l3_utils.send_ssh_command(rdk_device_ip, i, rdk_device_port)

def run_tests():

    global envVar

    # Get the current working directory
    current_directory = os.getcwd()

    # Create a directory named "test_results" in the current directory
    test_results_directory = os.path.join(current_directory, "test_results")
    os.makedirs(test_results_directory, exist_ok=True)

    # Create a directory for AAMP Logs in the current directory
    aamp_logs_directory = os.path.join(current_directory, "AAMP_Logs")
    os.makedirs(aamp_logs_directory, exist_ok=True)

    all_test_files = [os.path.basename(file) for file in glob.glob(os.path.join(current_directory, '*.html'))]

    # Run specific test suites mentioned in the command line arguments
    if args.testsuites_run:

        testsuites_run = set(args.testsuites_run)
        print("Running specific test suites: ", testsuites_run)

        test_files = []

        for test_suite in testsuites_run:

            for test_file in all_test_files:
                isMatched = re.match(rf"TST_{test_suite}_UVE.*\.html$", test_file)
                if isMatched:
                    test_files.append(test_file)
                    break

            else:
                print("No test files found for given test suite: ", test_suite)
                exit(1)    

    # Run all tests
    else:
        # Get the list of all HTML files in the current directory
        test_files = all_test_files

    # Run each test file
    for test_file in test_files:
        print("Running test file: ", test_file)
        l3_utils.send_ssh_command(rdk_device_ip, f"curl 'http://127.0.0.1:9001/as/apps/action/close?appId={aamp_widget}' -X POST -d '{{}}'", rdk_device_port)
        time.sleep(5)
        l3_utils.send_ssh_command(
            rdk_device_ip, 
            f"curl 'http://127.0.0.1:9001/as/apps/action/launch?appId={aamp_widget}' "
            f"-X POST -d '{{\"url\":\"http://{host_ip}:{PORT}/{test_file}?{envVar}\"}}'", 
            rdk_device_port
        )

        test_log_file = os.path.join(test_results_directory, f"{test_file[:len(test_file) - 5]}.txt")
        aamp_logs_file = os.path.join(aamp_logs_directory, f"AAMP_{test_file[:len(test_file) - 5]}.log")

        with open(aamp_logs_file, "w") as output_file:
            subprocess.Popen(
            ["ssh", "-p", rdk_device_port, f"root@{rdk_device_ip}", f"timeout {int(max_test_time)} tail -f /opt/logs/sky-messages.log | awk '/TST_END/{{print; exit}}; /L3_LOG/ || /AAMP/'"],
            stdout=output_file,
            text=True
            )
        
        with open(test_log_file, "w") as output_file:
            subprocess.run(
            ["ssh", "-p", rdk_device_port, f"root@{rdk_device_ip}", f"timeout {int(max_test_time)} tail -f /opt/logs/sky-messages.log | awk '/TST_END/{{print; exit}}; /L3_LOG/'"],
            stdout=output_file,
            text=True
            )
        time.sleep(15)

def generate_report():

    global passed

    # Get the current working directory
    current_directory = os.getcwd()

    if os.path.isfile(current_directory+"/l3_report.xml"):
        os.remove(current_directory+"/l3_report.xml")

    if os.path.isfile(current_directory+"/l3_report.json"):
        os.remove(current_directory+"/l3_report.json") 

    # Create a directory named "test_results" in the current directory
    test_results_directory = os.path.join(current_directory, "test_results")

    # Parse the log files and create a JUnit XML and JSON report
    passed = l3_utils.parse_log_files_to_xml(test_results_directory, "l3_report.xml")
    l3_utils.parse_log_files_to_json(test_results_directory, "l3_report.json")

def cleanup():

    global passed

    current_directory = os.getcwd()
    shutil.rmtree(current_directory+"/test_results")

    l3_utils.send_ssh_command(rdk_device_ip, f"curl 'http://127.0.0.1:9001/as/apps/action/close?appId={aamp_widget}' -X POST -d '{{}}'", rdk_device_port)
    l3_utils.kill_serving_port(PORT)

    if passed:
        print("All tests passed!")
        sys.exit(0)
    else:    
        print("Some tests failed!")
        sys.exit(1)

if __name__ == "__main__":
    
    # Run setup commands
    initialize()

    # Run the tests
    run_tests()

    # Parse the tests results and create JUnit XML and JSON test report files
    generate_report()
    
    # Cleanup
    cleanup()
