#!/usr/bin/python

##########
#
#  Batch prediction
#
##########

import os, string, glob, sys
sys.path.append(os.path.realpath("./"+os.path.dirname(sys.argv[0])))
import fas


## Variables
disallowParallel = False;
dataDir = "";
expDir = "";


## Parse arguments
if len(sys.argv) < 3:
	print("Usage: " + sys.argv[0] + " dataset_path experiment_path [options]");
	print("Options:");
	print("   -disallowParallel");
	exit();
else:
	dataDir = sys.argv[1];
	expDir = sys.argv[2];
	i = 3;
	while i < len(sys.argv):
		if (sys.argv[i] == "-disallowParallel"):
			disallowParallel = True;
		else:
			print("[ERROR] Unknown argument " + sys.argv[i]);
			exit();
		i = i + 1;


# Prepare files
mListTest = [os.path.basename(x) for x in glob.glob(dataDir + "/*.off")];
print "\n".join(mListTest)
binDir = "../../build/Build/bin/"


## Batch jobs
jobIDs = [];
removeFiles = [];
counter = 0;

for mName in mListTest:
	cmd = binDir + "QtViewer" + " "
	cmd += "--flagfile=" + expDir + "/arguments.txt" + " "
	cmd += "--mesh_filename=" + mName + " "
	cmd += "--run_prediction" + " "

	scriptFile = expDir + "/output/" + mName

	if not disallowParallel:
		jobIDs += fas.ScheduleJob(cmd, mName, scriptFile);
		removeFiles += fas.TmpFilesNames(scriptFile);
	else:
		os.system(cmd + " ");

fas.WaitForJobsInArray(jobIDs);