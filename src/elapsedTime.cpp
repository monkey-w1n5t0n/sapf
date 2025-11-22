//    SAPF - Sound As Pure Form
//    Copyright (C) 2019 James McCartney
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "elapsedTime.hpp"

#ifdef __APPLE__
#include <mach/mach_time.h>
#else
#include <chrono>
#endif

#include <pthread.h>

extern "C" {

#ifdef __APPLE__
static double gHostClockFreq;

void initElapsedTime()
{
	struct mach_timebase_info info;
	mach_timebase_info(&info);
	gHostClockFreq = 1e9 * ((double)info.numer / (double)info.denom);
}

double elapsedTime()
{
	return (double)mach_absolute_time() / gHostClockFreq;
}
#else

static std::chrono::steady_clock::time_point gStartTime;

void initElapsedTime()
{
    gStartTime = std::chrono::steady_clock::now();
}

double elapsedTime()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = now - gStartTime;
    return diff.count();
}
#endif

}
