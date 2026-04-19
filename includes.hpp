/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   includes.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: adnen <adnen@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/19 18:19:11 by adnen             #+#    #+#             */
/*   Updated: 2026/04/19 20:25:21 by adnen            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef INCLUDES_HPP
# define INCLUDES_HPP

#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>              
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <vector>

# define EXIT_SUCCESS 0
# define EXIT_FAILURE 1

# define SUCCESS true
# define FAILURE false

# define RED "\033[0;31m"
# define GREEN "\033[0;32m"
# define YELLOW "\033[0;33m"
# define BLUE "\033[0;34m"
# define MAGENTA "\033[0;35m"
# define CYAN "\033[0;36m"
# define RESET "\033[0m"

# define RED_BOLD "\033[1;31m"
# define GREEN_BOLD "\033[1;32m"
# define YELLOW_BOLD "\033[1;33m"
# define BLUE_BOLD "\033[1;34m"
# define MAGENTA_BOLD "\033[1;35m"
# define CYAN_BOLD "\033[1;36m"
# define RESET_BOLD "\033[1;0m"

#endif