
#include "copy_queue.hh"

#include <boost/thread/thread.hpp>
#include <boost/thread/thread_time.hpp>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>

//#define __FILE_COPY_QUEUE_USE_DELAY

namespace jill { 
namespace util {

FileCopyQueue::FileCopyQueue(std::string str_dest,
                             int max_files,
                             logstream *logv) :
                _final_dest(str_dest), 
                _max_files(max_files), 
		_running(false), 
                _logv(logv) {
	_file_list_lock = new boost::mutex();
	_thread = NULL;
	_files.clear();
}

FileCopyQueue::~FileCopyQueue() {
	stop();
	delete _file_list_lock;
	delete _thread;
	_files.clear();
} 

void FileCopyQueue::add_file(const std::string& filename, 
                             const FileCopyQueue::utimes_timeval& tv)
{
	// attempt to lock
	boost::mutex::scoped_lock(*_file_list_lock);
	this->_files.push_front(std::make_pair(filename, tv));
}

void FileCopyQueue::stop()
{
	// first, indicate that the copying should stop
	if (this->is_running()) {
		_running = false;
		this->_thread->join();
	} // if
}

class RunProxy {
public:
	RunProxy(FileCopyQueue &parent) : _parent(parent) {};
	void operator()() { _parent(); };
private:
	FileCopyQueue &_parent;
};

void FileCopyQueue::run()
{
	_running = true;
	if (this->_thread) {
		delete this->_thread;
	}
	RunProxy c(*this);
	this->_thread = new boost::thread(c);
}

void FileCopyQueue::operator()()
{
	while (this->is_running()) {
		std::string str;
		utimes_timeval tv;
		if (this->get_file(str, tv)) {
#ifdef __FILE_COPY_QUEUE_USE_DELAY
			// delay
			boost::posix_time::ptime t = boost::get_system_time();
			boost::posix_time::ptime later = t +
				boost::posix_time::seconds(30);
			while (t < later) {
				t = boost::get_system_time();
			} // while
#endif
			this->move_file(str, tv);
		} else {
			// sleep
			boost::posix_time::ptime t = boost::get_system_time();
			t = t + boost::posix_time::seconds(1);
			boost::thread::sleep(t);
		} // if
	} // while
}

bool FileCopyQueue::get_file(std::string &str, 
                             FileCopyQueue::utimes_timeval& tv)
{
	boost::mutex::scoped_lock(*_file_list_lock);
	int n = this->_files.size();
	if (n == 0)
		return false;

	while (this->_files.size() > this->_max_files) {
		this->_files.pop_back();
	} // while

	str = this->_files.front().first;
	tv  = this->_files.front().second;
	this->_files.pop_front();		
	return true;
}

bool FileCopyQueue::move_file(std::string old_name,
                              const FileCopyQueue::utimes_timeval& tv)
{
       // attempt to move file
        // first stat the location and verify that it is a directory
        struct stat stat_info;
        int stat_ret = stat(_final_dest.c_str(), &stat_info);
        if (stat_ret) {
                if (_logv) {
                        (*_logv) << "Failure to stat final destination: "
                                 << _final_dest
                                 << std::endl;
                } // if
                return false;
        } // if
        if (!S_ISDIR(stat_info.st_mode)) {
                if (_logv) {
                        (*_logv) << "Final destination is not a directory!"
                                 << std::endl;
                } // if
                return false;
        } // if
        std::string new_name = old_name;
        if (new_name.find_last_of("/") != std::string::npos) {
                new_name = new_name.substr(new_name.find_last_of("/")+1);
        } // if
        new_name = _final_dest + "/" + new_name;

        int rename_ret = rename(old_name.c_str(), new_name.c_str());
        bool copied = rename_ret == 0;
        if (!copied) { // rename_ret != 0 && errno ==) {
                // try copying
                int fp_src, fp_dest;
                fp_src = ::open(old_name.c_str(), O_RDONLY);
                fp_dest = ::open(new_name.c_str(),
                                 O_WRONLY | O_CREAT | O_TRUNC,
                                 S_IRUSR|S_IWUSR);
                if (fp_src != 0 && fp_dest != 0) {
                        const int buf_size=1024;
                        char buf[buf_size];
                        int bytes;

                        while ((bytes = read(fp_src, buf, buf_size)) > 0) {
                                ::write(fp_dest, buf, bytes);
                        } // while
                        ::close(fp_dest);
                        ::close(fp_src);

                        struct stat stat_info;
                        if (stat(new_name.c_str(), &stat_info) == 0) {
                                utimes(new_name.c_str(), &(tv.timeval1));
                                remove(old_name.c_str());
                                copied = true;
                        } // if
                } // if
        } // if
        if (_logv) {
                if (copied) {
                        (*_logv) << "Moved "
                                 << old_name
                                 << " to "
                                 << new_name
                                 << std::endl;
                } else {
                        (*_logv) << "File move failed!" << std::endl;
                } // if
        } // if (logv)

	return copied;
}

}
}

