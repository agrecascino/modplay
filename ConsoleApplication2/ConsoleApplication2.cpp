// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <string>
#include <assert.h>
#include <bitset>
#include <portaudio.h>
#include <thread>
#include <chrono>
#include <array>
#include <map>
#define swizzle_period(val) (val)
#pragma comment(lib,"portaudio_x86.lib")
enum PeriodType
{
	TYPE_UST_AHRM = 1,
	TYPE_UST_DOUBLED = 2,
	TYPE_UST_HALVED = 4,
	TYPE_PT = 8,
	TYPE_PT_MYSTERY = 16
};

double pow2(double x)
{
	return pow(2.0, x);
}

int getPTPeriod(int note, int tune)
{
	const double NTSC_CLK = 3579545.0;
	const double REF_PERIOD_PT = 856.0;
	const double REF_PERIOD_UST = NTSC_CLK / 523.3 / 8;

	// Convert note and tune into are more helpful representation

	int note2 = note + (tune + 8) / 8;
	int tune2 = (tune + 8) & 7;

	// (1) Calculate the normalized period, i.e. period <= 1.0

	double period = pow2((double)-tune / 8.0 * 1.0 / 12.0);
	period *= pow2((double)-note / 12.0);

	// (2) Select between the PT and UST for the wanted entry

	if (tune2 == 0 && note2 != 0)
	{
		period *= REF_PERIOD_UST;

		// (3) Perform the equivalent of taking resulting entry 
		//     "period/2", and multiply by 2 for periods above 508

		if (note2 < 10)
		{
			period = (double)((int)((period + 1.0) / 2.0) * 2);
		}
	}
	else
	{
		period *= REF_PERIOD_PT;

		// (4) Super efficient manual correction of the evil nine

		period = (tune == -7 && note == 6) ? period - 1 : period;
		period = (tune == -7 && note == 26) ? period - 1 : period;
		period = (tune == -4 && note == 34) ? period - 1 : period;
		period = (tune == 1 && note == 4) ? period - 1 : period;
		period = (tune == 1 && note == 22) ? period + 1 : period;
		period = (tune == 1 && note == 24) ? period + 1 : period;
		period = (tune == 2 && note == 23) ? period + 1 : period;
		period = (tune == 4 && note == 9) ? period + 1 : period;
		period = (tune == 7 && note == 24) ? period + 1 : period;
	}

	return (int)(period + 0.5);
}
class Sample {
public:
	Sample(std::string name, unsigned short length,
		unsigned short repeat, unsigned short repeat_length,
		unsigned char finetune, unsigned char volume) :
		name(name), length(length), repeat(repeat),
		repeat_length(repeat_length), finetune(finetune),
		volume(volume) {std::cout << name << std::endl;}
	std::string name;
	unsigned int length;
	unsigned int repeat;
	unsigned int repeat_length;
	unsigned char finetune;
	unsigned char volume;
};

struct Note {
	unsigned short period;
	unsigned int sample;
	unsigned int effect;
	unsigned char argument;
};

class Pattern {
public:
	Pattern(unsigned char *note_ptr) { 
		for (int i = 0; i < 64; i++) {
			for (int u = 0; u < 4; u++) {
				notes[i][u].effect = (note_ptr + (i * 16) + u * 4)[2] & 0x0F;
				notes[i][u].sample = ((note_ptr + (i * 16) + u * 4)[0] & 0xF0) | (((note_ptr + (i * 16) + u * 4)[2] & 0xF0) >> 4);
				notes[i][u].argument = (note_ptr + (i * 16) + u * 4)[3];
				notes[i][u].period = (((uint16_t)(note_ptr + (i * 16) + u * 4)[0] & 0x0F) << 8) | ((uint16_t)(note_ptr + (i*16) + u*4)[1]);
			}
		}
	}

	Note getNote(unsigned row, unsigned channel) {
		return notes[row][channel];
	}
private:
	Note notes[64][4];
};

