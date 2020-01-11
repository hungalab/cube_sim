import codecs
import sys
import re

args = sys.argv

if len(args) < 3:
    print("args: inputfile outputfile")
    sys.exit()

inputfile = args[1]
outputfile = args[2]

bary = bytearray()

with open(inputfile, 'r') as fin, open(outputfile, 'wb') as fout:
    lines = fin.readlines()
    for l in lines:
        l = re.sub(r"//.*", "", l)
        s = re.sub(r'[\r\n\t ]|0x', '', l)
        fout.write(codecs.getdecoder("hex_codec")(s)[0][::-1])

    # fill nop for reset pipeline
    for i in range(100):
        fout.write(codecs.getdecoder("hex_codec")("0000")[0][::-1])

#    fout.write(bary)
