
#ifndef _SWITCH_HH_
#define _SWITCH_HH_

namespace capture {

class Switch {
public:
	virtual ~Switch() { finalize(); };

        virtual bool initialize() { return true; };
        virtual bool finalize() { return true; };
        virtual bool get_state() = 0;
}; // class Switch

}

#endif
