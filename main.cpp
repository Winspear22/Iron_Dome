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
#include <sys/inotify.h>

void checkForRootPermission()
{
	uid_t rootStatus;
    rootStatus = geteuid();
	if (rootStatus != 0)
	{
		std::cerr << RED_BOLD << "Error, the processus was not ran as root." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE) ;
	}
}

void daemonize()
{
	pid_t pid;
	pid_t pid2;
	int fd;

	pid = fork();
	if (pid < 0)
	{
		std::cerr << RED_BOLD << "Error, fork() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE) ;
	}
	else if (pid > 0)
	{
		std::cout << GREEN_BOLD << "Success ! Daemon launched, PID: " << getpid() << "." << RESET_BOLD << std::endl;
		exit(EXIT_SUCCESS) ;
	}
	if (setsid() < 0)
	{
		std::cerr << RED_BOLD << "Error, setsid() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE) ;
	}
	pid2 = fork();
	if (pid2 < 0)
	{
		std::cerr << RED_BOLD << "Error, fork() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE) ;
	}
	else if (pid2 > 0)
	{
		std::cout << GREEN_BOLD << "Success ! Daemon launched, PID: " << getpid() << "." << RESET_BOLD << std::endl;
		exit(EXIT_SUCCESS) ;
	}
	umask(0);
	chdir("/");
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	fd = open("/dev/null", O_RDWR);
	if (fd < 0)
	{
		//std::cerr << RED_BOLD << "Error, open() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE);
	}
	dup(fd);
	dup(fd);
	close(fd);
}

void setupLog()
{
	std::string logDir = "/var/log/irondome/";
	std::string logFile = "/var/log/irondome/irondome.log";
	if (std::filesystem::exists(logDir) == FAILURE)
		std::filesystem::create_directories(logDir);
	std::ofstream stream(logFile, std::ios::app);
	if (stream.is_open() == FAILURE)
	{
		std::cerr << RED_BOLD << "Error, open() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE);
	}
	stream.close();                                                                                                                                                                                                                                                          
}

std::vector<std::filesystem::path> parsePaths(int argc, char **argv)
{
	std::vector<std::filesystem::path>			paths;
	size_t										i;
	if (argc == 1)
	{
		paths.push_back("/home");
		paths.push_back("/etc");
		paths.push_back("/tmp");
		return paths;
	}
	i = 1;
	while (i < argc)
	{
		std::filesystem::path p(argv[i]);
		if (std::filesystem::exists(p) == SUCCESS)
			paths.push_back(p);
		else
			std::cerr << RED_BOLD << "Error, path " << p << " does not exist. Path skipped." << RESET_BOLD << std::endl;
		i = i + 1;
	}
	if (paths.empty() == SUCCESS)
	{
		std::cerr << RED_BOLD << "Error, no valid paths provided." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE);
	}
	return paths;
}

void writeLog(const std::string &message)
{
	std::string logFile = "/var/log/irondome/irondome.log";
	std::ofstream stream(logFile, std::ios::app);

	if (stream.is_open() == FAILURE)
	{
		std::cerr << RED_BOLD << "Error, open() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE);
	}
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	stream << std::ctime(&time);   // timestamp
    stream << " | " << message << "\n";
	stream.close();
}                                                                                                                                                                                                       

int setupInotify(const std::vector<std::filesystem::path> &paths)
{
	int fd;
	size_t i;

	fd = inotify_init();
	if (fd < 0)
	{
		writeLog("ERROR: inotify_init() failed");
		exit(EXIT_FAILURE);
	}
	i = 0;
	while (i < paths.size())
	{
		int wd = inotify_add_watch(fd, paths[i].c_str(), IN_ACCESS | IN_OPEN | IN_MODIFY | IN_CREATE | IN_DELETE);
		if (wd < 0)
			writeLog("WARNING: cannot watch " + paths[i].string());
		i = i + 1;
	}
	return fd;
}

void monitorLoop(int inotifyFd)
{
    char buffer[4096];
    int len;
    
    // Boucle infinie englober TOUT
    while (true)
    {
        len = read(inotifyFd, buffer, sizeof(buffer));
        
        if (len < 0)
        {
            writeLog("ERROR: read() failed");
            continue;
        }

        int i = 0;
        // Boucle lecture événements DOIT être dedans
        while (i < len)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            // event->len dire si fichier a un nom. event->name avoir le nom.
            if (event->len > 0)
            {
                std::string filename = event->name;

                // Utiliser & (ET bit à bit) pour vérifier masque
                if (event->mask & IN_ACCESS)
                {
                    writeLog("ACCESS detected: " + filename);
                    // Toi faire checkReadAbuse() plus tard
                }

                if (event->mask & IN_MODIFY)
                {
                    writeLog("MODIFY detected: " + filename);
                    // Toi faire checkEntropy() plus tard
                }
            }

            // Avancer au prochain événement
            i = i + sizeof(struct inotify_event) + event->len;
        }
    }
}

int main(int argc, char **argv)
{
	checkForRootPermission();
	daemonize();
	setupLog();
	std::vector<std::filesystem::path> paths = parsePaths(argc, argv);
    return EXIT_SUCCESS;
}
