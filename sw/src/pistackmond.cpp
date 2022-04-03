#include <bitset>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/mman.h>	// mmap()
#include <fcntl.h>	// open()
#include <unistd.h>	// close()
#include <pthread.h>	// pthread_setschedparam()

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
const float pwm_lsb_period = 250;			// [us]

// bit depth of PWM LED driver (2-16)
const int pwm_res = 6;

// Layout vectors, containing positions of each LED in LED driver register
const std::vector<int> cpu_layout = {4, 3, 2, 1, 0};
const std::vector<int> ram_layout = {9, 8, 7, 6, 5};
const std::vector<int> temp_layout = {11, 10, 12, 13, 14};

// Led intensities adjusted by color 
// Depending on LED make and model, some colors might appear brighter than others
// Consts below may be used to equalize these differences
const float led_g = 1;
const float led_y = 1;
const float led_r = 1;
const std::vector<float> led_pwm_multipliers = {led_y, led_g, led_g, led_g, led_g,
						led_r, led_y, led_g, led_g, led_g,
						led_g, led_g, led_y, led_r, led_r};

// Gamma correction for LEDs
// It is not the proper way to linearize the LED response, but close enough
const float led_gamma = 2.8;

// Other consts
#ifdef defined(ODROID_M1)
const uint g3 = 0xFE760000 - 0xFDD60000; // Extra offset for reaching GPIO bank 3
#endif

//================================== GLOBALS ===================================

// Variables holding measurement results
// Numeric arguments are time constants of low pass filters [in seconds]
// Bigger values will further smooth (and slow down) the response.
floatLP cpu(0.5, refresh_rate);
floatLP ram(0.5, refresh_rate);
floatLP temp(0.5, refresh_rate);

// pwm_data contains data for PWM() thread to work on
// Each vector item contains 16 bits to be passed to LED driver.
// Vector items are ordered from the least to most significant bits
// in terms of PWM modulation.
// pwm_data_mutex protects pwm_data
typedef std::vector<std::bitset<16>> pwm_data_datatype;
pwm_data_datatype pwm_data;
std::mutex pwm_data_mutex;

// Map of a part of memory that provides access to GPIO registers
volatile uint32_t * gpiomap;

// Precalculated PWM periods
// To be filled by PWM thread
std::vector<std::chrono::microseconds> pwm_periods;

// Bools signaling the respective threads to stop
bool pwm_closing = 0;
bool main_closing = 0;

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

// A special function for accessing RockChip's GPIOs - "it's complicated"...
// It requires simultaneous write to two bits for whatever reason.
// Perhaps future release of full datasheet would explain this.
#ifdef defined(ODROID_M1)
void rk_gpio(int offset, int bit, bool val) {
	uint32_t buf = *(gpiomap + offset);
	buf |= (1 << (pin + 16));		// <- this thing - what is that?
	if (val) buf |=  (1 << bit);
	else     buf &= ~(1 << bit);
	*(gpiomap + offset) = buf;
}
#endif

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

// ================================ LED DRIVING ================================

std::vector<float> led_pwms() {
	// converts fetched numbers into float PWM values of each LED
	// Includes LED PWM multipliers (for intensity correcton or whatever)
	// Also includes gamma correction
	
	std::vector<float> output(16);

	for (int i=0; i<5; i++) {
		if (cpu.f() >= 20*(i+1)) output[cpu_layout[i]] = 1;
		else {
			output[cpu_layout[i]] = std::pow((cpu.f() - (20*i))/20, led_gamma);
			break;
		}
	}

	for (int i=0; i<5; i++) {
		if (ram.f() >= 20*(i+1)) output[ram_layout[i]] = 1;
		else {
			output[ram_layout[i]] = std::pow((ram.f() - (20*i))/20, led_gamma);
			break;
		}
	}
	if (temp.f() > 40) {
		for (int i=0; i<5; i++) {
			if (temp.f() >= 10*(i+1)+40) output[temp_layout[i]] = 1;
			else {
				output[temp_layout[i]] = std::pow((temp.f() - (10*i+40))/10, led_gamma);
				break;
			}
		}
	}

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

#if defined(RASPBERRY_PI3) || defined(RASPBERRY_PI4)
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		*(gpiomap+10) = (1 << 27);		// Set pin 27 (clk) low
		*(gpiomap+(f[15-i]?7:10)) = (1 << 17);	// Set pin 17 (data) to value f[15-i])
		__sync_synchronize();
		*(gpiomap+7) = (1 << 27);		// Set pin 27 (clk) high
	}
