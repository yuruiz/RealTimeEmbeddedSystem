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

		int count = syscall(__NR_count_rt_threads);

		if(count == 0){
			return NULL;
		}

		int i = 0;
		struct proc_struct* rt_proc_list = malloc(sizeof(struct proc_struct) * count);
		int* fill = malloc(sizeof(int)*count);

		syscall(__NR_list_rt_threads, rt_proc_list, count);

		for(i = 0; i < count; i++){
			fill[i] = (int)rt_proc_list[i].pid;
		}

	    tidlist = (*env)->NewIntArray(env, count);

	    if(tidlist == NULL) {
			return NULL;
		}

	    (*env)->SetIntArrayRegion(env, tidlist, 0, count, fill);
	    free(rt_proc_list);

	    return NULL;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_setReserve(JNIEnv* env,jobject thiz, jint tid, jint C, jint T, jint cpuID){
	int pid;
	char* argv[7];
	char tidstr[100];
	char cstr[100];
	char tstr[100];
	char cpustr[100];
	char* command = "/data/reserve";
	char* set = "set";

	memset(tidstr, 0, 100);
	memset(cstr, 0, 100);
	memset(tstr, 0, 100);
	memset(cpustr, 0, 100);

	sprintf(tidstr, "%d", tid);
	sprintf(cstr, "%d", C);
	sprintf(tstr, "%d", T);
	sprintf(cpustr, "%d", cpuID);

	argv[0] = command;
	argv[1] = set;
	argv[2] = tidstr;
	argv[3] = cstr;
	argv[4] = tstr;
	argv[5] = cpustr;
	argv[6] = 0;

	pid = fork();

	if(pid == -1){
		return -1;
	}

	/*Child process*/
	if(pid == 0){
		chdir("/");
		execve(argv[0], argv, environ);
	}

	return 0;
}

JNIEXPORT void JNICALL
Java_com_example_taskmon_JniWrapper_cancelReserve(JNIEnv* env,jobject thiz, jint tid){
	int pid;
	char* argv[4];
	char tidstr[100];
	char* command = "/data/reserve";
	char* cancel = "cancel";

	memset(tidstr, 0, 100);

	sprintf(tidstr, "%d", tid);

	argv[0] = command;
	argv[1] = cancel;
	argv[2] = tidstr;
	argv[3] = 0;

	pid = fork();

	/*Child process*/
	if(pid == 0){
		chdir("/");
		execve(argv[0], argv, environ);
	}
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_taskmonEnable(JNIEnv* env,jobject thiz){
	int taskmon;

	if((taskmon = open("/sys/rtes/taskmon/enabled", O_RDWR)) == -1){
		return -1;
	}

	if(write(taskmon, "1", 1) != 1){
		return -1;
	}

	close(taskmon);
	return 0;
}

JNIEXPORT int JNICALL
Java_com_example_taskmon_JniWrapper_taskmonDisable(JNIEnv* env,jobject thiz){
	int taskmon;

	if((taskmon = open("/sys/rtes/taskmon/enabled", O_RDWR)) == -1){
		return -1;
	}

	if(write(taskmon, "0", 1) != 1){
		return -1;
	}
	close(taskmon);

	return 0;
}

JNIEXPORT jfloatArray JNICALL
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


