import argparse
import os
import subprocess
import random
from pathlib import Path

#number of loops
loops = 1
# VGM/VGZ path
pathVGM = r"C:\Users\ingan\Documents\Docs\Personali\Music\VGM"

def is_vgm(filename):

    file = open(filename,"rb")
    magic = file.read(3)
    file.close()
    if (magic == b'Vgm'):
        return True
    else:
        return False 

def play(filename, script_path, output_flag):

    # check for magic word "Vgm"
    if (is_vgm(filename)):
        #os.system("VGM_streamer " + filename + " " + str(loops))
        if output_flag == True:
            prog = subprocess.Popen("VGM_streamer " + filename + " " + str(loops), stderr=subprocess.PIPE)
        else:
            prog = subprocess.Popen("VGM_streamer " + filename + " " + str(loops), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    else:
        prog = os.system("gzip -dc \"" + filename + "\" > \"" + script_path + "temp.vgm\"")
        if output_flag == True:
            prog = subprocess.Popen("VGM_streamer temp.vgm " + str(loops), stderr=subprocess.PIPE)
        else:
            prog = subprocess.Popen("VGM_streamer temp.vgm " + str(loops), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        
    prog.communicate()
    return prog.returncode

def main():

    #args = parse_args()

    #music_path = '\\'.join(args.filename.split('\\')[0:-1]) + '\\'
    #script_path = os.getcwd() + "\\"
    script_path = '\\'.join(__file__.split('\\')[0:-1]) + '\\'
    os.chdir(script_path)
    
    glob_path = Path(pathVGM)
    file_list_VGZ = [str(pp) for pp in glob_path.glob("**/*.vgz")]
    file_list_VGM = [str(pp) for pp in glob_path.glob("**/*.vgm")]

    file_list_TOT = file_list_VGZ + file_list_VGM
    random.shuffle(file_list_TOT)
    
    #if (is_playlist(args.filename)):
    #music_list = [];
    #with open(args.filename) as current:
    #    for line in current:
            #print("NOW PLAYING: " + music_path + line)
            #play(music_path + line[0:-1], script_path)
            #print()
    #        music_list.append(music_path + line[0:-1])
    i = 0
    while i < len(file_list_TOT):
        print("TRACK no. " + str(i+1))
        print(file_list_TOT[i])
        exit_code = play(file_list_TOT[i], script_path, True)
        if (exit_code == 1): # previous
            print("\r\n<<\r\n")
            if (i != 0):
                i = i - 1
        else:
            print("\r\n>>\r\n")
            i = i + 1
        # all channels are silenced thanks to a "silence.vgm" file (exported from Furnace trk)
        exit_code = play("silence.vgm", script_path, False)

    #else:
    #    play(args.filename, script_path)
    
    
if __name__ == '__main__':
    main()