#elif defined (ODROID_N2)
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		*(gpiomap+0x117) &= ~(1 << 4);		// Set pin X.4 (clk) low
		if(f[15-i]) {				// Set pin X.3 (data) high
			*(gpiomap+0x117) |= (1 << 3);
		}
		else {					// Set pin X.3 (data) low
			*(gpiomap+0x117) &= ~(1 << 3);
		}
		__sync_synchronize();
		*(gpiomap+0x117) |= (1 << 4);		// Set pin X.4 (clk) high
	}
#elif defined (ODROID_C1)
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		*(gpiomap+0x0D) &= ~(1 << (116-97));    // Set pin X.19 (clk) low
		if(f[15-i]) {				// Set pin Y.8 (data) high
                  *(gpiomap+0x10) |= (1 << (88-80));
		}
		else {					// Set pin Y.8 (data) low
                  *(gpiomap+0x10) &= ~(1 << (88-80));
		}
		__sync_synchronize();
		*(gpiomap+0x0D) |= (1 << (116-97));	// Set pin X.19 (clk) high
	}
#elif defined (ODROID_C2)
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		*(gpiomap+0x119) &= ~(1 << 11);    	// Set pin X.11 (clk) low
		if(f[15-i]) {				// Set pin X.19 (data) high
                  *(gpiomap+0x119) |= (1 << 19);
		}
		else {					// Set pin X.19 (data) low
                  *(gpiomap+0x119) &= ~(1 << 19);
		}
		__sync_synchronize();
		*(gpiomap+0x119) |= (1 << 11);		// Set pin X.11 (clk) high
	}
#elif defined (ODROID_M1)
	for(int i = 0; i < 16; i++) {
		__sync_synchronize();
		rk_gpio(     0x01,  1, 0);	// Set pin 0C.1 (clk ) low
		if(f[15-i]) {				// Set pin X.19 (data) high
			rk_gpio(0x01,  0, 1);	// Set pin 0C.0 (data) high
		}
		else {					// Set pin X.19 (data) low
			rk_gpio(0x01,  0, 0);	// Set pin 0C.0 (data) low
		}
		__sync_synchronize();
		rk_gpio(     0x01,  1, 1);	// Set pin 0C.1 (clk ) high
	}
#endif

}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

inline void commitFrame() {
	// Applies latch pulse to LED driver chip
	// Thus applying whatever has been previously sent to it
	// Keeping these separated helps synchronise PWM more precisely

	__sync_synchronize();
#if defined(RASPBERRY_PI3) || defined(RASPBERRY_PI4)
	*(gpiomap+7) = (1 << 22);			// Set pin 22 (latch) high
	__sync_synchronize();
	*(gpiomap+10) = (1 << 22);			// Set pin 22 (latch) low
#elif defined (ODROID_N2)
	*(gpiomap+0x117) |= (1 << 7);			// Set pin X.7 (latch) high
	__sync_synchronize();
	*(gpiomap+0x117) &= ~(1 << 7);			// Set pin X.7 (latch) low
#elif defined (ODROID_C1)
	*(gpiomap+0x0D) |= (1 << 18);		// Set pin X.18 (latch) high
	__sync_synchronize();
	*(gpiomap+0x0D) &= ~(1 << 18);		// Set pin X.18 (latch) low
#elif defined (ODROID_C2)
	*(gpiomap+0x119) |= (1 << 9);		// Set pin X.9 (latch) high
	__sync_synchronize();
	*(gpiomap+0x119) &= ~(1 << 9);		// Set pin X.9 (latch) low
#elif defined (ODROID_M1)
	rk_gpio( g3      , 10, 1);		// Set pin 3B.2 (latch) high
	__sync_synchronize();
	rk_gpio( g3      , 10, 0);		// Set pin 3B.2 (latch) low
#endif
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void gpioInit() {

	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
#ifdef RASPBERRY_PI3
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0x3F200000);
#elif defined(RASPBERRY_PI4)
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFE200000);
#elif defined(ODROID_N2)
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFF634000);
#elif defined(ODROID_C1)
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xC1108000);
#elif defined(ODROID_C2)
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xC8834000);
#elif defined(ODROID_M1)
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFDD60000);
#endif
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	// TODO: The following code will segfault if not run as root.
	// Let the user know what is wrong!

