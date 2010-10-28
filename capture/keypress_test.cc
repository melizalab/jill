
#include "keypress_switch.hh"

#include <iostream>
using namespace std;

int main() {
	capture::KeypressSwitch k;

	k.initialize();

	time_t original_t = time(NULL);
	time_t now_t = time(NULL);

	while (now_t - original_t < 120) {
//		cout << "Waiting...." << endl;
		::usleep(100);
		if (k.get_state()) {
			cout << "Got key!" << endl;
		} // if
		now_t = time(NULL);
	} // while
}
