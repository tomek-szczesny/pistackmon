// -------------------------------------------------------------------------
// Main application pistackmond
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#include <bitset>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>

#include <fcntl.h>	// open()
#include <unistd.h>	// close(), getopt()
#include <sys/mman.h>   // mmap()
#include <sys/stat.h>   // fchmod()
#include <pthread.h>	// pthread_setschedparam()

#include "gpio.h"       // this is a makefile-generated file (link)

using namespace std::chrono_literals;

//=========================== floatLP CLASS ====================================
// A float with low pass filter built in

class floatLP {
	// Acts like a float variable, but internally
	// filters its contents every time it is updated.
	// Must be updated at the same rate to be effective.

	private:
	float z = 0;			// The variable
	float alpha = 1;		// Smoothing factor

	public:

	// z - initial value
	// tau - time constant [s]
	// freq - sampling frequency
	floatLP(float tau, float freq, float z = 0) {
		this->z = z;
		this->alpha = 1 - (std::exp(-1/(freq * tau)));
	}

	floatLP operator=(float z0) {
		z = (alpha * z0) + ((1 - alpha) * z);
		return *this;
	}

	float f() { return z; }

};

//=================================== CONSTS ===================================

// Data refreshing rate
const float refresh_rate = 10;				// Hz
const std::chrono::microseconds refresh_period =
	std::chrono::microseconds(static_cast<uint32_t>(1000000/refresh_rate));

// Refresh divider - the actual data sampling is performed only once in ref_div,
// although data smoothing is performed on every refresh cycle.
const int ref_div = 5;

// LSB period of a PWM driver.
// Generally should be kept under 20ms/(2^pwm_res) for PWM operation above 50Hz
// for the LED intensity to appear smooth.
// Smaller values are possible at the expense of CPU load and accuracy.
// Values under 10us are not recommended due to thread sleep accuracy.
// For mathematical consistency it should be an integer, but you do you.
const float pwm_lsb_period = 50;			// [us]

// bit depth of PWM LED driver (2-16)
const int pwm_res = 8;

// Layout vectors, containing positions of each LED in LED driver register
const std::vector<int> cpu_layout = {4, 3, 2, 1, 0};
const std::vector<int> ram_layout = {9, 8, 7, 6, 5};
const std::vector<int> temp_layout = {11, 10, 12, 13, 14};
const std::vector<int> user_layout = {15};

// Led intensities adjusted by color 
// Depending on LED make and model, some colors might appear brighter than others
// Consts below may be used to equalize these differences
// Note: these values are set from the makefile
const float led_g = LED_G;
const float led_y = LED_Y;
const float led_r = LED_R;
const float led_b = LED_B;

std::vector<float> led_pwm_multipliers = {led_y, led_g, led_g, led_g, led_g,
						led_r, led_y, led_g, led_g, led_g,
						led_g, led_g, led_y, led_r, led_r, led_b};

// "Gamma" correction for LEDs
// This constant is in use by the led_linear() function below.
// This is not the actual "gamma" correction, but something similar.
const float led_gamma = LED_GAMMA;

//================================== GLOBALS ===================================

// Variables holding measurement results
// Numeric arguments are time constants of low pass filters [in seconds]
// Bigger values will further smooth (and slow down) the response.
floatLP cpu(0.5, refresh_rate);
floatLP ram(0.5, refresh_rate);
floatLP temp(0.5, refresh_rate);
float   user;

// pwm_data contains data for PWM() thread to work on
// Each vector item contains 16 bits to be passed to LED driver.
// Vector items are ordered from the least to most significant bits
// in terms of PWM modulation.
// pwm_data_mutex protects pwm_data
typedef std::vector<std::bitset<16>> pwm_data_datatype;
pwm_data_datatype pwm_data;
std::mutex pwm_data_mutex;

// Precalculated PWM periods
// To be filled by PWM thread
std::vector<std::chrono::microseconds> pwm_periods;

// Bools signaling the respective threads to stop
bool pwm_closing = 0;
bool main_closing = 0;


