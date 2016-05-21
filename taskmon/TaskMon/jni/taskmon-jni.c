/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <jni.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/unistd.h>
#include <dirent.h>
#include "ps.h"

/* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   apps/samples/hello-jni/project/src/com/example/taskmon/JniWrapper.java
 */
#define BUFSIZE 8096
#define ARRAYSIZE 1000
extern char **environ;

int bufreadline(char* srcbuf, int srcsize, char* destbuf, int destsize){
    int i;

    for (i = 0; i < srcsize && i < destsize; ++i) {
        char c;
        c = srcbuf[i];
        destbuf[i] = c;
        if (c == '\n') {
            i++;
            break;
        }
        else if (c == '\0') {
            break;
        }
    }

    return i;
}

JNIEXPORT jstring JNICALL
Java_com_example_taskmon_JniWrapper_stringFromJNI( JNIEnv* env,jobject thiz )
{
    return (*env)->NewStringUTF(env, "Hello from JNI !");
}

JNIEXPORT jintArray JNICALL
Java_com_example_taskmon_JniWrapper_getRelTID(JNIEnv* env,jobject thiz){
	jintArray tidlist;
	int n, readcount = 0, count = 0;
    int psdev;
    int fill[ARRAYSIZE];

    char buf[BUFSIZE];
    char readline[BUFSIZE];

    memset(buf, 0, BUFSIZE);
    memset(readline, 0, BUFSIZE);
    memset(fill, 0, sizeof(int) * ARRAYSIZE);

    syscall(__NR_count_rt_threads);

    if ((psdev = open("/dev/psdev", O_RDONLY)) == -1){
    	return NULL;
    }

    if((n = read(psdev, buf, BUFSIZE)) == 0){
    	return NULL;
    }

    while(readcount < n){
    	int tid, pid, prio;
    	char name[BUFSIZE];
    	readcount += bufreadline(buf+readcount, BUFSIZE, readline, BUFSIZE);

    	sscanf(readline, "%d %d %d %s", &tid, &pid, &prio, name);

    	fill[count] = tid;
    	count++;
    }



    tidlist = (*env)->NewIntArray(env, count);

    if(tidlist == NULL) {
		return NULL;
	}

    (*env)->SetIntArrayRegion(env, tidlist, 0, count, fill);
    close(psdev);

    return tidlist;
}

JNIEXPORT jintArray JNICALL
Java_com_example_taskmon_JniWrapper_getReservedTID(JNIEnv* env,jobject thiz){
	jintArray tidlist;
	int tidarray[1000], count = 0;

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir ("/sys/rtes/tasks/")) != NULL) {
	  while ((ent = readdir (dir)) != NULL) {
		  int tid = atoi(ent->d_name);
		  if(tid > 0){
			  tidarray[count] = tid;
		  }
	    count++;
	  }
	  closedir (dir);
	} else {
	  return NULL;
	}
	tidlist = (*env)->NewIntArray(env, count);

	if(tidlist == NULL) {
		return NULL;
	}

	(*env)->SetIntArrayRegion(env, tidlist, 0, count, tidarray);

	return tidlist;

}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_setReserve(JNIEnv* env,jobject thiz, jint tidarg, jint Carg, jint Targ, jint cpuIDarg){

	int c, t, cpuID, tid, ret;
	c = Carg;
	t = Targ;
	tid = tidarg;
	cpuID = cpuIDarg;
	struct timespec C, T;
	C.tv_sec = c/1000;
	C.tv_nsec = (c%1000)*1000000;
	T.tv_sec = t/1000;
	T.tv_nsec = (t%1000)*1000000;

	if (c > t) {
		return -1;
	}
	if (cpuID < 0 || cpuID > 3) {
		return -1;
	}
	if ((ret = syscall(__NR_set_reserve, (pid_t)tid, &C, &T, cpuID)) != 0) {
		return ret;
	}

	return 0;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_cancelReserve(JNIEnv* env,jobject thiz, jint tid){
	if(syscall(__NR_cancel_reserve, (pid_t)tid)){
		return -1;
	}

	return 0;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_taskmonEnable(JNIEnv* env,jobject thiz){
	int taskmon;

	if((taskmon = open("/sys/rtes/taskmon/enabled", O_RDWR)) == -1){
		return -2;
	}

	write(taskmon, "1", 1);

	close(taskmon);
	return 0;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_getFreq(JNIEnv* env,jobject thiz){
	int freqf, freq;
	char buf[BUFSIZE];

	memset(buf, 0, BUFSIZE);

	if((freqf = open("/sys/rtes/freq", O_RDONLY)) == -1){
		return -2;
	}

	if(read(freqf, buf, BUFSIZE) == 0){
	    	return -1;
	}

	if(sscanf(buf, "%d", &freq) != 1){
		return -1;
	}

	close(freqf);
	return freq;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_getPower(JNIEnv* env,jobject thiz){
	int powerf, power;
	char buf[BUFSIZE];

	memset(buf, 0, BUFSIZE);

	if((powerf = open("/sys/rtes/power", O_RDONLY)) == -1){
		return -3;
	}

	if(read(powerf, buf, BUFSIZE) == 0){
	    	return -2;
	}

	if(sscanf(buf, "%d", &power) != 1){
		return -1;
	}

	close(powerf);
	return power;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_getEnergy(JNIEnv* env,jobject thiz){
	int energyf, energy;
	char buf[BUFSIZE];

	memset(buf, 0, BUFSIZE);

	if((energyf = open("/sys/rtes/energy", O_RDONLY)) == -1){
		return -3;
	}

	if(read(energyf, buf, BUFSIZE) == 0){
	    	return -2;
	}

	if(sscanf(buf, "%d", &energy) != 1){
		return -1;
	}

	close(energyf);
	return energy;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_taskmonDisable(JNIEnv* env,jobject thiz){
	int taskmon;

	if((taskmon = open("/sys/rtes/taskmon/enabled", O_RDWR)) == -1){
		return -1;
	}

	write(taskmon, "0", 1);
	close(taskmon);

	return 0;
}

JNIEXPORT jstring JNICALL
Java_com_example_taskmon_JniWrapper_getUtil(JNIEnv* env,jobject thiz, jint tid){
	int threadfd;

	char threadpath[BUFSIZE];
	char buf[BUFSIZE];
	memset(threadpath, 0, BUFSIZE);
	memset(buf, 0, BUFSIZE);

	sprintf(threadpath, "/sys/rtes/taskmon/util/%d", tid);

	if((threadfd = open(threadpath, O_RDONLY)) == -1){
		return NULL;
	}

	if(read(threadfd, buf, BUFSIZE) == 0){
	    	return NULL;
	}

	jstring jstrBuf = (*env)->NewStringUTF(env, buf);

	return jstrBuf;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_getTidEnergy(JNIEnv* env,jobject thiz, jint tid){
	int threadfd, energy;

	char threadpath[BUFSIZE];
	char buf[BUFSIZE];
	memset(threadpath, 0, BUFSIZE);
	memset(buf, 0, BUFSIZE);

	sprintf(threadpath, "/sys/rtes/tasks/%d/energy", tid);

	if((threadfd = open(threadpath, O_RDONLY)) == -1){
		return -1;
	}

	if(read(threadfd, buf, BUFSIZE) == 0){
	    	return -1;
	}

	if(sscanf(buf, "%d", &energy) != 1){
			return -1;
		}

	close(threadfd);
	return energy;
}

