// VGM_streamer.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>

#define SAMPLE_FREQ			48000

#define GET_TIME_LAST()		QueryPerformanceCounter(&TimeLast);
#define GET_TIME_NOW()		QueryPerformanceCounter(&TimeNow);
#define	TIME_INTERVAL()		TimeLast.QuadPart - TimeNow.QuadPart
#define GET16(a)            (UINT8) *(a) + ((UINT8) *(a+1) << 8)
#define GET32(a)            (UINT8) *(a) + ((UINT8) *(a+1) << 8) + ((UINT8) *(a+2) << 16) + ((UINT8) *(a+3) << 32);

HANDLE open_serial_port(const char* device, UINT32 baud_rate);
int write_port(HANDLE port, UINT8* buffer, size_t size);
DWORD WINAPI SerialThread(void* data);

HANDLE port;
UINT8* VGM_ptr;
UINT8 size = 0;
HANDLE thread0, thread1, threadPSG, threadPCM;
UINT8 command_PCM[2] = { 0x60, 0x00 };
UINT8 sizeYM0 = 0, sizeYM1 = 0, sizePSG = 0, sizePCM = 0;
UINT8* PCM_ptr;
UINT8 USB_buffer[2][64];
UINT8* USB_buf_ptr = 0x00;
UINT8 write_bytes = 0;
UINT8 i = 0, j = 0;
UINT8 exit_code = 0;
bool pause = FALSE;

