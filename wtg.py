import os
import sys
from subprocess import Popen
from PIL import Image, ImageFilter

core_executable = os.path.join(os.path.dirname(__file__), 'wtgcore/x64/Release/wtgcore.exe')

def main():
	input = sys.argv[1]
	input = Image.open(input)
	resolution = input.size[0]
	if resolution != input.size[1]:
		raise Exception("input image must be square sized")
	input = input.convert("RGB")
	data = input.tobytes()
	tmpinput = os.path.abspath(".\\input.img")
	tmpoutput = os.path.abspath(".\\onput.img")
	f = open(tmpinput, "wb")
	f.write(data)
	f.close()

	p = Popen([core_executable, str(resolution), tmpinput, tmpoutput])
	returncode = p.wait()
	if returncode != 0:
		raise Exception("wtgcore returns error")

	f = open(tmpoutput, "rb")
	output = f.read()
	f.close()
	output = Image.frombytes("RGB", (resolution, resolution), output)
	output.show()

if __name__ == "__main__":
	main()