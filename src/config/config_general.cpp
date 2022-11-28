#include "config_general.h"
#include <limits>

namespace GENERAL {

VerboseLevel  							verboseLevel 		= VERBOSELEVEL_NOTSET;
int           							numberOfThreads 	= NOTSET;
time_t  								startDate;
std::chrono::steady_clock::time_point 	startTime 			= std::chrono::steady_clock::now();
int 									timeLimit           = NOTSET;
bool 									usingAPI 			= false;


std::mutex                  tracker_lock;
size_t                      ready_thread_id;
int                         lineCountToFlush;

bool                        initialized = false;


int runTime() {
	return int(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-startTime).count());
}


void setDefaultParametersWhenNecessary() {
    
    MT::MTINIT();

	// Handle verboseLevel
	if (verboseLevel==VERBOSELEVEL_NOTSET) 	verboseLevel 	= MINIMAL;

	// Handle numberOfThreads
	if ( (verboseLevel>ON) && (numberOfThreads != 1) ) {
		if (verboseLevel!=QUITE) std::cout << std::endl << "-numberOfThreads option is ignored and forced to be 1 since verbose level is bigger than 1." << std::endl;
		numberOfThreads = MT::maxNumberOfThreads = 1;
	} else {
		if (numberOfThreads==NOTSET) {
                    numberOfThreads = MT::maxNumberOfThreads;
		}
                else {
                    MT::maxNumberOfThreads = numberOfThreads;
                }
	}

	if (timeLimit<=0) {
		if (verboseLevel!=QUITE) std::cout << "Setting time limit to infinite" << std::endl;
		timeLimit = MAXTIMELIMIT; // no time limit by default
	}

	initialized = true;
	
}


void print() {
	std::cout << std::endl;
    if (GENERAL::usingAPI==false) {
        std::cout << "GENERAL OPTIONS"<< std::endl;
        std::cout << "verbose level        : "  << verboseLevel 		<< std::endl;
    }
	std::cout << "number of threads    : "  << numberOfThreads 	<< std::endl;
	std::cout << "time limit           : ";
	if (timeLimit==MAXTIMELIMIT)
		std::cout << "infinite minutes";
	else {
		float tmp = float(timeLimit)/60.0;
		std::cout << tmp;
		if (tmp<=1)
			std::cout << " minute" << std::endl;
		else
			std::cout << " minutes" << std::endl;
	}
	if (GENERAL::usingAPI==false) {
        std::cout << std::endl;
    }

}

}

