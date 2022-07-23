#include <cstdint>
#include <functional>
#include <ios>
#include <zmq.h>
#include <zmq.hpp>
#include <iostream>
#include <fstream>
#include <chrono>

using std::cout, std::endl;

struct human_size {
  size_t bytes;
  size_t kbytes;
  size_t mbytes;
  size_t gbytes;
};

// Return a struct containing the number of kb, mb, and gb in a given number of bytes.
struct human_size size2human(size_t bytes){
  struct human_size ret;
  ret.bytes = bytes;
  ret.kbytes = bytes >> 10;
  ret.mbytes = bytes >> 20;
  ret.gbytes = bytes >> 30;

  return ret;
}

int main() {

  // Connect socket
  zmq::context_t ctx(1);
  zmq::socket_t sock(ctx, ZMQ_SUB);
  sock.set(zmq::sockopt::subscribe, "");

  cout << "Connecting to tcp://localhost:5555" << endl;
  sock.connect("tcp://localhost:5555");
  cout << "Connected." << endl;
  
  // Program vars  
  int timeoutms = 500;
  int max_reads = 1, num_reads = 0;
  std::chrono::seconds max_read_time(60);
  size_t total_read = 0;
  bool read_time = false; // If true, read until max_read_time is reached. Else read max_reads messages.
  std::optional<std::fstream> outfile;

  // Configure socket
  sock.set(zmq::sockopt::rcvtimeo, timeoutms);
  
  // Print config info
  cout << "Set a "<< timeoutms <<"ms timeout for socket reads." << endl;

  if( read_time ){
    cout << "Reading for a maximum of " << max_read_time << endl;
  }else{
    cout << "Reading a maximum of " << max_reads << " messages." << endl;
  }
  
  auto start_time = std::chrono::system_clock::now();
  
  // Small lambda to tell if the read loop should exit. 
  auto loopexit = [&](void) -> bool {
    if( read_time ){
      auto elapsed = std::chrono::system_clock::now() - start_time;
      return elapsed < max_read_time;
    }else{
      return num_reads < max_reads;
    }
  };
  
  // Read loop.
  while( loopexit() ){

    // Read a message.
    zmq::message_t msg;
    zmq::recv_result_t ret = sock.recv(msg); // recv_result_t is an optional wrapping a size_t
    
    // Error checking for message
    if( !ret.has_value() || *ret == 0 ){
      cout << "Recieved nothing from the socket!" << endl;
      if( outfile.has_value() ) break;
      else return -1;
    }else if( !outfile.has_value() ){ // Set outfile if data found.
      outfile = std::fstream("outfile.bin", std::ios_base::binary | std::ios_base::out);
      cout << "Found data. Created outfile.bin" << endl;
    }
  
    // Parse out small header from data.
    // Data header is 16 bytes
    // * global sequence number
    // * number of channels
    // * number of IQ pairs per channel
    // * unused 32-bit int
    int8_t *s8bit = msg.data<int8_t>();
    uint32_t gseq = *((uint32_t*)s8bit); // First uint
    uint32_t chnls = *(((uint32_t*)s8bit)+1); // 2nd uint. 
    uint32_t blocksize = *(((uint32_t*)s8bit)+2); // 3rd uint. 
    uint32_t unused = *(((uint32_t*)s8bit)+3); // 4th uint
    uint32_t ch0 = *(((uint32_t*)s8bit)+4); // Channel 0 blocksize
    uint32_t ch1 = *(((uint32_t*)s8bit)+5); // Channel 1 blocksize
    uint32_t ch2 = *(((uint32_t*)s8bit)+6); // Channel 2 blocksize
    uint32_t ch3 = *(((uint32_t*)s8bit)+7); // Channel 3 blocksize
    uint32_t ch4 = *(((uint32_t*)s8bit)+8); // Channel 4 blocksize
    size_t msgsize = msg.size() - 36;
    
    cout << "GSEQN: " << gseq << " " << chnls << " channels X " << blocksize << " IQ pairs" << endl;
     
    cout << "Unused: " << unused << endl;

    cout << "Ch0: " << ch0 << endl;
    cout << "Ch1: " << ch1 << endl;
    cout << "Ch2: " << ch2 << endl;
    cout << "Ch3: " << ch3 << endl;
    cout << "Ch4: " << ch4 << endl;
    
    int8_t *bch0 = s8bit + 36;
    int8_t *bch1 = s8bit + 36 + 2*blocksize;
    int8_t *bch2 = s8bit + 36 + 2*blocksize*2;
    int8_t *bch3 = s8bit + 36 + 2*blocksize*3;
    int8_t *bch4 = s8bit + 36 + 2*blocksize*4;
    
    // Test theory that there might be a second packet after the first one?
    int8_t *second = bch4 + 2*blocksize;     
    gseq = *((uint32_t*)second); // First uint
    chnls = *(((uint32_t*)second)+1); // 2nd uint. 
    blocksize = *(((uint32_t*)second)+2); // 3rd uint. 
    unused = *(((uint32_t*)second)+3); // 4th uint
    ch0 = *(((uint32_t*)second)+4); // Channel 0 blocksize
    ch1 = *(((uint32_t*)second)+5); // Channel 1 blocksize
    ch2 = *(((uint32_t*)second)+6); // Channel 2 blocksize
    ch3 = *(((uint32_t*)second)+7); // Channel 3 blocksize
    ch4 = *(((uint32_t*)second)+8); // Channel 4 blocksize
    msgsize = msg.size() - 36;
    
    cout << "GSEQN: " << gseq << " " << chnls << " channels X " << blocksize << " IQ pairs" << endl;
     
    cout << "Unused: " << unused << endl;

    cout << "Ch0: " << ch0 << endl;
    cout << "Ch1: " << ch1 << endl;
    cout << "Ch2: " << ch2 << endl;
    cout << "Ch3: " << ch3 << endl;
    cout << "Ch4: " << ch4 << endl;
    


    // cout << "Recieved message of size " << msgsize << ". (" << msgsize/2 << " complex nums)" << endl;
    cout << "Completed read no. " << num_reads << "\r";
    cout.flush();

    // Write data
    outfile->write((char*)(s8bit + 12), msgsize);
    
    total_read += msg.size();
    num_reads++;
    
    // Ensures that the msg object is not destroyed after until we're done with data processing.
    msg.data();
  }

  outfile->close();

  cout << "\nFile written." << endl;
  
  auto byteinfo = size2human(total_read); 
  cout << "Read a total of " << byteinfo.bytes << " bytes." << endl;
  cout << byteinfo.kbytes << " Kb" << endl << byteinfo.mbytes << " Mb" << endl;

}
