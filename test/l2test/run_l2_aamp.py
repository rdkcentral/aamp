#!/usr/bin/env python3
# -*- coding:utf-8 -*-
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
#
import argparse
import os
import sys
import glob
import fnmatch
import subprocess
import json
import stat
import shutil
import webbrowser
from sys import platform
import xml.etree.cElementTree as ET

#declaring global lists and variables
testsuitenumbers=[]
testsuite_excluded=[]
rebuildtestsuite=[]
defbuildtestsuite=[]
testdir=[]
xmlsummary={}
defaultbuild_done=False
skip_build=True
results={"Pass":0 ,"Fail":0}

#setting file paths
l2testdir=os.getcwd()
aampdir=os.path.abspath(os.path.join('..','..'))
print ("Path of Aamp:", aampdir)

if (platform == "darwin"):
    subprocess.run('pip3 install tabulate',shell=True)
else: 
    subprocess.run('sudo pip3 install tabulate',shell=True)

from tabulate import tabulate
#Parsing the command line inputs to get the test suite numbers to execute
#If no inputs then will execute all the test suites without making build
argParser = argparse.ArgumentParser(description='Run L2 AAMP test(s)')
argParser.add_argument("-b","--build",help="build tests before running them",action="store_true")
argParser.add_argument("-dt","--testsuites_exclude",nargs='*',help="skip build and run specify testsuite numbers to skip Ex:-dt 1000 2000")
argParser.add_argument("-t","--testsuites_run",nargs='*',help="skip make build and run the specified testsuite numbers Ex:-t 1000 2000")
argParser.add_argument("-lt",help="list all available tests",action="store_true")
args = argParser.parse_args()
if args.lt:
    testdir=os.listdir(l2testdir)
    searchtestnum='TST_'
    testsuite = [i for i in testdir if i.startswith(searchtestnum)]
    testsuite.sort()
    table=[[i.split("_")[1],i.split("_")[2]] for i in testsuite]

    print(tabulate(table, headers=["Test Number", "Scenario"],tablefmt="rst"))
    exit()

if (args.build):
    print("To make and run testsuites",args.testsuites_run)
    skip_build=False

if (args.testsuites_run):
    print("To run the specified suites",args.testsuites_run)
    testsuitenumbers = args.testsuites_run

else:
    print("Suites for execution")
    for dir in os.listdir(l2testdir):
        if dir.startswith(('TST_')):
            testsuitenumbers.append(dir[4:8])

if (args.testsuites_exclude):
    testsuitenumbers = [a for a in testsuitenumbers if a not in args.testsuites_exclude]
    testdir=os.listdir(l2testdir)
    for num in args.testsuites_exclude:
        searchtestnum='TST_'+num
        testsuite_excluded.append([i for i in testdir if i.startswith(searchtestnum)])
    print( testsuite_excluded ,'dont run this test' )

for num in testsuite_excluded:
    xmlsummary[num[0]] = {"result": "not run"}

print("Test suites chosen for execution: ", testsuitenumbers)

#Checking whether the test suites available for given input
#Classifying the suites requires special build before running tests
testdir=os.listdir(l2testdir)
for num in testsuitenumbers:
    searchtestnum='TST_'+num
    testsuite = [i for i in testdir if i.startswith(searchtestnum)]
    #Checking test suite folder is available
    if (testsuite):
        #Checking whether specific build description available
        check_config_file = os.path.isfile(os.path.join(l2testdir, testsuite[0], "build_config.json"))
        check_postscript_file = os.path.isfile(os.path.join(l2testdir, testsuite[0], "postscript.sh"))
        if(check_config_file == True or check_postscript_file == True):
            rebuildtestsuite.append(testsuite[0]) 
        else:
            defbuildtestsuite.append(testsuite[0]) 
    else:
        print("Wrong test suite number ::", num)

print(defbuildtestsuite)
print(rebuildtestsuite)

