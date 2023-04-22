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

#ifndef CREFNOISEH
#define CREFNOISEH

#include <iostream>
#include <rtl-sdr.h>
#include "csdrdevice.h"

class crefnoise{
private:
	bool enabled;
	crefsdr* dev;
public:
	void set_state(bool s){
		std::cout << "noise set to " << s << std::endl;
		enabled = s;
		if (s)
			rtlsdr_set_bias_tee_gpio(dev->dev, 0, 1);
		else
			rtlsdr_set_bias_tee_gpio(dev->dev, 0, 0);
		
	};

	bool isenabled(){
		return enabled;
	}

	crefnoise(crefsdr* dev){
		this->dev = dev;

	};
	
	~crefnoise(){
		
	};
};
#endif
