
#ifndef _NIDAQ_DIO_SWITCH_HH_
#define _NIDAQ_DIO_SWITCH_HH_

#include "switch.hh"
#include <string>

namespace capture {

class NidaqDioSwitch : public Switch {
public:
	NidaqDioSwitch(const char* device_name,
	               int subdevice,
	               int dio_line);
	virtual bool initialize();
	virtual bool finalize();
	virtual bool get_state();
private:
	std::string _device_name;
	int _subdevice;
	int _dio_line;
	int _previous_state;
}; // class NidaqDioSwitch

}

#endif 
