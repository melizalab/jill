
/**
 * @file   copy_queue.hh
 * @author Mike Lusignan <mlusigna@uchicago.edu>
 * @date   Wed Nov 3 2010
 *
 * @brief  A class for asynchronous file moves
 *
 * Copyright C Mike Lusignan, 2010.  Licensed for use under Creative
 * Commons Attribution-Noncommerical-Share Alike 3.0 United States
 * License (http://creativecommons.org/licenses/by-nc-sa/3.0/us/).
 */
#ifndef __FILE_COPY_QUEUE__H__
#define __FILE_COPY_QUEUE__H__

#include <list>
#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "logger.hh"

namespace jill{
namespace util{

/**
 * @brief move files to a remote destination asynchronously
 *
 * Class to move files to a remote location asynchronously.  The class
 * accepts new files through the add_file() method and moves the files
 * to the destination location specified at the creation of the class.
 *
 * To use, an instance of FileCopyQueue should be instantiated with
 * a destination, a maximum number of queued files, and an optional
 * logstream instance.  Once the run() method is invoked, files from the
 * queue will be moved in a separate thread, with the most recently added
 * file moved first.  To avoid a pileup of files in the case of high-latency
 * moves, files in excess of the queue maximum will be removed from the
 * queue and not moved.
 *
 * N.B. The destructor attempts to complete any moves in progress before
 *      returning, so destruction under the scenario of high-latency
 *      moves may take some time.  
 */
class FileCopyQueue {
public:
	FileCopyQueue(std::string str_dest, 
	              int max_files,
	              logstream *logv=NULL);
	~FileCopyQueue();

	/**
	 * Adds a file to the queue.
	 *
	 * @param  s file to add to the queue
	 */
	void add_file(std::string s);

	/**
	 * Stops the copying procedure.  If a move is in progress, the move
	 * is allowed to complete before stop() returns.  Thus, this method
	 * could require substantial time when moving files across a high-
	 * latency network.
	 */
	void stop();

	/**
	 * Starts the copying procedure.
	 */
	void run();

	/**
	 * Used by boost::thread to perform the copying.
	 */
	void operator()();

	/**
	 * Returns the running state of the move.
	 *
	 * @returns bool state
	 */
	bool is_running() { return _running; };
protected:
	std::string get_file();
	bool move_file(std::string str_file);
private:
	bool _running;
	std::string _final_dest;
	boost::mutex *_file_list_lock;
	boost::thread *_thread;
	std::list<std::string> _files;

	int _max_files;
	logstream *_logv;
};

}
}

#endif 