int main(int argc, char** argv)
{
	LARGE_INTEGER CPUFreq;
	LARGE_INTEGER TimeNow;
	LARGE_INTEGER TimeLast;
	UINT64 WaitTime = 0;
	UINT64 SingleSample;
    UINT64 Wait735 = 0;
    UINT64 Wait882 = 0;
    UINT64 Wait7x[16];
    UINT64 Wait8x[16];
    UINT32 PCMSize = 0;
    UINT32 PCM_idx = 0;
	FILE *fp;
	char filename[25];
	UINT8* VGM_file;
    UINT8* VGM_init;
    UINT8* PCM_bank = 0x00;
    UINT8 PCM_cmd[3] = { 0x52, 0x2a, 0x00 };
    int result;
    UINT8* GD3_ptr;
    UINT8* VGM_loop;
    UINT8* VGM_rate = 0;
    UINT8 loops = 0;
    UINT8 cmd_bytes = 0;
    UINT8 PCM_byte = 0;
    BOOL PCM_flag = FALSE;
    UINT8 res = 0;

    // file open
	if (argc == 1)
	{
		scanf_s("%s", filename, sizeof(filename));
		fopen_s(&fp, filename, "rb");
        loops = 1;
	}
    else if (argc == 2)
    {
        printf("SYNTAX: VGM_streamer.exe <VGM_file.vgm> <no. of loops>\r\n");
        return -1;
    }
    else
	{
		fopen_s(&fp, argv[1], "rb");
        // read number of loops
        loops = *argv[2] - '0';
	}

    if (!fp)
    {
        printf("File error.\r\n");
        return -1;
    }

    // copy whole VGM in RAM
    fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
    if (fsize < 1)
    {
        printf("VGM error.\r\n");
        return -1;
    }

	fseek(fp, 0, SEEK_SET);  /* same as rewind(f); */

	VGM_file = (UINT8*) malloc(fsize + 1);
    if (!VGM_file)
    {
        printf("Memory error.\r\n");
        return -1;
    }

	fread(VGM_file, fsize, 1, fp);
	fclose(fp);

    // init timing
	QueryPerformanceFrequency(&CPUFreq);
	GET_TIME_NOW();

	TimeLast = TimeNow;
	SingleSample = CPUFreq.QuadPart / SAMPLE_FREQ;
    Wait735 = SingleSample * 735;
    Wait882 = SingleSample * 882;
    for (UINT8 i = 0; i < 16; i++)
    {
        Wait7x[i] = SingleSample * (i + 1);
        Wait8x[i] = SingleSample * i;
    }

    thread0 = CreateThread(NULL, 0, SerialThread, NULL, 0, NULL);
    if (!thread0) {
        return -1;
    }

    // GD3 read
    GD3_ptr = VGM_file + GET32((UINT8*)(VGM_file + 0x14)); // GD3 relative offset
    GD3_ptr += 0x14; // file position
    GD3_ptr += 4; // GD3 string
    GD3_ptr += 4; // version number
    GD3_ptr += 4; // data length
    printf("TRACK    : %ls\r\n", GD3_ptr);
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // advance ptr
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // skip jap name
    printf("GAME     : %ls\r\n", GD3_ptr);
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // advance ptr
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // skip jap name
    printf("SYSTEM   : %ls\r\n", GD3_ptr);
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // advance ptr
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // skip jap name
    printf("AUTHOR(s): %ls\r\n", GD3_ptr);
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // advance ptr
    GD3_ptr += wcslen((wchar_t*)GD3_ptr) * 2 + 2; // skip jap name

    // loops offset
    VGM_loop = VGM_file + GET32((UINT8*)(VGM_file + 0x1C)); // GD3 relative offset
    VGM_loop += 0x1C; // file position
    if ((UINT32) *VGM_loop == 0x00)
    {
        loops = 0;
    }

    //VGM_rate = VGM_file + 0x24; // rate
    //printf("RATE     : %d\r\n", *VGM_rate);

    // open port
    port = open_serial_port("\\\\.\\COM6", 921600);

    // playback loop
	//VGM_ptr = VGM_file + 0x40;
    VGM_ptr = VGM_file + GET32((UINT8*)(VGM_file + 0x34)); // VGM data offset
    if (VGM_ptr - VGM_file == 0x00)
    {
        // For versions prior to 1.50, it should be 0 and the VGM data must start at offset 0x40
        VGM_ptr += 0x40;
    }
    else
    {
        VGM_ptr += 0x34;
    }

    //UINT8 silence_cmd[64];
    //silence_cmd[0] = 0x11;
    //for (UINT8 g = 1; g < 64; g++)
    //{
    //    silence_cmd[g] = 0x00;
    //}
    //res = write_port(port, silence_cmd, 64);

	for (;;)
	{
		// check for pause
        while (pause != FALSE);

        // check for prev/next
        if (exit_code == 1)
        {
            return 1;
        }
        else if (exit_code == 2)
        {
            return 2;
        }

        switch (*VGM_ptr) // VGM commands parsing
        {
        case 0x4F:
            VGM_ptr += 2;
            break;

        case 0x50:
            //GET_TIME_NOW();
            //sizePSG = 2;
            //while (sizePSG != 0);
            cmd_bytes = 2;

            //WaitTime = SingleSample;
            break;

        case 0x52:
        case 0x53:
            //GET_TIME_NOW();
            //sizeYM1 = 3;
            //while (sizeYM1 != 0);
            cmd_bytes = 3;
            //WaitTime = SingleSample;
            break;

        case 0x61:
            //GET_TIME_NOW();
            //WaitTime = SingleSample * (GET16(VGM_ptr + 1));
            cmd_bytes = 3;
            break;
        
        case 0x62:
            cmd_bytes = 1;
            //GET_TIME_NOW();
            //WaitTime = Wait735;
            break;

        case 0x63:
            cmd_bytes = 1;
            //GET_TIME_NOW();
            //WaitTime = Wait882;
            break;

        case 0x67:
            // DATA BLOCK
            PCMSize = GET32(VGM_ptr + 3);
            PCM_bank = (UINT8 *) malloc(PCMSize);
            if (PCM_bank != 0)
            {
                memcpy(PCM_bank, VGM_ptr + 7, PCMSize);
            }
            else
                return 0;
            VGM_ptr += PCMSize + 7;
            break;


        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            //GET_TIME_NOW();
            //WaitTime = Wait7x[(*VGM_ptr++ & 0x0F)];
            cmd_bytes = 1;
            break;
        
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
            //GET_TIME_NOW();
            PCM_byte = PCM_bank[PCM_idx++];
            PCM_flag = TRUE;
            cmd_bytes = 2;
            //sizePCM = 2;
            //WaitTime = Wait8x[(*VGM_ptr++ & 0x0F)];
            break;
        
        case 0xE0:
            PCM_idx = *((unsigned long*)((UINT32*)(VGM_ptr + 1)));
            VGM_ptr += 5;
            break;

        case 0x66:
            if (loops != 0)
            {
                VGM_ptr = VGM_loop;
                loops--;
            }
            else
            {
                return 0;
            }
            break;
        
        case 0x90:
        case 0x91:
            VGM_ptr += 5;
            break;

        case 0x92:
            VGM_ptr += 6;
            break;

        case 0x94:
            VGM_ptr += 2;
            break;

        default:
            UINT16 offset = VGM_ptr - VGM_file;
            printf("Unrecognized command: %02x. Position = 0x%x\r\n", *VGM_ptr, offset);
            return -1;
            break;
        }
		
        if (cmd_bytes != 0)
        {
            if ((j - 1) + cmd_bytes > 63)
            {
                //USB_buf_ptr = USB_buffer[i];
                //write_bytes = j;
                res = write_port(port, USB_buffer[i], j);
                /*printf("Done! %d bytes, buffer %d\r\n", j, i);
                for (UINT8 h = 0; h < j; h++)
                {
                    printf("[%02x]%02x ", h, USB_buffer[i][h]);
                }
                printf("\r\n\r\n");*/
                i = (i + 1) % 2;
                j = 0;
            }

            for (UINT8 h = 0; h < cmd_bytes; h++)
            {
                USB_buffer[i][j + h] = *(VGM_ptr + h);
                if (h == 0 && PCM_flag == TRUE)
                {
                    USB_buffer[i][j + 1] = PCM_byte;
                    cmd_bytes--;
                    j++;
                    PCM_flag = FALSE;
                }
            }
            j += cmd_bytes;
            VGM_ptr += cmd_bytes;
            cmd_bytes = 0;
        }
	}

	return 0;
}

