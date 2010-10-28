
#include "switch_tracker.hh"

namespace capture {

SwitchTracker::SwitchTracker(const std::string &output_name,
                             const std::string &song_name,
                             int switch_refractory,
                             QuotaInfo &quotas,
                             boost::shared_ptr<SwitchPlaybackListener> listener,
                             boost::shared_ptr<Switch> sw) 
                             : _switch_refractory(switch_refractory),
                               _end_switch_refractory(0),
                               _output_name(output_name),
                               _song_name(song_name)
{
	_switch = sw;
	_listener = listener;

	_quotas = quotas;
	_tutor_client.reset(new SndfilePlayerClient(output_name.c_str()));
	std::string key = "tutor song";
	const char* song_name_as_char = song_name.c_str();
	_tutor_client->load_file(key, song_name_as_char);
	_tutor_client->select(key);
	_triggering_interval = -1;
	_triggering_count = 1;
} // SwitchTracker::SwitchTracker()

bool SwitchTracker::trigger() {
	// first, check switch!
	if (!_switch->get_state()) {
		return false;
	} // if

	// grab time
	time_t t = time(NULL);
	// get time of day
	struct tm tm;
	localtime_r(&t, &tm);
	int time_of_day = ((tm.tm_hour * 60) + tm.tm_min) * 60 + tm.tm_sec;

	std::cout << "Switch detected at " 
	          << tm.tm_hour << ":";
	if (tm.tm_min < 10)
		std::cout << '0';
	std::cout << tm.tm_min << ":";
	if (tm.tm_sec < 10)
		std::cout << '0';
	std::cout << tm.tm_sec << std::endl;

	if (_end_switch_refractory && t < _end_switch_refractory) {
		std::cout << "NO PLAYBACK: in switch refraction" << std::endl;
		return 0;
	} // if

	if (_tutor_client->is_running()) {
		std::cout << "NO PLAYBACK: previous playback still running" 
		          << std::endl;
		return 0;
	} // if

	// check tutor intervals
	int trigger = 0;

	int i=0;
	int found_quota_start, found_quota_end, found_quota_max;
	for (QuotaInfo::iterator iter = _quotas.begin();
             !trigger && iter != _quotas.end();
             iter++,i++) {

		QuotaInterval q = *iter;

		int start_time, end_time, max_count;
		start_time = q.start_time();
		end_time = q.end_time();
		max_count = q.quota();

		if (time_of_day > start_time && time_of_day < end_time) {
			if (_triggering_interval == i) {
				// check that we don't exceed count
				if (_triggering_count < max_count) {
					_triggering_count++;
					trigger = 1;
				} else {
					std::cout << "NO PLAYBACK: exceeded quota for this interval" << std::endl;
					return 0;
				} // if
			} else {
				_triggering_interval = i;
				_triggering_count = 1;
				trigger = 1;
			} // if
			found_quota_start = start_time;
			found_quota_end = end_time;
			found_quota_max = max_count;
			break;
			std::cout << "playback " << _triggering_count << " of " << max_count << std::endl;
		} // if
	} // for

	if (!trigger) {
		// no suitable interval
		std::cout << "NO PLAYBACK: outside quota intervals" << std::endl;
		return 0;
	} // if

	if (_listener) {
		time_t t = time(NULL);
		std::cout << "Playback: (" 
		          << _song_name
		          << "), "
		          << _triggering_count 
		          << " of " 
		          << found_quota_max 
		          << std::endl;
		_listener->playback(_output_name.c_str(), 
		                    _song_name.c_str(), 
		                    t);
	} // if
	_tutor_client->oneshot();
	_song_ended = 0;
	_end_switch_refractory = t + _switch_refractory;
	return 1;
} // SwitchTracker::trigger()

bool SwitchTracker::song_ended() {
	if (!_song_ended) {
		_song_ended = !_tutor_client->is_running();
	}

	if (_song_ended) {
		return 1;
	}
	return _song_ended;
} // SwitchTracker::song_ended()

bool SwitchTracker::stop_playback(const char* msg)
{
	_tutor_client->stop(msg);
	return true;
} // stop_playback

bool SwitchTracker::connect_playback_output(const char* channel_name)
{
	_tutor_client->connect_output(channel_name);
	return true;
} // SwitchTracker::connect_playback_output()

}