//============================== LED LINEARIZATION =============================

float led_linear(float in) {
	// A LED linearization algorithm that takes a brightness value (0-1)
	// and returns duty cycle (also 0-1).
	// Uses constant "gamma" declared above

	float a = 1 / (std::exp(led_gamma) - 1);
	return a * (std::exp(led_gamma*in) - 1);
}


//================================ shared-memory ===============================

// Shared-memory region
#define SHR_MEM_PATH "/pistackmond"
void *shrmap;
const off_t SHR_MEM_SIZE = sizeof(float);

int openShrMem(int oflags) {
	int rc;
	int fd;

	// get shared memory file descriptor
	fd = shm_open(SHR_MEM_PATH,oflags,S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("shm_open failed");
		return -1;
	}
	// works only after shm_open, since shm_open respects umask
	fchmod(fd,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

	// increase the size if in create-mode
	if (oflags & O_CREAT) {
		rc = ftruncate(fd,SHR_MEM_SIZE);
		if (rc == -1) {
			perror("ftruncate failed");
			shm_unlink(SHR_MEM_PATH);
			return -1;
		}
	}

	// map shared memory
	if (oflags & O_RDWR) {
		shrmap = mmap(NULL,SHR_MEM_SIZE,PROT_WRITE,MAP_SHARED,fd,0);
	} else {
		shrmap = mmap(NULL,SHR_MEM_SIZE,PROT_READ,MAP_SHARED,fd,0);
	}
	close(fd);
	return 0;
}

// read float from shared memory
inline float readShrMem() {
	float value;
	memcpy(&value,shrmap,sizeof(float));
	return value;
}

// this writes a string (cmdline-arg) as float
inline void writeShrMem(std::string str) {
	float value = std::stof(str);
	if (value > 1) value = 1;
	if (value < 0) value = 0;
	memcpy(shrmap,&value,sizeof(float));
}

void closeShrMem(bool unlink=false) {
	munmap(shrmap,SHR_MEM_SIZE);
	if (unlink) {
		shm_unlink(SHR_MEM_PATH);
	}
}

//================================ getopt ======================================

std::string arg_cmd = "";
std::string arg_user = "";
std::string arg_brightness = "";
bool arg_service = false;

void help(char* pgm) {
	std::cerr << "usage: " << pgm << " -u USER_LED" << std::endl <<
	  "where USER_LED is the brightness of a blue user LED on PiStackMon Lite (range: 0-1)" << std::endl <<
	  "For more advanced options see README.md" << std::endl;
	exit(3);
}

void parseArgs(int argc, char*argv[]) {
	int c;
	opterr = 0;

	while ((c = getopt (argc,argv,"sb:u:h")) != -1) {
		switch (c) {
			case 's':
			arg_service = true;
			break;
		case 'b':
			arg_brightness = std::string(optarg);
			break;
		case 'u':
			arg_user = std::string(optarg);
			break;
		case 'h':
	  		help(argv[0]);
	  		break;
		case '?':
			if (optopt == 'b' || optopt == 'u') {
				std::cerr << "error: option -" << optopt << 
				  " requires an argument" << std::endl;
				help(argv[0]);
			} else {
				std::cerr << "error: unknown option -" << 
				  optopt << std::endl;
				help(argv[0]);
				break;
			}
		default:
			help(argv[0]);
		}
	}
	if (optind < argc) {
		arg_cmd = argv[optind];
	}
}

//=================================== MISC =====================================

std::vector<long int> getIntsFromLine(std::string s) {
	// Returns all integers found in a given string

	std::string temp_s;
	long int temp_i;
	std::stringstream ss;
	std::vector<long int> output;

	ss << s;

	// Reading values from ss one by one, and discarding those that
	// cannot be converted to int.
	while (!ss.eof()) {
		ss >> temp_s;
		if (std::stringstream(temp_s) >> temp_i) {
			output.push_back(temp_i);
		}
	}
	return output;
}

//================================ DATA SOURCES ================================

float fetchTemp() {
	// Returns CPU temperature in degrees C

	const std::string temp_file_name = 
			"/sys/devices/virtual/thermal/thermal_zone0/temp";

	std::ifstream temp_file(temp_file_name);

	if(!temp_file.is_open()) {
          	std::fprintf(stderr,"Unable to open %s.\n",
						temp_file_name.c_str());
		return -1.0;
	}

	float result;
	temp_file >> result;
	temp_file.close();
	result /= 1000;
	return result;	
}

//------------------------------------------------------------------------------

float fetchCpu() {
	// Returns CPU load in %.
	// It's a mean value for all cores,
	// and a mean value since the last call of this method.
	// 
	// /proc/stat contains counters of CPU time dedicated to various tasks.
	// Fourth column is the CPU idle time.
	// This function computes how much time CPUs were *not* idle.
	//
	// Too frequent calling (< 50ms) yields results with poor resolution,
	// due to kernel counters working typically at 100Hz.
	// It is recommended to apply some sort of low-pass filter
	// for more meaningful long-term results.

	static std::vector<long int> last_stat;
	std::vector<long int> current_stat;
	int temp_i = 0;
	std::string temp_s;

repeat:

	std::ifstream cpu_file("/proc/stat");
	if(!cpu_file.is_open()) {
          	std::fprintf(stderr,"Unable to open /proc/stat. Quitting!\n");
		exit(-1);
	}

	getline(cpu_file, temp_s);
	cpu_file.close();
	current_stat = getIntsFromLine(temp_s);

	// On first run, the last_stat vector is empty,
	// so it needs to be populated by repeating the whole thing.
	if (last_stat.empty()) {
		last_stat = current_stat;
		std::this_thread::sleep_for(50ms);	
		goto repeat;
	}

	float result = 0;
	for (auto item : current_stat) temp_i+= item;
	for (auto item : last_stat) temp_i-= item;
	
	// This might happen if called too soon after last call
	// The result would be division by zero - nan.
	if (temp_i == 0) {
		std::this_thread::sleep_for(50ms);	
		goto repeat;
	}
	
	// The fourth column represents CPU idle time
	result = current_stat.at(3) - last_stat.at(3);
	result /= temp_i;
	result = 1 - result;
	result *= 100;

	last_stat = current_stat;
	return result;	
}

//------------------------------------------------------------------------------

float fetchRam() {
	// Returns percentage of used RAM.
	// Used memory estimated just like "free" does:
	// MemTotal - MemFree - Buffers - Cached - SReclaimable
	// See "man free" for details.
	
	int memtotal = 1;

	std::string temp_s;
	float result = 0;


	std::ifstream mem_file("/proc/meminfo");
	if(!mem_file.is_open()) {
          	std::fprintf(stderr,"Unable to open /proc/meminfo. Quitting!\n");
		exit(-1);
	}

	while (!mem_file.eof()) {
		getline(mem_file, temp_s);
		//TODO: Replace with "starts_with" when it gets around)
		if 	(temp_s.find("MemTotal:") == 0)
			memtotal = getIntsFromLine(temp_s).at(0);
		else if	(temp_s.find("MemFree:") == 0)
			result -= getIntsFromLine(temp_s).at(0);
		else if	(temp_s.find("Buffers:") == 0)
			result -= getIntsFromLine(temp_s).at(0);
		else if	(temp_s.find("Cached:") == 0)
			result -= getIntsFromLine(temp_s).at(0);
		else if	(temp_s.find("SReclaimable:") == 0)
			result -= getIntsFromLine(temp_s).at(0);
	}
	mem_file.close();

	result /= memtotal;
	result += 1;
	result *= 100;
	return result;
}

inline float fetchUser() {
	// read user-value from shared-memory
	return readShrMem();
}

// ================================ LED DRIVING ================================

std::vector<float> led_pwms() {
	// converts fetched numbers into float PWM values of each LED
	// Includes LED PWM multipliers (for intensity correcton or whatever)
	// Also includes gamma correction
	
	std::vector<float> output(16);

	for (int i=0; i<5; i++) {
		if (cpu.f() >= 20*(i+1)) output[cpu_layout[i]] = 1;
		else {
			output[cpu_layout[i]] = led_linear((cpu.f() - (20*i))/20);
			break;
		}
	}

	for (int i=0; i<5; i++) {
		if (ram.f() >= 20*(i+1)) output[ram_layout[i]] = 1;
		else {
			output[ram_layout[i]] = led_linear((ram.f() - (20*i))/20);
			break;
		}
	}
	if (temp.f() > 40) {
		for (int i=0; i<5; i++) {
			if (temp.f() >= 10*(i+1)+40) output[temp_layout[i]] = 1;
			else {
				output[temp_layout[i]] = led_linear((temp.f() - (10*i+40))/10);
				break;
			}
		}
	}

	output[user_layout[0]] = led_linear(user);
	for (int i=0; i<16; i++) output[i] *= led_pwm_multipliers[i];

	return output;
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

pwm_data_datatype format_pwms(std::vector<float> pwms) {
	// Converts float pwms of each LED into a vector of bitsets.
	
	std::vector<std::bitset<pwm_res>> pwms_digitized;
	pwm_data_datatype output;
	std::bitset<16> temp_data;

	for (auto f : pwms) {
		pwms_digitized.push_back(static_cast<uint16_t>(f*(std::pow(2,pwm_res)-1)));
	}
	for (int i=0; i < pwm_res; i++) {
		temp_data.reset();
		for (int j = 0; j < 16; j++) {
			temp_data[j] = pwms_digitized[j][i];
		}
		output.push_back(temp_data);
	}
	return output;

}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void sendFrame16(std::bitset<16> f) {
	// Sends data to LED driver chip but does not latch it
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		gpioClear(PIN_CLK);
		if (f[15-i]) {
                	gpioSet(PIN_DATA);
		} else {
                	gpioClear(PIN_DATA);
		}
		__sync_synchronize();
		gpioSet(PIN_CLK);
	}
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

inline void commitFrame() {
	// Applies latch pulse to LED driver chip
	// Thus applying whatever has been previously sent to it
	// Keeping these separated helps synchronise PWM more precisely

	__sync_synchronize();
	gpioSet(PIN_LATCH);
	__sync_synchronize();
	gpioClear(PIN_LATCH);
	__sync_synchronize();
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void gpioInit() {
	gpioInitImpl();     // platform-specific implementation
	sendFrame16(0);     // clear all LEDs
	commitFrame();
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void setLedState(bool state = true) {
	__sync_synchronize();
	if (state) {
		gpioClear(PIN_BLANK);
        } else {
		gpioSet(PIN_BLANK);
        }
	__sync_synchronize();
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -

void gpioDeinit(bool noclear = false) {

	if (!noclear) {
		sendFrame16(0);		// Turn all them LEDs off
		commitFrame();
	}
	gpioDeinitImpl();
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void PWM() {
	// This is a process intended to run as a separate thread
	// for the sake of simplicity. Really.
	// It reads pwm_data and executes whatever is in there.
	// In order to kill this thread gracefully, set "pwm_closing" to 1. 

	auto next_step = std::chrono::high_resolution_clock::now();
	pwm_data_datatype local_pwm_data;

	gpioInit();
	setLedState(true);

	// Compute PWM periods for each bit
	// Every more significant bit gets twice the time of the previous one
	for (int i = 0; i < pwm_res; i++) {
		pwm_periods.push_back(std::chrono::microseconds(
				static_cast<uint32_t>(pwm_lsb_period*std::pow(2,i))));
	}
	
	/*
	// Initial LED test
	for (int i = 0; i < 16; i++) {
		sendFrame16(1 << i);
		commitFrame();
		std::this_thread::sleep_for(150ms);	
	}
	*/

	while (!pwm_closing) {
		if (pwm_data_mutex.try_lock()){
			local_pwm_data = pwm_data;
			pwm_data_mutex.unlock();
		}
		if (local_pwm_data.size() < pwm_res) {	// Avoid segfaults when pwm data not initialized yet
			std::this_thread::sleep_for(100ms);
			continue;
		}
		for (int i = 0; i < pwm_res; i++) {
			next_step += pwm_periods[i];
			// Catch up if this thread is running really late
			// Happens if daemon launches before Pi updates real time clock
			if (next_step < std::chrono::high_resolution_clock::now())
				next_step = std::chrono::high_resolution_clock::now() + pwm_periods[i];
			sendFrame16(local_pwm_data[(i+1)%pwm_res]);
			std::this_thread::sleep_until(next_step);
			commitFrame();
		}
	}

	gpioDeinit();

}

// =================================== MAIN ====================================

void signal_handle(const int s) {
	// Handles a few POSIX signals, asking the process to die gracefully
	
	main_closing = 1;

	if (s){};	// Suppress warning about unused, mandatory parameter
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

int main(int argc, char*argv[]) {

	signal (SIGINT, signal_handle);		// Catches SIGINT (ctrl+c)
	signal (SIGTERM, signal_handle);	// Catches SIGTERM

        parseArgs(argc,argv);
	if (arg_cmd == "allon") {
		gpioInit();
		sendFrame16(-1);	// Dirty "all ones" hack
		commitFrame();
                       setLedState(true);
		exit(0);                // test-mode, no gpioDeInit()
	}
	else if (arg_cmd == "alloff") {
		gpioInit();
		setLedState(true);
		exit(0);                // test-mode, no gpioDeInit()
	}
	else if (arg_user != "") {            // expecting a float 0<=x<=1
		if (openShrMem(O_RDWR)) {
			exit(3);
		}
		writeShrMem(arg_user);
		closeShrMem();
		exit(0);
	}
	else if (!arg_service) {  // require explicit -s flag for service
		std::cerr << "error: illegal invocation" << std::endl;
		help(argv[0]);
	}
	if (arg_brightness != "") {      // expecting a float 0<=x<=1
		float value = std::stof(arg_brightness);
		for (int i=0; i<16; i++) {
			led_pwm_multipliers[i] =
				std::min(1.0f,led_pwm_multipliers[i]*value);
		}
	}

	// create shared memory for user-led
	if (openShrMem(O_RDWR|O_CREAT|O_EXCL)) {
		exit(3);
	}

	// An exact time to gather measurement data and update pwm values
	// refresh_rate determines its frequency.
	auto next_refresh = std::chrono::high_resolution_clock::now();
	
	// Create PWM thread
	std::thread pwm_thread (PWM);

	// Assign Real Time priority to PWM thread
	sched_param sch;
	int policy;
	pthread_getschedparam(pwm_thread.native_handle(), &policy, &sch);
	sch.sched_priority = 99;
	pthread_setschedparam(pwm_thread.native_handle(), SCHED_FIFO, &sch);


	float userCache = 0;
	float cpuCache = 0;
	float ramCache = 0;
	float tempCache = 0;
	int divCounter = 0;

	while (!main_closing) {

		if (++divCounter >= ref_div) {
			cpuCache = fetchCpu();
			ramCache = fetchRam();
                        if (tempCache >= 0.0) {
                          // returns -1 if thermal_zone0 is missing
                          tempCache = fetchTemp();
                        }
			divCounter = 0;
		}	
		// Refresh user LED on every cycle, for faster response
		userCache = fetchUser();

		user = userCache;
		cpu = cpuCache;
		ram = ramCache;
		temp = tempCache < 0.0 ? 0.0:tempCache;

		//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
	
		pwm_data_mutex.lock();
		pwm_data = format_pwms(led_pwms());
		pwm_data_mutex.unlock();

		//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -

		next_refresh += refresh_period;
		// Catch up if this thread is running really late
		// Happens if daemon launches before Pi updates real time clock
		if (next_refresh < std::chrono::high_resolution_clock::now())
			next_refresh = std::chrono::high_resolution_clock::now() + refresh_period;
		std::this_thread::sleep_until(next_refresh);	
	}

	pwm_closing = 1;
	pwm_thread.join();
	closeShrMem(true);
	exit(0);
}