#if defined(RASPBERRY_PI3) || defined(RASPBERRY_PI4)
	// Set pins as inputs (reset 3-bit pin mode to 000)
	*(gpiomap+1) &= ~(7<<(7*3));		// Pin 17
	*(gpiomap+2) &= ~(7<<(2*3));		// Pin 22
	*(gpiomap+2) &= ~(7<<(7*3));		// Pin 27

	// Set pins as outputs (sets 3-bit pin mode to 001)
	*(gpiomap+1) |=  (1<<(7*3));		// Pin 17
	*(gpiomap+2) |=  (1<<(2*3));		// Pin 22
	*(gpiomap+2) |=  (1<<(7*3));		// Pin 27
#elif defined(ODROID_N2)
	// Set pins as outputs 
	*(gpiomap+0x116) &= ~(1<<3);		// Pin X.3
	*(gpiomap+0x116) &= ~(1<<4);		// Pin X.4
	*(gpiomap+0x116) &= ~(1<<7);		// Pin X.7

	// Do something with mux (eh? whatever that means)
	*(gpiomap+0x1B3) &=  (0xF<<3*4);	// Pin X.3
	*(gpiomap+0x1B3) &=  (0xF<<4*4);	// Pin X.4
	*(gpiomap+0x1B3) &=  (0xF<<7*4);	// Pin X.7
#elif defined(ODROID_C1)
	// Set pins as outputs
	*(gpiomap+0x0F) &= ~(1<<8);		// Pin Y.8
	*(gpiomap+0x0C) &= ~(1<<19);		// Pin X.19
	*(gpiomap+0x0C) &= ~(1<<18);		// Pin X.18
#elif defined(ODROID_C2)
	// Set pins as outputs
	*(gpiomap+0x118) &= ~(1<<19);		// Pin X.19
	*(gpiomap+0x118) &= ~(1<<11);		// Pin X.11
	*(gpiomap+0x118) &= ~(1<<9);		// Pin X.9
#elif defined(ODROID_M1)
	// Set pins as outputs
	rk_gpio(     0x02 + 0x01,  0, 1);	// Pin 0C.0
	rk_gpio(     0x02 + 0x01,  1, 1);	// Pin 0C.1
	rk_gpio(g3 + 0x02       , 10, 1);	// Pin 3B.2
//	rk_gpio(g3 + 0x02 + 0x01,  9, 1);	// Pin 3D.1
#endif
	
}

//  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   

void gpioDeinit(bool noclear = false) {

	if (!noclear) {
		sendFrame16(0);		// Turn all them LEDs off
		commitFrame();
	}
	
	// Set pins as inputs (default state)
#if defined(RASPBERRY_PI3) || defined(RASPBERRY_PI4)
	*(gpiomap+1) &= ~(7<<(7*3));	// Pin 17
	*(gpiomap+2) &= ~(7<<(2*3));	// Pin 22
	*(gpiomap+2) &= ~(7<<(7*3));	// Pin 27
#elif defined(ODROID_N2)
	*(gpiomap+0x116) |= (1<<3);		// Pin X.3
	*(gpiomap+0x116) |= (1<<4);		// Pin X.4
	*(gpiomap+0x116) |= (1<<7);		// Pin X.7
#elif defined(ODROID_C1)
	*(gpiomap+0x0F) |= (1<<(8));		// Pin Y.8
	*(gpiomap+0x0C) |= (1<<(19));		// Pin X.19
	*(gpiomap+0x0C) |= (1<<(18));		// Pin X.18
#elif defined(ODROID_C2)
	*(gpiomap+0x118) |= (1<<(19));		// Pin X.19
	*(gpiomap+0x118) |= (1<<(11));		// Pin X.11
	*(gpiomap+0x118) |= (1<<(9));		// Pin X.9
#elif defined(ODROID_M1)
	rk_gpio(     0x02 + 0x01,  0, 0);	// Pin 0C.0
	rk_gpio(     0x02 + 0x01,  1, 0);	// Pin 0C.1
	rk_gpio(g3 + 0x02       , 10, 0);	// Pin 3B.2
//	rk_gpio(g3 + 0x02 + 0x01,  9, 0);	// Pin 3D.1
#endif

	//TODO: munmap the gpiomap.
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
	
	if (argc==2) {		// If there's exactly one argument
		std::string argument = argv[1];
		if (argument == "allon") {
			gpioInit();
			sendFrame16(-1);	// Dirty "all ones" hack
			commitFrame();
			gpioDeinit(1);
			exit(0);
		}
		if (argument == "alloff") {
			gpioInit();
			sendFrame16(0);
			commitFrame();
			gpioDeinit(1);
			exit(0);
		}
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
	exit(0);
}

