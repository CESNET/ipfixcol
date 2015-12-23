/**
 * \file File.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief File output (source file)
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "File.h"
#include <stdexcept>
#include <string>
#include <vector>

#include <cstring>
#include <cerrno>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define DEF_WINDOW_SIZE (300)
#define DEF_WINDOW_ALIGN (true)

static const char *msg_module = "json_storage(file)";

/**
 * \brief Class constructor
 *
 * Parse output configuration and create an output file
 * \param config[in] XML configuration
 */
File::File(const pugi::xpath_node &config)
{
	unsigned int w_size;
	bool w_align;
	std::string path, prefix;

	// Storage path
	path = config.node().child_value("path");
	if (path.empty()) {
		throw std::invalid_argument("Missing storage path specification.");
	}

	// Make sure the path ends with '/' character
	if (path.back() != '/') {
		path += '/';
	}

	// File prefix
	prefix = config.node().child_value("prefix");

	// Windows size & interval
	pugi::xml_node ie = config.node().child("dumpInterval");
	if (!ie) {
		// Node not found
		w_size = DEF_WINDOW_SIZE;
		w_align = DEF_WINDOW_ALIGN;
		MSG_WARNING(msg_module, "Dump interval not specified. Using default "
			"values (timeWindow: %u, timeAlignment: %s).", w_size,
			w_align ? "yes" : "no");
	} else {
		// Node found
		std::string tmp;
		tmp = ie.child_value("timeWindow");

		if (tmp.empty()) {
			w_size = DEF_WINDOW_SIZE;
			MSG_WARNING(msg_module, "Time window not specified. Using "
				"default value (%u).", w_size);
		} else {
			try {
				w_size = std::stoul(tmp);
			} catch (std::exception &e) {
				throw std::invalid_argument("Invalid size of the time window.");
			}
		}

		if (w_size < _WINDOW_MIN_SIZE) {
			throw std::invalid_argument("Window size is smaller then minimal "
				"value.");
		}

		tmp = ie.child_value("timeAlignment");
		if (tmp.empty()) {
			w_align = DEF_WINDOW_ALIGN;
			MSG_WARNING(msg_module, "Window alignment not specified. Using "
				"default value (%s).", w_align ? "yes" : "no");
		} else {
			w_align = (strcasecmp(tmp.c_str(), "yes") == 0 || tmp == "1");
		}
	}

	// Prepare a configuration of the thread for changing time windows
	_thread = new thread_ctx_t;
	_thread->new_file = NULL;
	_thread->new_file_ready = false;
	_thread->stop = false;

	_thread->storage_path = path;
	_thread->file_prefix = prefix;
	_thread->window_size = w_size;
	time(&_thread->window_time);

	if (w_align) {
		// Window alignment
		_thread->window_time = (_thread->window_time / _thread->window_size) *
			_thread->window_size;
	}

	// Create directory & first file
	FILE *new_file = file_create(_thread->storage_path, _thread->file_prefix,
		_thread->window_time);
	if (!new_file) {
		delete _thread;
		throw std::runtime_error("Failed to create a time window file.");
	}

	_file = new_file;

	if (pthread_mutex_init(&_thread->mutex, NULL) != 0) {
		fclose(_file);
		delete _thread;
		throw std::runtime_error("Mutex initialization failed");
	}

	if (pthread_create(&_thread->thread, NULL, &File::thread_window,
			_thread) != 0) {
		fclose(_file);
		pthread_mutex_destroy(&_thread->mutex);
		delete _thread;
		throw std::runtime_error("Failed to start a thread for changing time "
			"windows.");
	}
}

/**
 * \brief Class destructor
 *
 * Close all opened files
 */
File::~File()
{
	if (_file) {
		fclose(_file);
	}

	if (_thread) {
		_thread->stop = true;
		pthread_join(_thread->thread, NULL);
		pthread_mutex_destroy(&_thread->mutex);

		if (_thread->new_file) {
			fclose(_thread->new_file);
		}

		delete _thread;
	}
}

/**
 * \brief Thread function for changing time windows
 * \param[in,out] context Thread configuration
 * \return Nothing
 */
void *File::thread_window(void *context)
{
	thread_ctx_t *ctx = (thread_ctx_t *) context;
	MSG_DEBUG(msg_module, "Thread started...");

	while(!ctx->stop) {
		// Sleep
		struct timespec tim;
		tim.tv_sec = 0;
		tim.tv_nsec = 100000000L; // 0.1 sec
		nanosleep(&tim, NULL);

		// Get current time
		time_t now;
		time(&now);

		if (difftime(now, ctx->window_time) <= ctx->window_size) {
			continue;
		}

		// New time window
		pthread_mutex_lock(&ctx->mutex);
		if (ctx->new_file) {
			fclose(ctx->new_file);
			ctx->new_file = NULL;
		}

		ctx->window_time += ctx->window_size;
		FILE *file = file_create(ctx->storage_path, ctx->file_prefix,
			ctx->window_time);
		if (!file) {
			MSG_ERROR(msg_module, "Failed to create a time window file.");
		}

		// Null pointer is also valid...
		ctx->new_file = file;
		ctx->new_file_ready = true;
		pthread_mutex_unlock(&ctx->mutex);
	}

	MSG_DEBUG(msg_module, "Thread terminated.");
	return NULL;
}

