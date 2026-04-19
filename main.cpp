/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: adnen <adnen@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/19 18:18:53 by adnen             #+#    #+#             */
/*   Updated: 2026/04/19 19:41:59 by adnen            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "includes.hpp"
#include <cstdlib>
#include <unistd.h>

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
		std::cout << GREEN_BOLD << "Success ! You are now a daemon." << RESET_BOLD << std::endl;
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
		std::cout << GREEN_BOLD << "Success ! You are now a daemon." << RESET_BOLD << std::endl;
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
		std::cerr << RED_BOLD << "Error, open() failure." << RESET_BOLD << std::endl;
		exit(EXIT_FAILURE);
	}
	dup(fd);
	dup(fd);
}

int main(int argc, char **argv)
{

    return EXIT_SUCCESS;
}
