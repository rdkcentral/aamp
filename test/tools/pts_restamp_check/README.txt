This folder has a script, pts_restamp_check.py, used to parse a log file and produce a .csv file
The csv file contains the extracted information from log
The information is used to check that the pts value is being restampted correctly
The script extracts info from AAMP_INFO log lines. So aamp config must be configured to produce INFO logs

Usage: - python3 <path_to_script>/pts_restamp_check.py <path_to_script>/sky-messages.log > <path_to_script>/output.csv

For example, executing the script from the current ../pts_restamp_check/ folder, the command would be

python3 pts_restamp_check.py sky-messages.log > output.csv

In case of multiple logs:

pts_restamp_check.py sky-messages.log.2 sky-messages.log.1 sky-messages.log > output.csv

