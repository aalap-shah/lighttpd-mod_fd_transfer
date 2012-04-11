/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#ifndef STRATUS_LOGGING_H
#define STRATUS_LOGGING_H

//#define STRATUS_LOG_FILE "/home/rpf468/stratus.log"
#define STRATUS_LOG_FILE "/userdata/var/log/stratus.log"

#ifdef LOGGING_FILE
#define STRATUS_LOG_V(msg, ...)\
{\
	fp = fopen(STRATUS_LOG_FILE, "a+");\
	if(fp){\
		fprintf(fp, "%d:%s:%s:%d:", getpid(), __FILE__, __FUNCTION__, __LINE__); \
		fprintf(fp, msg, __VA_ARGS__);\
	}\
	fclose(fp);\
}

#define STRATUS_LOG(msg)\
{\
	fp = fopen(STRATUS_LOG_FILE, "a+");\
	if(fp){\
		fprintf(fp, "%d:%s:%s:%d:%s\n", getpid(), __FILE__, __FUNCTION__, __LINE__, msg); \
	}\
	fclose(fp);\
}
#endif

#ifdef LOGGING_STDOUT
#define STRATUS_LOG_V(msg, ...)\
{\
	printf("%d:%s:%s:%d:", getpid(), __FILE__, __FUNCTION__, __LINE__); \
	printf(msg, __VA_ARGS__);\
}

#define STRATUS_LOG(msg)\
{\
	printf("%d:%s:%s:%d:%s\n", getpid(), __FILE__, __FUNCTION__, __LINE__, msg); \
}
#endif

#ifdef LOGGING_NONE

#define STRATUS_LOG_V(msg, ...) {};

#define STRATUS_LOG(msg) {};

#endif

#endif //STRATUS_LOGGING_H
