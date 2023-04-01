/*
coherent-rtlsdr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

coherent-rtlsdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with coherent-rtlsdr.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
//#include <stdint.h>
#include <string>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <condition_variable>
#include <complex>
#include <utility>

// #define USEZMQ

#ifdef USEZMQ
#include <zmq.hpp>
#else
#include <fstream>
#include <time.h>
#include <vector>
#include <cstring>
#endif
//byte-size buffers are static, thus, when the first object of class packetize is created
struct hdr0{
	uint32_t globalseqn;
	uint32_t N;
	uint32_t L;
	uint32_t unused;
};


class cpacketize{
	static int 						objcount;
	#ifdef USEZMQ
	static zmq::context_t 			*context;
	static zmq::socket_t  			*socket;
	#endif
	static std::mutex	  			bmutex;
	static std::condition_variable 	cv;
	static bool 					noheader;
	static size_t					packetlen;
	static uint32_t 				globalseqn;

	static int 						writecnt; //was atomic
	static bool						bufferfilled; //atomic

	static bool				 		do_exit; 	//atomic
	static uint32_t 				blocksize;

	static std::unique_ptr<int8_t[]> 	packetbuf0, packetbuf1; //samplebuffers, one is being sent, one written to
	static std::vector<std::complex<float>> pcorrection;
	

	static size_t packetlength(uint32_t N,uint32_t L);
	void resize_buffers(uint32_t N, uint32_t L);
public:

	//cpacketize(uint32_t N,uint32_t L, std::string address, bool no_header) : context(1), socket(context,ZMQ_PUB) {
	cpacketize();
	~cpacketize();
	static void init(std::string address,bool noheader_,uint32_t nchannels_,uint32_t blocksize_);
	static void cleanup();

	void request_exit();
	static int send();
	int write(uint32_t channeln,uint32_t readcnt,int8_t *rp);
	int write(uint32_t channeln,uint32_t readcnt,std::complex<float> *in);

	int writedebug(uint32_t channeln,std::complex<float> p);

	int notifysend();
};