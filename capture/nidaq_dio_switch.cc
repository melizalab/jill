
#include "nidaq_dio_switch.hh"
#include <comedilib.h>
#include <iostream>

namespace capture {

NidaqDioSwitch::NidaqDioSwitch(const char* device_name,
                               int subdevice,
                               int dio_line)
	: _device_name(device_name),
	  _subdevice(subdevice),
	  _dio_line(dio_line),
	  _previous_state(0)
{}

bool NidaqDioSwitch::initialize() {}
bool NidaqDioSwitch::finalize() {}

bool NidaqDioSwitch::get_state() {
	// open the device
	comedi_t *it;
	unsigned int bits;

	it = comedi_open(_device_name.c_str());
	comedi_dio_config(it, _subdevice, _dio_line, COMEDI_INPUT);
	comedi_dio_read(it, _subdevice, _dio_line, &bits);
	// std::cout << "subdev" << _subdevice << " line: " << _dio_line << " bits: " << bits << std::endl;
	comedi_close(it);

	int current_state = bits != 1;
	// register on -> off events only
	int trigger = _previous_state & !current_state;
	_previous_state = current_state;
	// std::cout << _previous_state << std::endl;
	return trigger;
}

}

