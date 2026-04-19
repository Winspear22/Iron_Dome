/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: adnen <adnen@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/19 18:18:53 by adnen             #+#    #+#             */
/*   Updated: 2026/04/19 21:11:30 by adnen            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "includes.hpp"

#define READ_THRESHOLD          50
#define READ_WINDOW_SECONDS     10
#define ENTROPY_DELTA           1.5
#define ENTROPY_ABSOLUTE        7.2
#define ENTROPY_DROP_THRESHOLD  200
#define CRYPTO_CHECK_INTERVAL   5
#define ENTROPY_POOL_FILE       "/proc/sys/kernel/random/entropy_avail"

static std::map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> readCounters;
static std::map<std::string, double> previousEntropy;
static int previousEntropyPool = -1;

// ── Logging ──────────────────────────────────────────────────────────────────

void writeLog(const std::string &message)
{
	std::ofstream stream("/var/log/irondome/irondome.log", std::ios::app);
	if (!stream.is_open())
		exit(EXIT_FAILURE);
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::string ts = std::ctime(&t);
	ts.pop_back();
	stream << ts << " | " << message << "\n";
	stream.close();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void checkForRootPermission()
{
	uid_t uid = geteuid();
	if (uid != 0)
	{
		std::cerr << RED_BOLD << "Error: must be run as root." << RESET << std::endl;
		exit(EXIT_FAILURE);
	}
}

void setupLog()
{
	std::string logDir  = "/var/log/irondome/";
	std::string logFile = "/var/log/irondome/irondome.log";
	if (!std::filesystem::exists(logDir))
		std::filesystem::create_directories(logDir);
	std::ofstream stream(logFile, std::ios::app);
	if (!stream.is_open())
	{
		std::cerr << RED_BOLD << "Error: cannot open log file." << RESET << std::endl;
		exit(EXIT_FAILURE);
	}
	stream.close();
}

void daemonize()
{
	pid_t pid = fork();
	if (pid < 0)
	{
		std::cerr << RED_BOLD << "Error: fork() failure." << RESET << std::endl;
		exit(EXIT_FAILURE);
	}
	if (pid > 0)
	{
		std::cout << GREEN_BOLD << "Daemon launched, PID: " << pid << RESET << std::endl;
		exit(EXIT_SUCCESS);
	}
	if (setsid() < 0)
	{
		writeLog("ERROR: setsid() failure");
		exit(EXIT_FAILURE);
	}
	pid_t pid2 = fork();
	if (pid2 < 0)
	{
		writeLog("ERROR: second fork() failure");
		exit(EXIT_FAILURE);
	}
	if (pid2 > 0)
		exit(EXIT_SUCCESS);
	umask(0);
	chdir("/");
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	int fd = open("/dev/null", O_RDWR);
	if (fd < 0)
	{
		writeLog("ERROR: cannot open /dev/null");
		exit(EXIT_FAILURE);
	}
	dup(fd);
	dup(fd);
	close(fd);
}

std::vector<std::filesystem::path> parsePaths(int argc, char **argv)
{
	std::vector<std::filesystem::path> paths;
	if (argc == 1)
	{
		paths.push_back("/home");
		paths.push_back("/etc");
		paths.push_back("/tmp");
		return paths;
	}
	int i = 1;
	while (i < argc)
	{
		std::filesystem::path p(argv[i]);
		if (std::filesystem::exists(p))
			paths.push_back(p);
		else
			writeLog("WARNING: path not found, skipped: " + p.string());
		i = i + 1;
	}
	if (paths.empty())
	{
		writeLog("ERROR: no valid paths provided");
		exit(EXIT_FAILURE);
	}
	return paths;
}

// ── Detection: disk read abuse ────────────────────────────────────────────────

void checkReadAbuse(const std::string &filename)
{
	auto now = std::chrono::steady_clock::now();
	if (readCounters.find(filename) == readCounters.end())
	{
		readCounters[filename] = {1, now};
		return;
	}
	long elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		now - readCounters[filename].second).count();
	if (elapsed > READ_WINDOW_SECONDS)
	{
		readCounters[filename] = {1, now};
		return;
	}
	readCounters[filename].first += 1;
	if (readCounters[filename].first > READ_THRESHOLD)
	{
		writeLog("ALERT: read abuse on " + filename
			+ " | " + std::to_string(readCounters[filename].first)
			+ " reads in " + std::to_string(READ_WINDOW_SECONDS) + "s");
		readCounters[filename].first = 0;
	}
}

// ── Detection: entropy changes ────────────────────────────────────────────────

double computeEntropy(const std::filesystem::path &filePath)
{
	std::ifstream stream(filePath, std::ios::binary);
	if (!stream.is_open())
		return -1;
	std::array<size_t, 256> freq{};
	size_t total = 0;
	char b;
	while (stream.get(b))
	{
		freq[(unsigned char)b] += 1;
		total += 1;
	}
	if (total == 0)
		return 0;
	double entropy = 0.0;
	size_t i = 0;
	while (i < 256)
	{
		if (freq[i] > 0)
		{
			double p = (double)freq[i] / (double)total;
			entropy -= p * std::log2(p);
		}
		i += 1;
	}
	return entropy;
}

void checkEntropy(const std::filesystem::path &filePath)
{
	double current = computeEntropy(filePath);
	if (current < 0)
		return;
	std::string name = filePath.string();
	if (previousEntropy.find(name) == previousEntropy.end())
	{
		previousEntropy[name] = current;
		return;
	}
	double delta = current - previousEntropy[name];
	if (delta > ENTROPY_DELTA)
		writeLog("ALERT: entropy spike on " + name
			+ " | delta=" + std::to_string(delta)
			+ " current=" + std::to_string(current) + "/8.0");
	if (current > ENTROPY_ABSOLUTE)
		writeLog("ALERT: high entropy file " + name
			+ " | entropy=" + std::to_string(current) + "/8.0");
	previousEntropy[name] = current;
}

// ── Detection: cryptographic activity ────────────────────────────────────────

int readEntropyAvail()
{
	std::ifstream stream(ENTROPY_POOL_FILE);
	if (!stream.is_open())
	{
		writeLog("WARNING: cannot read " + std::string(ENTROPY_POOL_FILE));
		return -1;
	}
	int value = 0;
	stream >> value;
	return value;
}

void checkCryptoActivity()
{
	int current = readEntropyAvail();
	if (current == -1)
		return;
	if (previousEntropyPool == -1)
	{
		previousEntropyPool = current;
		return;
	}
	int drop = previousEntropyPool - current;
	if (drop > ENTROPY_DROP_THRESHOLD)
		writeLog("ALERT: crypto activity detected"
			+ std::string(" | entropy pool dropped by ") + std::to_string(drop)
			+ " bits (now " + std::to_string(current) + " bits)");
	previousEntropyPool = current;
}

// ── Inotify setup ─────────────────────────────────────────────────────────────

int setupInotify(const std::vector<std::filesystem::path> &paths,
				 std::map<int, std::filesystem::path> &watchMap)
{
	int fd = inotify_init();
	if (fd < 0)
	{
		writeLog("ERROR: inotify_init() failed");
		exit(EXIT_FAILURE);
	}
	size_t i = 0;
	while (i < paths.size())
	{
		int wd = inotify_add_watch(fd, paths[i].c_str(),
					IN_ACCESS | IN_OPEN | IN_MODIFY | IN_CREATE | IN_DELETE);
		if (wd < 0)
			writeLog("WARNING: cannot watch " + paths[i].string());
		else
			watchMap[wd] = paths[i];
		i += 1;
	}
	return fd;
}

// ── Main monitoring loop ──────────────────────────────────────────────────────

void monitorLoop(int inotifyFd, const std::map<int, std::filesystem::path> &watchMap)
{
	char          buffer[4096];
	fd_set        fds;
	struct timeval timeout;
	int           ret;
	int           len;

	while (true)
	{
		FD_ZERO(&fds);
		FD_SET(inotifyFd, &fds);
		timeout.tv_sec  = CRYPTO_CHECK_INTERVAL;
		timeout.tv_usec = 0;

		ret = select(inotifyFd + 1, &fds, nullptr, nullptr, &timeout);

		if (ret < 0)
		{
			writeLog("ERROR: select() failed");
			continue;
		}

		if (ret == 0)
		{
			checkCryptoActivity();
			continue;
		}

		len = read(inotifyFd, buffer, sizeof(buffer));
		if (len < 0)
		{
			writeLog("ERROR: read() on inotify failed");
			continue;
		}

		int i = 0;
		while (i < len)
		{
			struct inotify_event *event = (struct inotify_event *)&buffer[i];
			if (event->len > 0)
			{
				std::string           filename = event->name;
				std::filesystem::path fullPath;
				if (watchMap.count(event->wd))
					fullPath = watchMap.at(event->wd) / filename;
				else
					fullPath = std::filesystem::path(filename);
				if (event->mask & IN_ACCESS)
				{
					writeLog("ACCESS detected: " + fullPath.string());
					checkReadAbuse(fullPath.string());
				}
				if (event->mask & IN_MODIFY)
				{
					writeLog("MODIFY detected: " + fullPath.string());
					checkEntropy(fullPath);
				}
			}
			i = i + (int)sizeof(struct inotify_event) + (int)event->len;
		}
	}
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
	checkForRootPermission();
	setupLog();
	daemonize();
	std::vector<std::filesystem::path>    paths = parsePaths(argc, argv);
	std::map<int, std::filesystem::path>  watchMap;
	int inotifyFd = setupInotify(paths, watchMap);
	writeLog("INFO: irondome started, monitoring " + std::to_string(paths.size()) + " path(s)");
	monitorLoop(inotifyFd, watchMap);
	return EXIT_SUCCESS;
}