#testsuite running
def runtest(suitenumber):
    os.environ['AAMP_HOME'] = aampdir
    os.chdir(os.path.join(l2testdir, suitenumber))
    #Checking any system packages needs to be prepared for execution
    check_file = os.path.isfile("prepare_testenv.sh")
    if(check_file == False):
        print("Prepare test environment not defined")
    else:
        print("Preparing test environment")
        sys.stdout.flush()
        subprocess.run('./prepare_testenv.sh', shell=True)

    #Installing additional python test packages
    sys.stdout.flush()
    subprocess.run('pipreqs .', shell=True)
    requirepackage=os.path.isfile("requirements.txt")
    print("requirepackage")
    if(requirepackage == False):
          print("No additional python packages/modules required")
    else:
          print("Installing additional python packages/modules...")
          sys.stdout.flush()
          subprocess.run('python3 -m pip install -r requirements.txt', shell=True)

    #Checking for prerequisites and executing if required to make test setup ready
    prerequisite=os.path.isfile("prerequisite.sh")
    if(prerequisite == False):
        print("no prerequisites defined")
    else:
        print("Running prerequisites...")
        os.chmod('prerequisite.sh', stat.S_IRWXU)
        sys.stdout.flush()
        subprocess.run('./prerequisite.sh', shell=True)

    print("Start running tests...")
    sys.stdout.flush()
    testresult = subprocess.run("./run_test.py", stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    #Updating failure details
    stack_lines = []
    filename = ""
    linenumber=0
    exceptionline=""
    if testresult.returncode:
         if testresult.stderr:
             stack_lines = testresult.stderr.splitlines()
         else:
             stack_lines = testresult.stdout.splitlines()
         if len(stack_lines) > 0:
             testString = ""
             temp_lines = []
             for testline in reversed(stack_lines):
                 if testline.find("most recent call last") != -1:
                     break
                 temp_lines.append(testline)

             trace_first_line = temp_lines[-3]
             exception_line = temp_lines[0].replace("Exception: ","")
             word_list = trace_first_line.split(" ")
             if len(word_list) > 5:
                print ("File name: ", word_list[3])
                filename=word_list[3].replace(",","").replace("\"","")
                print ("Line number: ", word_list[5])
                linenumber=word_list[5].replace(",","")
                print ("Exception line-------")
                print (exception_line)

    if testresult.returncode == 0:
        xmlsummary[suitenumber] = {"result": "pass"}
        print( suitenumber ,'executed successfully' )
        results["Pass"] +=1
    else:
        xmlsummary[suitenumber] = {"result": "fail","filename":filename,"linenumber":linenumber,"exception_line":exception_line}
        print( suitenumber ,'terminated unsuccessfully with return value ', testresult)
        results["Fail"] +=1

    print(xmlsummary)

#runscript will return False if the script fails and True if it succeeds or the script does not exists
def runscript(scriptname):
    script_is_available=os.path.isfile(scriptname)
    if(script_is_available == True):
        print('Running '+scriptname)
        os.chmod(scriptname, stat.S_IRWXU)
        sys.stdout.flush()
        p=(subprocess.run(('./'+scriptname), shell=True))
        if( p.returncode != 0):
            print('subprocess for ' +scriptname+ ' failed')
            return False
        else:
            print(scriptname+" passed...")
            return True
    else:
        return True

#A specific build is required for one or more of these reasons:
#The build requires extra compiler flags, passed as a parameter to install-aamp.sh
#A script (prescript.sh) needs to be executed before calling install-aamp.sh
#A script (postcript.sh) needs to be executed after calling install-aamp.sh
def dospecificbuild(suitenumber):
    global defaultbuild_done
    print("defaultbuild_done:",defaultbuild_done)
    print("Rebuild require for :",suitenumber)
    #Checking prescript is available and making build accordingly
    os.chdir(os.path.join(l2testdir, suitenumber))
    if(runscript("prescript.sh") == False):
        return False

    #Reading and parsing the build description and making build accordingly
    build_config_defined=os.path.isfile("build_config.json")
    if(build_config_defined == True):
        with open('build_config.json') as user_file:
            file_contents = user_file.read()
            parsed_json = json.loads(file_contents)
            print(parsed_json)
            os.chdir(aampdir)
            cmd_string='bash install-aamp.sh -d $(pwd -P) -n -f '+parsed_json['CXXFLAGS']+' || true'
            print(cmd_string)
            sys.stdout.flush()
            subprocess.run(cmd_string,shell=True)
            #Upon completion of install-aamp.sh with additional buildconfig,
            #reset defaultbuild_done which helps to make default build for the next tests.
            defaultbuild_done=False
    else:
        os.chdir(aampdir)
        #If defaultbuild is avaliable then no need to trigger install-aamp.sh and reuse default build.
        check_prescript_file = os.path.isfile(os.path.join(l2testdir, suitenumber, "prescript.sh"))
        if(check_prescript_file == True):
            subprocess.run('bash install-aamp.sh -d $(pwd -P) -n || true', shell=True)
            defaultbuild_done=False
        elif(defaultbuild_done == False):
            subprocess.run('bash install-aamp.sh -d $(pwd -P) -n || true', shell=True)
            defaultbuild_done=True
        else:
            print("Reusing Default build")

    #Checking postscript is available and making build accordingly
    os.chdir(os.path.join(l2testdir, suitenumber))
    if(runscript("postscript.sh") == False):
        return False

    return True

#looping through the test suites which uses default build
for suitenumber in defbuildtestsuite:
    print(suitenumber)
    #If skip_build is set, install-aamp.sh will be skipped, and just runtest will be performed.
    if((defaultbuild_done == False) and (skip_build == False)):
        os.chdir(aampdir)
        print(os.getcwd())
        sys.stdout.flush()
        subprocess.run('bash install-aamp.sh -d $(pwd -P) -n || true', shell=True)
        defaultbuild_done=True
    runtest(suitenumber)

#looping through the test suites which requires new build
for suitenumber in rebuildtestsuite:
    os.chdir(aampdir)
    #If skip_build is set, dospecificbuild will be skipped, and just runtest will be performed.
    if ((skip_build == True) or (dospecificbuild(suitenumber) == True)):
        runtest(suitenumber)
    else:
        print("Build failed for Testsuite :",suitenumber)

#Updating the test result summary in l2test/result/result_summary.html

if (platform == "darwin"):
    subprocess.run('pip3 install lxml',shell=True)
else: 
    subprocess.run('sudo pip3 install lxml',shell=True)

import lxml.etree as ET
os.chdir(l2testdir)
directory = "result"
if os.path.isdir(directory):
    shutil.rmtree((os.path.join(l2testdir, directory)), ignore_errors=True)
os.makedirs((os.path.join(l2testdir, directory)), exist_ok=True)
shutil.copy('result.xsl', (os.path.join(l2testdir, directory)))
shutil.copy('runresult.dtd', (os.path.join(l2testdir, directory)))
os.chdir(os.path.join(l2testdir, directory))

root = ET.Element("L2_TEST_RUN_REPORT")
doc = ET.SubElement(root, "L2_HEADER",name="header").text="Result File"
resultListing = ET.SubElement(root, "L2_RESULT_LISTING")
reultsuite = ET.SubElement(resultListing, "L2_RUN_SUITE")

for result in xmlsummary:
    print (result)
    print (xmlsummary[result]["result"])
    testrecord = ET.SubElement(reultsuite, "L2_RUN_TEST_RECORD")
    if  xmlsummary[result]["result"] == "pass":
        teststatus = ET.SubElement(testrecord, "L2_RUN_TEST_SUCCESS")
        ET.SubElement(teststatus, "TEST_NAME", name="suitename").text = str(result)
    elif xmlsummary[result]["result"] == "not run":
        teststatus = ET.SubElement(testrecord, "L2_TEST_NOT_RUN")
        ET.SubElement(teststatus, "TEST_NAME", name="suitename").text = str(result)
    else:
        teststatus = ET.SubElement(testrecord, "L2_RUN_TEST_FAILURE")
        ET.SubElement(teststatus, "TEST_NAME", name="suitename").text = str(result)
        ET.SubElement(teststatus, "FILE_NAME", name="suitename").text = str(xmlsummary[result]["filename"])
        ET.SubElement(teststatus, "LINE_NUMBER", name="suitename").text = str(xmlsummary[result]["linenumber"])
        ET.SubElement(teststatus, "Exception", name="suitename").text = str(xmlsummary[result]["exception_line"])

tree = ET.ElementTree(root)
with open("filename.xml","wb") as f:
    f.write('<?xml-stylesheet type = "text/xsl" href = "result.xsl"?> <!DOCTYPE L2_TEST_RUN_REPORT  SYSTEM "runresult.dtd">\n'.encode("UTF-8"))
    tree.write(f)

dom = ET.parse('filename.xml')
xslt = ET.parse('result.xsl')
transform = ET.XSLT(xslt)
newdom = transform(dom)
print(ET.tostring(newdom, pretty_print=True, encoding="unicode"))
infile =(ET.tostring(newdom,encoding="unicode"))
with open("result_summary.html", "w") as f:
    f.write(infile)
    f.close();

print("Results", results)

#Copy and archive the textlogs under result directory
os.chdir(l2testdir)
testdir=os.listdir(l2testdir)
for num in testsuitenumbers:
    searchtestnum='TST_'+num
    testsuite = [i for i in testdir if i.startswith(searchtestnum)]
    #Checking test suite folder is available
    if (testsuite):
        os.chdir(os.path.join(l2testdir,testsuite[0]))
        files = os.listdir(os.path.join(l2testdir,testsuite[0]))
        os.makedirs((os.path.join(l2testdir,directory,testsuite[0])), exist_ok=True)
        for file in files:
            isdir = os.path.isdir(os.path.abspath(file))
            if (file.startswith('testdata') and (isdir == False)):
                shutil.copy(os.path.join(l2testdir,testsuite[0],file), os.path.join(l2testdir, directory,testsuite[0],file))
        os.chdir(os.path.join(l2testdir,directory))
        shutil.make_archive(testsuite[0],'zip', os.path.join(l2testdir, directory), os.path.join(l2testdir, directory,testsuite[0]))
        shutil.rmtree(os.path.join(l2testdir, directory,testsuite[0]))

