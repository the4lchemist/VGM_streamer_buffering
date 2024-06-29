import argparse
import os
import subprocess
import gzip
import configparser
import serial
import msvcrt
from threading import Thread
import time

# number of loops
loops = 1

# VGM header locations
GD3_h  = 0x14
loop_h = 0x1c
data_h = 0x34

pause = False
playback = False
key = 0

class header:

    def __init__(self,VGM_file):
    
        GD3_length_idx = get32(VGM_file[GD3_h:GD3_h + 4]) + GD3_h + 4 + 4
        GD3_length = get32(VGM_file[GD3_length_idx:GD3_length_idx + 4])
        GD3_idx = get32(VGM_file[GD3_h:GD3_h + 4]) + GD3_h + 4 + 4 + 4
        GD3_array = str(''.join(chr(i) for i in VGM_file[GD3_idx:GD3_idx+GD3_length]))
        GD3_array = GD3_array[0:len(GD3_array):2]
        self.GD3 = GD3_array.split('\0')
        if (get32(VGM_file[loop_h:loop_h + 4]) == 0):
            self.loop_idx = 0
        else:       
            self.loop_idx = get32(VGM_file[loop_h:loop_h + 4]) + loop_h
        self.data_offset_idx = get32(VGM_file[data_h:data_h + 4])
        if (self.data_offset_idx == 0):
            # For versions prior to 1.50, it should be 0 and the VGM data must start at offset 0x40
            self.data_offset_idx = 0x40
        else:
            self.data_offset_idx = self.data_offset_idx + 0x34

class config_file:

    def __init__(self,filename):
    
        config = configparser.ConfigParser()
        config.read(filename)
        self.n_loops = int(config.get('Streamer', 'n_loops'))
        self.port = config.get('Streamer', 'port')

def get32(data):
    
    return int.from_bytes(data, "little")

def parse_args():

    # Create the parser and add arguments
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='filename', help="Filename", type=os.path.abspath)

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

def key_listener():

    global pause
    global key

    while True:
        time.sleep(0.5)        
        if (msvcrt.kbhit()):
            press = ord(msvcrt.getch())
            if (press == 32): # space (pause/resume)
                pause = not(pause)
            if (press == 224): # arrow
                press = ord(msvcrt.getch())
                if (press == 75): # left
                    key = 1
                if (press == 77): # right
                    key = 2
                #break
        if (key != 0):
            break
                
def play(filename, script_path):

    global loops
    global port
    global pause
    global key

    # check for magic word "Vgm"
    if (is_vgm(filename)):
        #os.system("VGM_streamer " + filename + " " + str(loops))
        f = open(filename,'rb')
        VGM_file = f.read()
    else:
        file = gzip.open(filename,'rb')
        try:
            if (file.read(3) != b'Vgm'):
                print("Error: file not recognized.")
                return -1
        except:
            print("Error: not a GZIP file.")
            return -1
        
        file.rewind()
        VGM_file = file.read()
    
    # get header content
    h = header(VGM_file)
    print("TITLE  : " + h.GD3[0])
    print("GAME   : " + h.GD3[2])
    print("SYSTEM : " + h.GD3[4])
    print("AUTHOR : " + h.GD3[6])
    print("RELEASE: " + h.GD3[8])
    
    # loop
    i = h.data_offset_idx
    num_bytes = 0
    j = 0
    buffer = a=[0]*64
    PCM_flag = False
    PCM_idx = 0
    playback = True
    key = 0
    loc_loops = loops
    
    # open serial
    serialPort = serial.Serial(
        port=port, baudrate=9600, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE
    )
    
    th = Thread(target=key_listener)
    th.start()
    
    while (playback == True):
          
        # pause flag from key_listener thread  
        while (pause == True):
            pass
        
        # prev/next track
        if (key == 1):
            playback = False
            exit_code = 1
        if (key == 2):
            playback = False
            exit_code = 2    
        
        # begin parsing
        if (VGM_file[i] == 0x4f):
            i += 2
            
        elif (VGM_file[i] == 0x50):
            num_bytes += 2
                
        elif (VGM_file[i] == 0x52 or VGM_file[i] == 0x53):
            num_bytes += 3
            
        elif (VGM_file[i] == 0x67):
            PCM_len = get32(VGM_file[i+3:i+7])
            PCM_data = VGM_file[i+7:i+7+PCM_len]
            i += 7 + PCM_len
            
        elif (VGM_file[i] == 0x61):
            num_bytes += 3
            
        elif (VGM_file[i] == 0x62):
            num_bytes += 1
            
        elif (VGM_file[i] == 0x63):
            num_bytes += 1    
        
        elif (VGM_file[i] >= 0x70 and VGM_file[i] <= 0x7f):
            num_bytes += 1
        
        elif (VGM_file[i] >= 0x80 and VGM_file[i] <= 0x8f):
            num_bytes += 2
            PCM_flag = True
        
        elif (VGM_file[i] == 0xe0):
            PCM_idx = get32(VGM_file[i+1:i+5])
            i += 5
            
        elif (VGM_file[i] == 0x66):            
            if (h.loop_idx == 0 or loc_loops == 0):
                playback = False
                key = 2
                exit_code = 0
            else:
                loc_loops -= 1
                i = h.loop_idx
                
        # DAC Stream Control Write commands are ignored        
        elif (VGM_file[i] == 0x90):
            i += 5
        
        elif (VGM_file[i] == 0x91):
            i += 5
            
        elif (VGM_file[i] == 0x92):
            i += 6
            
        elif (VGM_file[i] == 0x93):
            i += 11

        elif (VGM_file[i] == 0x94):
            i += 2
            
        elif (VGM_file[i] == 0x95):
            i += 5

        else:
            print("Unrecognized command: " + str(hex(VGM_file[i])) + ", pos. " + str(hex(i)) + ".")
            playback = False
            key = 2
            exit_code = 0
            
        # check for valid command
        if (num_bytes != 0):
            # check for buffer full
            if (j + num_bytes > 63):
                # send buffer
                #print("[" + str(j+1) + "] ")
                #print(' '.join(format(x, '02x') for x in buffer[0:j]))
                #print("")
                serialPort.write(buffer[0:j])
                j = 0

            # fill buffer
            if (PCM_flag == True):
                buffer[j] = VGM_file[i]
                buffer[j+1] = PCM_data[PCM_idx]
                PCM_idx += 1
                PCM_flag = False
                i += 1
            else:
                buffer[j:j+num_bytes] = VGM_file[i:i+num_bytes]
                i += num_bytes
            # increment buffer pointer
            j += num_bytes
                
            num_bytes = 0
    
    th.join()
    serialPort.close()
    return exit_code

def main():

    global loops
    global port

    os.chdir(os.path.dirname(__file__))
    args = parse_args()
    conf = config_file('streamer.ini')
    loops = conf.n_loops
    port = conf.port

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