# Converter from Geyser program in hex format to binary for CubeSim
# Copyright (c) 2021 Amano laboratory, Keio University.
#     Author: Takuya Kojima

# This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

# CubeSim is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.

# CubeSim is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.


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