DWORD WINAPI SerialThread(void* data)
{
    for (;;)
    {
        // check for keypress
        if (_kbhit())
        {
            char key_press = _getch();
            switch (key_press)
            {
                // space (pause/play)
            case 32:
                pause = !pause;
                break;

                // arrows (left/right - prev/next)
            case -32:
                key_press = _getch();
                if (key_press == 77)
                {
                    exit_code = 2;
                }
                else if (key_press == 75)
                {
                    exit_code = 1;
                }
                break;

            default:
                break;
            }

        }
    }
}

HANDLE open_serial_port(const char* device, UINT32 baud_rate)
{
    HANDLE port = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (port == INVALID_HANDLE_VALUE)
    {
        return INVALID_HANDLE_VALUE;
    }

    // Flush away any bytes previously read or written.
    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        printf("Failed to flush serial port");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Configure read and write operations to time out after 100 ms.
    // see https://stackoverflow.com/questions/15752272/serial-communication-with-minimal-delay
    COMMTIMEOUTS timeouts = { MAXDWORD,0,0,0,0 };
    /*COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;*/

    success = SetCommTimeouts(port, &timeouts);
    if (!success)
    {
        printf("Failed to set serial timeouts");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    // Set the baud rate and other options.
    DCB state = { 0 };
    state.DCBlength = sizeof(DCB);
    state.BaudRate = baud_rate;
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    success = SetCommState(port, &state);
    if (!success)
    {
        printf("Failed to set serial settings");
        CloseHandle(port);
        return INVALID_HANDLE_VALUE;
    }

    return port;
}

int write_port(HANDLE port, UINT8* buffer, size_t size)
{
    DWORD written;
    BOOL success = WriteFile(port, buffer, size, &written, NULL);
    if (!success)
    {
        printf("Failed to write to port");
        return -1;
    }
    if (written != size)
    {
        printf("Failed to write all bytes to port");
        return -1;
    }
    return 0;
}