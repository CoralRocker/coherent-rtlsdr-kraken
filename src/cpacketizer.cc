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

#include "cpacketizer.h"
#include <algorithm>
#include <memory>
#include "cdsp.h"

#include <iostream>

int 						cpacketize::objcount=0;
uint32_t 					cpacketize::globalseqn=0;
std::condition_variable 	cpacketize::cv;
#ifdef USEZMQ
zmq::socket_t		 		*cpacketize::socket;
zmq::context_t 				*cpacketize::context;
#endif
std::mutex 					cpacketize::bmutex;
std::unique_ptr<int8_t[]>	cpacketize::packetbuf0;
std::unique_ptr<int8_t[]>	cpacketize::packetbuf1;
bool 						cpacketize::noheader;
size_t 						cpacketize::packetlen=0;

int 						cpacketize::writecnt=0;
bool						cpacketize::bufferfilled=false;
uint32_t					cpacketize::blocksize=0;
bool						cpacketize::do_exit=false;
std::vector<std::complex<float>> cpacketize::pcorrection;

#ifdef USEZMQ
zmq::socket_t *debugsocket;
#endif
cpacketize::cpacketize(){

    objcount++; 			//this is a stupid approach. should just alloc a large buffer beforehand...realloc sparsely if need be.
  	if (packetlength(objcount,blocksize)>packetlen){
  		resize_buffers(objcount,blocksize);		
  	}
}
cpacketize::~cpacketize(){
	objcount--;
	resize_buffers(objcount,blocksize);
}


void cpacketize::init(std::string address,bool noheader_,uint32_t nchannels_,uint32_t blocksize_){
	#ifdef USEZMQ
	context = new zmq::context_t(1);
	socket  = new zmq::socket_t(*context,ZMQ_PUB);
	socket->bind(address.data());
	#endif
	noheader = noheader_;
	blocksize= blocksize_;
	#ifdef USEZMQ
	debugsocket = new zmq::socket_t(*context,ZMQ_PUB);
	debugsocket->bind("tcp://*:5557");
	#endif
	packetbuf0 = std::make_unique<int8_t[]>(cpacketize::packetlength(nchannels_,blocksize_));
	packetbuf1 = std::make_unique<int8_t[]>(cpacketize::packetlength(nchannels_,blocksize_));

	packetlen  = cpacketize::packetlength(nchannels_,blocksize_);
	
	pcorrection.resize(nchannels_,std::complex<float>(0.0f,0.0f));
}

void cpacketize::cleanup(){
	#ifdef USEZMQ
	socket->close();
	debugsocket->close();
	delete context;
	delete socket;

	delete debugsocket;
	#endif
}



void cpacketize::request_exit(){
		do_exit = true;
}

size_t cpacketize::packetlength(uint32_t N,uint32_t L){
		if (noheader)
			return (2*N*L);
		else
			return (16 + 4*N) + 2*N*L;
}

void cpacketize::resize_buffers(uint32_t N, uint32_t L){
	size_t plen = packetlength(N,L);
	{//lock, switch pointers, delete, release lock
		std::lock_guard<std::mutex> lock(bmutex);
		packetbuf0 = std::make_unique<int8_t[]>(cpacketize::packetlength(N,L));
		packetbuf1 = std::make_unique<int8_t[]>(cpacketize::packetlength(N,L));

		pcorrection.resize(N,std::complex<float>(0.0f,0.0f));
	}
}
#ifdef USEZMQ
int cpacketize::send(){
	if (!noheader){
		//fill static header. block readcounts filled by calls to write:
		hdr0 *hdr 		= (hdr0 *) packetbuf0.get();
		hdr->globalseqn	= globalseqn++;
		hdr->N 			= objcount; //nchannels;
		hdr->L 			= blocksize >> 1;
		hdr->unused 	= 0;
		//std::cout << "sending packet: "<<std::to_string(hdr->N) <<"x" << std::to_string(hdr->L) << "header " << std::to_string(noheader) << std::endl;
	}
		
	{
	    std::unique_lock<std::mutex> lock(bmutex);
	    cv.wait(lock,[]{return ((bufferfilled) || (do_exit));});
	    bufferfilled = false;
	}
    socket->send(packetbuf1.get(),packetlen,0);
    //printf("sending %d phase factors\n",objcount);
    debugsocket->send(pcorrection.data(),objcount*sizeof(std::complex<float>),0);
    return 0;
}
#else
int cpacketize::send() {
	if (!noheader){
		//fill static header. block readcounts filled by calls to write:
		hdr0 *hdr 		= (hdr0 *) packetbuf0.get();
		hdr->globalseqn	= globalseqn++;
		hdr->N 			= objcount; //nchannels;
		hdr->L 			= blocksize >> 1;
		hdr->unused 	= 0;
		//std::cout << "sending packet: "<<std::to_string(hdr->N) <<"x" << std::to_string(hdr->L) << "header " << std::to_string(noheader) << std::endl;
	}
		
	{
	    std::unique_lock<std::mutex> lock(bmutex);
	    cv.wait(lock,[]{return ((bufferfilled) || (do_exit));});
	    bufferfilled = false;
	}
	//https://stackoverflow.com/questions/20619236/how-to-get-utc-time
	char fname[200];
	time_t t = time(nullptr);
    struct tm *tmp;
    const char* fmt = "%d%b%y_%T_%z.bin";
	tmp = gmtime(&t);
	strftime(fname, sizeof(fname), fmt, tmp);

	std::ofstream ofs(fname, std::ios::binary);
	ofs.write((char*)packetbuf1.get(), packetlen);
	ofs.close();

    return 0;
}
#endif

int cpacketize::writedebug(uint32_t channeln,std::complex<float> p){
	pcorrection[channeln] = p;
    return 0;
}

//the idea is that each thread could call this method from it's own context, as we're writing to separate locations
int cpacketize::write(uint32_t channeln,uint32_t readcnt,int8_t *rp){
    uint32_t loc;

    if (!noheader){
        //fill dynamic size part of header, write readcounts
        *(((uint32_t *)packetbuf0.get()) + sizeof(hdr0)/sizeof(uint32_t)+channeln)=readcnt;
        loc = (sizeof(hdr0)+objcount*sizeof(uint32_t)) + channeln*blocksize;
    }
    else{
        loc = channeln*blocksize;
    }

    //printf("writing to %p, %d bytes, %d reserved size...",(int8_t *) (packetbuf0.get() +loc),blocksize, packetlength(objcount,blocksize));
    //copy data
    std::memcpy((int8_t *) (packetbuf0.get() +loc),rp,blocksize);
    //printf("written\n");

    //if(writecnt--==0)
    return 0;
}

int cpacketize::write(uint32_t channeln,uint32_t readcnt,std::complex<float> *in){
    uint32_t loc;

    if (!noheader){
        //fill dynamic size part of header, write readcounts
        *(((uint32_t *)packetbuf0.get()) + sizeof(hdr0)/sizeof(uint32_t)+channeln)=readcnt;
        loc = (sizeof(hdr0)+objcount*sizeof(uint32_t)) + channeln*blocksize;
    }
    else{
        loc = channeln*blocksize;
    }
    cdsp::convto8bit((std::complex<int8_t> *) (packetbuf0.get()+loc),in, (blocksize>>1));
    //std::memcpy((int8_t *) (packetbuf0.get() +loc),rp,blocksize);
    return 0;
}

int cpacketize::notifysend(){
	std::unique_lock<std::mutex> lock(bmutex);

	packetbuf0.swap(packetbuf1);
    writecnt = objcount;

	bufferfilled = true;
	lock.unlock();

	cv.notify_one();
    return 0;
} 


