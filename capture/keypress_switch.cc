
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "keypress_switch.hh"

struct termios orig_termios;

void reset_terminal_mode() {
	tcsetattr(0, TCSANOW, &orig_termios);
}

bool capture::KeypressSwitch::initialize() {
	struct termios new_termios;

	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));

	atexit(reset_terminal_mode);
//	cfmakeraw(&new_termios);
	// just turn off line buffering
	new_termios.c_lflag &= ~ICANON;
	tcsetattr(0, TCSANOW, &new_termios);

	return true;
}

#include <iostream>

bool capture::KeypressSwitch::get_state() {
	struct timeval tv = {0L, 0L};
	fd_set fds;
	FD_SET(0, &fds);
	int state = select(1, &fds, NULL, NULL, &tv);

	if (state) {
		unsigned char c;
		// eat first character
		read(0, &c, sizeof(c));

		// check for more
		while(select(1, &fds, NULL, NULL, &tv)) {
			read(0, &c, sizeof(c));
		} // while
	} // if
	if (state != 0) {
		std::cout << "keypress state = " << (state != 0) << std::endl;
	} // if
	return state != 0;
}


