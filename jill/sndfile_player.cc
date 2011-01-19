#include "sndfile_player.hh"
#include "util/sndfile_reader.hh"
#include "util/logger.hh"
#include <samplerate.h>

using namespace jill;

nframes_t
SndfilePlayer::load_file(const key_type &key, const char * audiofile)
{
	if (audiofile == 0) return 0;
	util::SndfileReader sf(audiofile);
	nframes_t nframes = sf.frames();
	data_array buf(new sample_t[nframes]);
	sf(buf.get(), nframes);

	return load_data(key, buf, nframes, sf.samplerate());
}

nframes_t
SndfilePlayer::load_data(const key_type &key, data_array buf, nframes_t size, nframes_t rate)
{
	if (rate != _samplerate) {
		if (_logger)
			*_logger << util::logstream::allfields << key << ": resampling "
				 << size << " samples from "
				 << rate << "->" << _samplerate << " Hz" << std::endl;
		SRC_DATA data;
		data.input_frames = size;
		data.data_in = buf.get();
		data.src_ratio = float(_samplerate) / float(rate);
		size = data.output_frames = (int)(size * data.src_ratio);

		_buf.reset(new sample_t[size]);
		data.data_out = _buf.get();

		int ec = src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
		if (ec != 0)
			throw FileError(std::string("Resampling error: ") + src_strerror(ec));
	}
	else
		_buf = buf;

	_buf_size = _buf_pos = size; // otherwise playback starts immediately
	_buffers[key] = _buf;
	_buffer_sizes[key] = _buf_size;
	return _buf_size;
}

nframes_t
SndfilePlayer::select(const key_type &key)
{
	std::map<key_type, data_array>::const_iterator it1 = _buffers.find(key);
	std::map<key_type, nframes_t>::const_iterator it2 = _buffer_sizes.find(key);
	if (it1 == _buffers.end() || it2 == _buffer_sizes.end())
		throw NoSuchKeyError(key);

	_buf_name = it1->first;
	_buf = it1->second;
	_buf_size = _buf_pos = it2->second;
	return _buf_size;
}

void
SndfilePlayer::operator()(sample_t * __restrict buffer, nframes_t nframes) __restrict
{
	int avail = _buf_size - _buf_pos;
	if (avail > 0) {
		if (avail > int(nframes)) avail = nframes;
		memcpy(buffer, _buf.get()+_buf_pos, avail * sizeof(sample_t));
		_buf_pos += avail;
	}
	if (avail < nframes)
		memset(buffer+avail, 0, (nframes-avail) * sizeof(sample_t));

}


namespace jill {

std::ostream &
operator<< (std::ostream &os, const SndfilePlayer &player)
{
	os << player._buf_name << ": " << player._buf_pos << '/' << player._buf_size 
	   << " @ " << player._samplerate << " Hz";
	return os;
}

}
