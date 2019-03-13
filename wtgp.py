import os
import sys
import math
import time
from subprocess import Popen
from PIL import Image, ImageFilter

core_executable = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Release/wtgcore.exe')

"""
Usage: python wtgp.py <resolution> <outputfile>
"""
def main():
	resolution = int(sys.argv[1])
	output_path = sys.argv[2]

	tmpoutput = os.path.abspath(".\\output_p.img")
	command = [core_executable, "--palette", str(resolution), tmpoutput]
	p = Popen(command)
	returncode = p.wait()
	if returncode != 0:
		raise Exception("wtgcore returns error")

	f = open(tmpoutput, "rb")
	output = f.read()
	f.close()
	output = Image.frombytes("RGB", (resolution, resolution), output)
	output.save(output_path)

	os.remove(tmpoutput)


if __name__ == "__main__":
	start_time = time.time()
	main()
	elapsed_time = time.time() - start_time
	print "execution finished in %f seconds" % elapsed_time