class Module {
public:
	Module(std::fstream file) {
		unsigned int notetoperiod[] = { 856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120,113 };
		unsigned int finetune[] = { 0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1 };
		for (int i = 0; i < 36; i++) {
			for (int k = 0; k < 16; k++) {
				insanity[notetoperiod[i]][k] = getPTPeriod(i, finetune[k]);
			}
		}
		Pa_Initialize();
		Pa_OpenDefaultStream(&stream, 0, 1, paInt16, 44100, paFramesPerBufferUnspecified, NULL, NULL);
		Pa_StartStream(stream);
		//file.seekg(file.end);
		//file.seekg(file.beg);
		for (int i = 0; i < 20; i++) {
			name += file.get();
		}
		std::cout << name << std::endl;
		for (int i = 0; i < 31; i++) {
			std::string sname;
			unsigned short slength;
			unsigned char sfinetune;
			unsigned char svolume;
			unsigned short srepeat;
			unsigned short srepeat_length;
			for (int k = 0; k < 22; k++) {
				sname += file.get();
			}
			file.read((char*)&slength, 2);
			slength = _byteswap_ushort(slength);
			slength *= 2;
			sfinetune = file.get();
			svolume = file.get();
			file.read((char*)&srepeat, 2);
			srepeat = _byteswap_ushort(srepeat);
			srepeat *= 2;
			file.read((char*)&srepeat_length, 2);
			srepeat_length = _byteswap_ushort(srepeat_length);
			if (srepeat_length > 1) {
				srepeat_length *= 2;
			}
			samples[i] = new Sample(sname, slength, srepeat, srepeat_length, sfinetune, svolume);
		}
		length = file.get();
		file.get();
		std::cout << (int)length << std::endl;
		int high_order = 0;
		for (int i = 0; i < 128; i++) {
			orders[i] = file.get();
			high_order = orders[i] > high_order ? orders[i] : high_order;
			std::cout << (int)orders[i];
			if (((i % 16) == 0) || i == 127) {
				std::cout << std::endl;
			}
		}
		for (int i = 0; i < 4; i++) {
			type += (char)file.get();
		}
		std::cout << type << std::endl;
		for (int i = 0; i <= high_order; i++) {
			unsigned int notes[64][4];
			memset(notes, 0, 256 * 4);
			
			for (unsigned int k = 0; k < 64; k++) {
				for (unsigned int u = 0; u < 4; u++) {
					if (file.eof()) {
						std::cout << "file end reached, failing." << std::endl;
						exit(-1);
					}
					file.read((char *)&notes[k][u], 4);
				}

			}
			std::cout << high_order << std::endl;
			patterns[i] = new Pattern((unsigned char*)notes);
		}
		unsigned int current = file.tellg();
		file.seekg(0, std::ios_base::end);
		unsigned int end = file.tellg();
		file.seekg(current);
		unsigned int sampledatalength = end - current;
		sampledata = new unsigned char[sampledatalength];
		for (unsigned int i = 0; i < sampledatalength; i++) {
			sampledata[i] = file.get();
		}
	}
	unsigned int getsampleoffset(unsigned int id) {
		unsigned int offset = 0;
		for (int i = 0; i < id; i++) {
			offset += samples[i]->length;
		}
		return offset;
	}
	void play() {
		system("cls");
		for (unsigned char i = 0; i < length; i++) {
			pjump:
			unsigned order = orders[i];
			Pattern *cur_pat = patterns[order];
			for (unsigned row = 0; row < 64; row++) {
				rjump:
				Note notes[4] = { cur_pat->getNote(row, 0), cur_pat->getNote(row, 1), cur_pat->getNote(row, 2), cur_pat->getNote(row, 3) };
				for (Note note : notes) {
					switch (note.effect) {
					default:
						break;
					case 0xF:
						if (note.argument < 32) {
							ticksperrow = note.argument; 
						}
						else {
							bpm = note.argument;
						}
						break;
					}
				}
				for (unsigned channel = 0; channel < 4; channel++) {
					bool dontchangeperiodpls = false;
					switch (notes[channel].effect) {
					case 0x3:
						if (notes[channel].argument > 0) {
							lastportamento[channel] = notes[channel].argument;
						}
						if (notes[channel].period) {
							target[channel] = notes[channel].period;
						}
						std::cout << "ptarget set:" << target[channel] << std::endl;
						assert(target[channel] != 0);
						dontchangeperiodpls = true;
						break;
					}
					if (notes[channel].sample) {
						Sample *s = samples[notes[channel].sample - 1];
						volumes[channel] = s->volume;
						csample[channel] = notes[channel].sample;
					}
					if (notes[channel].period) {
						csamplepoint[channel] = 0.0;
						if (!dontchangeperiodpls) {
							Sample *s = samples[notes[channel].sample - 1];
							if ((notes[channel].sample) == 0) {
								s = samples[csample[channel]];
							}
							cliveperiod[channel] = insanity[notes[channel].period][s->finetune];
						}
						looping[channel] = 0;
					}
					switch (notes[channel].effect) {
					case 0x0:
						if (!notes[channel].argument) {
							break;
						}
						break;
						
					case 0x8:
						break;
					case 0xC:
						volumes[channel] = notes[channel].argument;
						break;
					case 0xD:
						if (notes[channel].argument == 0) {
							i++;
							goto pjump;
						}
						else {
							int x = (notes[channel].argument & 0xF0) >> 4;
							int y = (notes[channel].argument & 0x0F);
							int xy = ((x * 10) + y);
							if (xy > 63) {
								row = xy;
								goto rjump;
							}
							else {
								row = 0;
								goto rjump;
							}
						}
					case 0x9:
						csamplepoint[channel] = notes[channel].argument * 256;
						break;
					case 0xE:
						unsigned int v = (notes[channel].argument & 0xF0) >> 4;
						switch (v) {
						case 1:
							notes[channel].period -= notes[channel].argument & 0x0F;
							if (notes[channel].period < 113) {
								notes[channel].period = 113;
							}
							break;
						case 2:
							notes[channel].period -= notes[channel].argument & 0x0F;
							if (notes[channel].period > 856) {
								notes[channel].period = 856;
							}
							break;
						}
					}
				}
				for (int i = 0; i < ticksperrow; i++) {
					short *sampledataout[4];
					for (unsigned channel = 0; channel < 4; channel++) {
						switch (notes[channel].effect) {
						case 0xE: {
							unsigned int i = (notes[channel].argument & 0xF0) >> 4;
							switch (i) {
							case 9:
								if ((i % (notes[channel].effect & 0x0F)) == 0) {
									csamplepoint[channel] = 0.0;
									cliveperiod[channel] = notes[channel].period;
									looping[channel] = 0;
								}
								break;
							case 0xC:
								if (i == notes[channel].argument) {
									cliveperiod[channel] = 0;
									csamplepoint[channel] = 0.0;
									looping[channel] = 0;
								}
								break;
							}
							break; }
						case 0x1:
							if (i == 0) {
								break;
							}
							cliveperiod[channel] -= notes[channel].argument;
							if (cliveperiod[channel] < 113) {
								cliveperiod[channel] = 113;
							}
							break;
						case 0x2:
							if (i == 0) {
								break;
							}
							cliveperiod[channel] += notes[channel].argument;
							if (cliveperiod[channel] > 856) {
								cliveperiod[channel] = 856;
							}
							break;
						case 0x3: {
							if (i == 0) {
								break;
							}
							if (cliveperiod[channel] == target[channel]) {
								break;
							}
							if (cliveperiod[channel] > target[channel]) {
								cliveperiod[channel] -= (lastportamento[channel]);
								if (cliveperiod[channel] < target[channel]) {
									cliveperiod[channel] = target[channel];
								}
							} else if (cliveperiod[channel] < target[channel]) {
								cliveperiod[channel] += (lastportamento[channel]);
								if (cliveperiod[channel] > target[channel]) {
									cliveperiod[channel] = target[channel];
								}
							}
							break; }
						case 0xA: {
							if (i == 0) {
								break;
							}
							int x = (notes[channel].argument & 0xF0) >> 4;
							int y = notes[channel].argument & 0x0F;
							if ((x > 0) && (y > 0)) {
								break;
							}
							if (x > 0) {
								volumes[channel] += x;
								volumes[channel] = volumes[channel] > 64 ? 64 : volumes[channel];
							}
							if (y > 0) {
								volumes[channel] -= y;
								volumes[channel] = volumes[channel] < 0 ? 0 : volumes[channel];
							}
							break; }
						case 0x5: {
							if (i == 0) {
								break;
							}

							if (cliveperiod[channel] > target[channel]) {
								cliveperiod[channel] -= (lastportamento[channel]);
								if (cliveperiod[channel] < target[channel]) {
									cliveperiod[channel] = target[channel];
								}
							}
							else if (cliveperiod[channel] < target[channel]) {
								cliveperiod[channel] += (lastportamento[channel]);
								if (cliveperiod[channel] > target[channel]) {
									cliveperiod[channel] = target[channel];
								}
							}
							int x = notes[channel].argument & 0xF0;
							int y = notes[channel].argument & 0x0F;
							if ((x > 0) && (y > 0)) {
								break;
							}
							if (x > 0) {
								volumes[channel] += x;
								volumes[channel] = volumes[channel] > 64 ? 64 : volumes[channel];
							}
							if (y > 0) {
								volumes[channel] -= x;
								volumes[channel] = volumes[channel] < 0 ? 0 : volumes[channel];
							}
							break; }
						}
						sampledataout[channel] = new short[samplespertick()];
						if ((csample[channel] == 0) || (cliveperiod[channel] == 0)) {
							memset(sampledataout[channel], 0, samplespertick() * 2);
							continue;
						}
						Sample *s = samples[csample[channel] - 1];
						char *sampleptr = (char*)(sampledata + getsampleoffset(csample[channel] -1));
						for (unsigned int k = 0; k < samplespertick(); k++) {
							
							if (round(csamplepoint[channel]) >= s->length) {
								//std::cout << csamplepoint[channel] << ":" << s->repeat_length << std::endl;
								if (s->repeat_length < 2) {
									//std::cout << "cutting sample on row:" << row << " tick:" << i <<  " sample id:" << csample[channel] << std::endl;
									cliveperiod[channel] = 0;
									csamplepoint[channel] = 0;
									sampledataout[channel][k] = 0;
									for (int j = k; j < samplespertick(); j++) {
										sampledataout[channel][j] = 0;
									}
									break;
								}
								else {
									//std::cout << "looping sample on row:" << row << " tick:" << i << " sample id:" << csample[channel] << " length:" << s->length <<  std::endl;
									csamplepoint[channel] = s->repeat;
									looping[channel] = 1;
								}
							}
							if (looping[channel] == 1) {
								if (csamplepoint[channel] >= (s->repeat + s->repeat_length)) {
									csamplepoint[channel] = s->repeat;
								}
							}
							int a = s->length;
							sampledataout[channel][k] = sampleptr[(unsigned long)csamplepoint[channel]] * (127.0 * (volumes[channel]/64.0));
							double b = (14317456.0/cliveperiod[channel])/ (44100*4);
							csamplepoint[channel] += b;
						}
						std::cout << cliveperiod[channel] << std::endl;
					}
					short *allstream = new short[samplespertick()];
					memset(allstream, 0, samplespertick() * 2);
					for (int u = 0; u < samplespertick(); u++) {
						allstream[u] = ((int)sampledataout[1][u] + (int)sampledataout[0][u] + (int)sampledataout[2][u] + (int)sampledataout[3][u])/4;
					}
					for (int i = 0; i < 4; i++) {
						delete[] sampledataout[i];
					}
					Pa_WriteStream(stream, allstream, samplespertick());
					delete[] allstream;
				}
			}
		}
	}
private:
	unsigned target[4] = { 0,0,0,0 };
	unsigned int notetable[12] = { 1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907 };
	unsigned samplerate = 44100;
	unsigned int bpm = 125;
	double samplespertick() { return (unsigned int)(44100*(((2500.0 / bpm))/1000.0)); }
	unsigned int ticksperrow = 6;
	unsigned lastportamento[4] = { 0,0,0,0 };
	unsigned short cliveperiod[4] = { 0,0,0,0 };
	double csamplepoint[4] = { 0.0,0.0,0.0,0.0 };
	unsigned int csample[4] = { 0,0,0,0 };
	unsigned int rsample[4] = { 0,0,0,0 };
	unsigned int looping[4] = { 0,0,0,0 };
	int volumes[4] = { 64,64,64,64 };
	unsigned char *sampledata;
	unsigned char length;
	unsigned char orders[128];
	std::string name;
	std::string type;
	Pattern *patterns[128];
	Sample *samples[32];
	PaStream *stream;
	std::map<unsigned, std::array<unsigned int, 16>> insanity;
};


int main()
{
	std::cout << sizeof(Note) << std::endl;
	fgetc(stdin);
	Module mod(std::fstream("break.mod", std::ios_base::in | std::ios_base::binary));
	mod.play();
	fgetc(stdin);
    return 0;
}

