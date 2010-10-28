
#ifndef _KEYPRESS_SWITCH_HH_
#define _KEYPRESS_SWITCH_HH_

#include "switch.hh"

namespace capture {

class KeypressSwitch : public Switch {
public:
	virtual bool initialize();
	virtual bool finalize() {return true;};
	virtual bool get_state();
}; // class KeypressSwitch

}

#endif 
