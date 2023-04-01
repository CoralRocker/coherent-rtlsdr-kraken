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
#include <zmq.hpp>
#include <string>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <condition_variable>

struct hdr0{
	uint32_t globalseqn;
	uint32_t N;
	uint32_t L;
	uint32_t unused;
};

class cpacketize{
	std::atomic<int>	writecnt;
	std::atomic<bool>	bufferfilled;

	std::atomic<bool>	do_exit;

	uint32_t 			globalseqn;
	uint32_t 			nchannels;
	uint32_t 			blocksize;
	bool 				noheader;
	size_t	 			packetlength;
	zmq::context_t 		context;
	zmq::socket_t  		socket;

	std::mutex          bmutex;
    std::condition_variable cv;

	int8_t *packetbuf0, *packetbuf1;
public:
	cpacketize(uint32_t N,uint32_t L, std::string address, bool no_header) : context(1), socket(context,ZMQ_PUB) {
		globalseqn	= 0;
		nchannels	= N;
		blocksize	= L;
		noheader 	= no_header;
		socket.bind(address);
        writecnt    = nchannels;
        bufferfilled= false;

        do_exit  	= false;

		if (noheader)
			packetlength = 2*nchannels*blocksize;
		else
			packetlength = (16 + 4*nchannels) + 2*nchannels*blocksize;

		packetbuf0	= new int8_t[packetlength];
		packetbuf1	= new int8_t[packetlength];
	}

	~cpacketize(){
		sleep(1);
		delete [] packetbuf0;
		delete [] packetbuf1;
	}

	void request_exit(){
		do_exit = true;
	};

	int send();

	//the idea is that each thread could call this method from it's own context, as we're writing to separate locations
	int write(uint32_t channeln,uint32_t readcnt,int8_t *rp);


};
