
#ifndef _SWITCH_PLAYBACK_LISTENER_HH_
#define _SWITCH_PLAYBACK_LISTENER_HH_

namespace capture {

class SwitchPlaybackListener {
public:
	virtual ~SwitchPlaybackListener() {};

        virtual void playback(const char* name,
                              const char* song,
                              time_t time) = 0;
}; // class SwitchPlaybackListener

}

#endif
