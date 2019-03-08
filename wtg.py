import os
import sys
from subprocess import Popen
from PIL import Image, ImageFilter

core_executable_release = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Release/wtgcore.exe')
core_executable_debug = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Debug/wtgcore.exe')

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
	tmpoutput_cornors = os.path.abspath(".\\output_cornors.img")
	f = open(tmpinput, "wb")
	f.write(data)
	f.close()

	if len(sys.argv) > 2 and sys.argv[2] == '--debug':
		core_executable = core_executable_debug
	else:
		core_executable = core_executable_release
	p = Popen([core_executable, str(resolution), tmpinput, tmpoutput, tmpoutput_cornors])
	returncode = p.wait()
	if returncode != 0:
		raise Exception("wtgcore returns error")

	f = open(tmpoutput, "rb")
	output = f.read()
	f.close()
	output = Image.frombytes("RGB", (resolution, resolution), output)

	f = open(tmpoutput_cornors, "rb")
	output_cornors = f.read()
	f.close()
	output_cornors = Image.frombytes("RGB", (resolution, resolution), output_cornors)

	fn, ext = os.path.splitext(input_path)
	output_path = fn + "_wangtiles" + ext
	output.save(output_path)

	output_path = fn + "_wangtiles_cornors" + ext
	output_cornors.save(output_path)

	os.remove(tmpinput)
	os.remove(tmpoutput)
	os.remove(tmpoutput_cornors)

if __name__ == "__main__":
	main()