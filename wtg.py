import os
import sys
import math
import time
from subprocess import Popen
from PIL import Image, ImageFilter

core_executable_release = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Release/wtgcore.exe')
core_executable_debug = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Debug/wtgcore.exe')

"""
Usage: python wtg.py input_image [--debug [<debug-tile-index>]]
"""
def main():
	input_path = sys.argv[1]
	input = Image.open(input_path)
	resolution = input.size[0]
	if resolution != input.size[1]:
		raise Exception("input image must be square sized")
	input = input.convert("RGB")
	data = input.tobytes()
	tmpinput = os.path.abspath(".\\input.img")
	tmpoutput = os.path.abspath(".\\output.img")
	tmpoutput_constraints = os.path.abspath(".\\graphcut_constraints.img")
	f = open(tmpinput, "wb")
	f.write(data)
	f.close()

	# invoke wtgcore.exe
	command = ["exe_path(place holder)", "--tiles", str(resolution), tmpinput, tmpoutput, tmpoutput_constraints]
	if len(sys.argv) > 2 and sys.argv[2] == '--debug':
		core_executable = core_executable_debug
		if len(sys.argv) > 3:
			debug_tileindex = int(sys.argv[3])
			command.append(str(debug_tileindex))
	else:
		core_executable = core_executable_release
	command[0] = core_executable
	p = Popen(command)
	returncode = p.wait()
	if returncode != 0:
		raise Exception("wtgcore returns error")

	# read packed corners with its mask
	f = open(tmpoutput, "rb")
	packed_corners = f.read()
	f.close()
	packed_corners = Image.frombytes("RGBA", (resolution, resolution), packed_corners)

	# process mask
	mask = packed_corners.getchannel("A")
	#mask = mask.filter(ImageFilter.GaussianBlur(mask.height / 80.0))
	packed_corners.putalpha(mask)

	# composite output wang tiles
	output = Image.composite(packed_corners.convert("RGB"), input, mask)

	# read graphcut constraints for debugging output
	f = open(tmpoutput_constraints, "rb")
	graphcut_constraints = f.read()
	f.close()
	constraints_resolution = int(math.sqrt(len(graphcut_constraints) / 3))
	graphcut_constraints = Image.frombytes("RGB", (constraints_resolution, constraints_resolution), graphcut_constraints)

	fn, ext = os.path.splitext(input_path)
	output_path = fn + "_wangtiles" + ext
	output.save(output_path)

	output_path = fn + "_wangtiles_corners" + ext
	packed_corners.save(output_path)

	output_path = fn + "_wangtiles_graphcut_constraints" + ext
	graphcut_constraints.save(output_path)

	os.remove(tmpinput)
	os.remove(tmpoutput)
	os.remove(tmpoutput_constraints)



if __name__ == "__main__":
	start_time = time.time()
	main()
	elapsed_time = time.time() - start_time
	print "execution finished in %f seconds" % elapsed_time