/**
 * \brief Store a record to a file
 * \param[in] record JSON record
 */
void File::ProcessDataRecord(const std::string &record)
{
	// Should we change a time window
	if (_thread->new_file_ready) {
		// Close old time window
		if (_file) {
			fclose(_file);
		}

		// Get new time window
		pthread_mutex_lock(&_thread->mutex);
		_file = _thread->new_file;
		_thread->new_file = NULL;
		_thread->new_file_ready = false;
		pthread_mutex_unlock(&_thread->mutex);
	}

	if (!_file) {
		return;
	}

	// Store the record
	fwrite(record.c_str(), record.size(), 1, _file);
}

/**
 * \brief Get a directory path for a time window
 * \param[in] tm Time window
 * \param[in] tmplt Template of the directory path
 * \param[out] dir Directory path
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int File::dir_name(const time_t &tm, const std::string &tmplt, std::string &dir)
{
	char dir_fmt[1024];

	// Get UTC time
	struct tm gm;
	if (gmtime_r(&tm, &gm) == NULL) {
		MSG_ERROR(msg_module, "Failed to convert time to UTC.");
		return 1;
	}

	// Convert time template to a string
	if (strftime(dir_fmt, sizeof(dir_fmt), tmplt.c_str(), &gm) == 0) {
		MSG_ERROR(msg_module, "Failed to fill storage path template.");
		return 1;
	}

	dir = std::string(dir_fmt);
	return 0;
}

/**
 * \brief Create a directory for a time window
 *
 * \warning Directory must ends with '/'. Otherwise only directories before last
 * symbol '/' will be created.
 * \param[in] path Directory path
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int File::dir_create(const std::string &path)
{
	const mode_t mask = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

	if (path.empty()) {
		return 1;
	}

	std::string tmp = path;
	std::size_t pos = std::string::npos;
	std::vector<std::string> mkdir_todo;
	bool stop = 0;

	// Try to create directories from the end
	while (!stop && (pos = tmp.find_last_of('/', pos)) != std::string::npos) {
		// Try to create a directory
		std::string aux_str = tmp.substr(0, pos + 1);
		if (aux_str == "/") {
			// Root
			MSG_ERROR(msg_module, "Failed to create a storage directory '%s'.",
				path.c_str());
			return 1;
		}

		// Create a directory
		if (mkdir(aux_str.c_str(), mask) == 0) {
			// Directory created
			break;
		} else {
			// Failed to create the directory
			switch(errno) {
			case EEXIST:
				// Directory already exists
				stop = 1;
				break;
			case ENOENT:
				// Parent directory is missing
				mkdir_todo.push_back(aux_str);
				pos--;
				continue;
			default:
				// Other errors
				MSG_ERROR(msg_module, "Failed to create a directory %s (%s).",
					aux_str.c_str(), strerror(errno));
				return 1;
			}
		}
	}

	// Create remaining directories
	while (!mkdir_todo.empty()) {
		std::string aux_str = mkdir_todo.back();
		mkdir_todo.pop_back();

		if (mkdir(aux_str.c_str(), mask) != 0) {
			// Failed to create directory
			MSG_ERROR(msg_module, "Failed to create a directory %s (%s).",
					aux_str.c_str(), strerror(errno));
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Create a file for a time window
 *
 * Check/create a directory hierarchy and create a new file for time window.
 * \param[in] tm Time window
 * \return On success returns pointer to the file, Otherwise returns NULL.
 */
FILE *File::file_create(const std::string &tmplt, const std::string &prefix,
	const time_t &tm)
{
	char file_fmt[20];

	// Get UTC time
	struct tm gm;
	if (gmtime_r(&tm, &gm) == NULL) {
		MSG_ERROR(msg_module, "Failed to convert time to UTC.");
		return NULL;
	}

	// Convert time template to a string
	if (strftime(file_fmt, sizeof(file_fmt), "%Y%m%d%H%M", &gm) == 0) {
		MSG_ERROR(msg_module, "Failed to create a flow file.");
		return NULL;
	}

	// Check/create a directory
	std::string directory;
	if (dir_name(tm, tmplt, directory) != 0) {
		return NULL;
	}

	if (dir_create(directory) != 0) {
		return NULL;
	}

	std::string file_name = directory + prefix + file_fmt;
	FILE *file = fopen(file_name.c_str(), "w");
	if (!file) {
		// Failed to create a flow file
		MSG_ERROR(msg_module, "Failed to create a flow file '%s' (%s).",
			file_name.c_str(), strerror(errno));
		return NULL;
	}

	return file;
}
