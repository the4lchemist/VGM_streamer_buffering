import argparse
import os
import subprocess

#number of loops
loops = 1

def parse_args():

    # Create the parser and add arguments
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='filename', help="Filename", type=os.path.abspath)
    #parser.add_argument(dest='loops_no', help="Number of loops")

    # Parse and print the results
    return parser.parse_args()

def is_vgm(filename):

    file = open(filename,"rb")
    magic = file.read(3)
    file.close()
    if (magic == b'Vgm'):
        return True
    else:
        return False

def is_playlist(filename):
    
    if (filename[-3:] == "m3u"):
        return True
    else:
        return False        

def play(filename, script_path):

    # check for magic word "Vgm"
    if (is_vgm(filename)):
        os.system("VGM_streamer " + filename + " " + str(loops))
    else:
        #print("gzip -dc \"" + filename + "\" > \"" + script_path + "temp.vgm\"")
        prog = os.system("gzip -dc \"" + filename + "\" > \"" + script_path + "temp.vgm\"")
        #exit_code = os.system("VGM_streamer temp.vgm " + str(loops))
        #prog = subprocess.Popen("VGM_streamer temp.vgm " + str(loops), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        prog = subprocess.Popen("VGM_streamer temp.vgm " + str(loops), stderr=subprocess.PIPE)
        prog.communicate()
        return prog.returncode
        #print("\"" + script_path + "VGM_streamer temp.vgm\" " + str(loops))
        #os.system("\"" + script_path + "VGM_streamer\" temp.vgm " + str(loops))

def main():

    args = parse_args()

    music_path = '\\'.join(args.filename.split('\\')[0:-1]) + '\\'
    #script_path = os.getcwd() + "\\"
    script_path = '\\'.join(__file__.split('\\')[0:-1]) + '\\'
    os.chdir(script_path)
    
    if (is_playlist(args.filename)):
        music_list = [];
        with open(args.filename) as current:
            for line in current:
                #print("NOW PLAYING: " + music_path + line)
                #play(music_path + line[0:-1], script_path)
                #print()
                music_list.append(music_path + line[0:-1])
        i = 0
        while i < len(music_list):
            print("TRACK no. " + str(i+1))
            exit_code = play(music_list[i], script_path)
            if (exit_code == 1): # previous
                print("\r\n<<\r\n")
                if (i != 0):
                    i = i - 1
            else:
                print("\r\n>>\r\n")
                i = i + 1

    else:
        play(args.filename, script_path)
    
    
if __name__ == '__main__':
    main()